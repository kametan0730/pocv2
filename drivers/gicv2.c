/*
 *  GICv2 (generic interrupt controller) driver
 */

#include "aarch64.h"
#include "gic.h"
#include "gicv2.h"
#include "log.h"
#include "param.h"
#include "irq.h"
#include "vcpu.h"
#include "pcpu.h"
#include "localnode.h"
#include "compiler.h"
#include "panic.h"
#include "vgic-v2.h"

static struct gic_irqchip gicv2_irqchip;

static void *gicc_base;
static void *gicd_base;
static void *gich_base;
static void *gicv_base;

static inline u32 gicd_read(u32 offset) {
  return *(volatile u32 *)((u64)gicd_base + offset);
}

static inline void gicd_write(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gicd_base + offset) = val;
}

static inline u32 gicc_read(u32 offset) {
  return *(volatile u32 *)((u64)gicc_base + offset);
}

static inline void gicc_write(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gicc_base + offset) = val;
}

static inline u32 gich_read(u32 offset) {
  return *(volatile u32 *)((u64)gich_base + offset);
}

static inline void gich_write(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gich_base + offset) = val;
}

static inline u32 gicv_read(u32 offset) {
  return *(volatile u32 *)((u64)gicv_base + offset);
}

static inline void gicv_write(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gicv_base + offset) = val;
}

static u32 gicv2_read_lr(int n) {
  if(n > gicv2_irqchip.max_lr)
    panic("lr");

  return gich_read(GICH_LR(n));
}

static void gicv2_write_lr(int n, u32 val) {
  if(n > gicv2_irqchip.max_lr)
    panic("lr");

  gich_write(GICH_LR(n), val);
}

static inline u8 lr_priority(u8 prio) {
  return (prio >> 4) & 0xf;
}

static u32 gicv2_pending_lr(struct gic_pending_irq *irq) {
  u32 lr = irq->virq & 0x3ff;

  lr |= LR_PENDING << GICH_LR_State_SHIFT;

  if(irq->group == 1)
    lr |= GICH_LR_Grp1;

  lr |= lr_priority(irq->priority) << GICH_LR_Priority_SHIFT;

  if(irq->pirq) {
    /* this is hw irq */
    lr |= GICH_LR_HW;
    lr |= (irq_no(irq->pirq) & 0x3ff) << GICH_LR_PID_SHIFT;
  } else if(is_sgi(irq->virq)) {
    lr |= (irq->req_cpu & 0x7) << GICH_LR_CPUID_SHIFT;
  }

  return lr;
}

static bool gicv2_guest_irq_pending(u32 virq) {
  for(int i = 0; i <= gicv2_irqchip.max_lr; i++) {
    u64 lr = gicv2_read_lr(i);

    if((lr & 0x3ff) == virq) {
      if(lr & (LR_PENDING << GICH_LR_State_SHIFT))
        return true;
      else
        return false;
    }
  }

  return false;
}

static int gicv2_inject_guest_irq(struct gic_pending_irq *irq) {
  u32 virq = irq->virq;

  if(virq == 2)
    panic("!? maybe Linux kernel panicked");

  u32 elsr0 = gich_read(GICH_ELSR0);
  u32 elsr1 = gich_read(GICH_ELSR1);
  u64 elsr = ((u64)elsr1 << 32) | elsr0;
  int freelr = -1;
  u64 lr;

  for(int i = 0; i < gicv2_irqchip.max_lr; i++) {
    if((elsr >> i) & 0x1) {
      if(freelr < 0)
        freelr = i;

      continue;
    }

    if(((gicv2_read_lr(i) >> GICH_LR_PID_SHIFT) & 0x3ff) == virq)
      return -1;    // busy
  }

  if(freelr < 0)
    return -1;    // no entry

  lr = gicv2_pending_lr(irq);

  gicv2_write_lr(freelr, lr);

  return 0;
}

static u32 gicv2_read_iar() {
  return gicc_read(GICC_IAR);
}

static void gicv2_eoi(u32 iar) {
  gicc_write(GICC_EOIR, iar);
}

static void gicv2_deactive_irq(u32 irq) {
  gicc_write(GICC_DIR, irq);
}

static void gicv2_host_eoi(u32 iar) {
  gicv2_eoi(iar);
  gicv2_deactive_irq(iar & 0x3ff);
}

static void gicv2_guest_eoi(u32 iar) {
  gicv2_eoi(iar);
}

static void gicv2_send_sgi(struct gic_sgi *sgi) {
  u32 sgir = (sgi->mode << GICD_SGIR_TargetListFilter_SHIFT) |
             ((sgi->targets & 0xff) << GICD_SGIR_TargetList_SHIFT) |
             (sgi->sgi_id & 0xf);

  dsb(ish);

  gicd_write(GICD_SGIR, sgir);
}

static bool gicv2_irq_pending(u32 irq) {
  u32 is = gicd_read(GICD_ISPENDR(irq / 32));
  return !!(is & (1u << (irq % 32)));
}

static bool gicv2_irq_enabled(u32 irq) {
  u32 is = gicd_read(GICD_ISENABLER(irq / 32));
  return !!(is & (1u << (irq % 32)));
}

static void gicv2_enable_irq(u32 irq) {
  u32 is = gicd_read(GICD_ISENABLER(irq / 32));
  is |= 1u << (irq % 32);
  gicd_write(GICD_ISENABLER(irq / 32), is);
}

static void gicv2_disable_irq(u32 irq) {
  u32 is = 1u << (irq % 32);
  gicd_write(GICD_ICENABLER(irq / 32), is);
}

static u32 gicv2_get_target(u32 irq) {
  u32 itargetsr = gicd_read(GICD_ITARGETSR(irq / 4));
  u32 targets = (itargetsr >> (irq % 4 * 8)) & 0xff;

  return targets;
}

static void gicv2_set_targets(u32 irq, u8 targets) {
  if(is_sgi_ppi(irq)) {
    vmm_warn("sgi_ppi set target?");
    return;
  }

  u32 itargetsr = gicd_read(GICD_ITARGETSR(irq / 4));
  itargetsr &= ~((u32)0xff << (irq % 4 * 8));
  gicd_write(GICD_ITARGETSR(irq / 4), itargetsr | (targets << (irq % 4 * 8)));
}

static void gicv2_setup_irq(u32 irq) {
  if(is_spi(irq)) {
    gicv2_set_targets(irq, 1 << 0);    // route to 0
  }

  gicv2_enable_irq(irq);
}

static void gicv2_irq_handler(int from_guest) {
  do {
    u32 iar = gicv2_read_iar();
    u32 irq = iar & 0x3ff;

    if(irq == 1023)    /* spurious interrupt */
      break;

    if(is_ppi_spi(irq)) {
      isb();

      local_irq_enable();

      int handled = handle_irq(irq);

      local_irq_disable();

      if(handled)
        gicv2_host_eoi(iar);
    } else if(is_sgi(irq)) {
      cpu_sgi_handler(irq);

      gicv2_host_eoi(iar);
    } else {
      panic("??????? %d", irq);
    }
  } while(1);
}

static void gicv2_h_init() {
  u32 vtr = gich_read(GICH_VTR);

  gicv2_irqchip.max_lr = vtr & 0x3f;

  // gich_write(GICH_VMCR, GICH_VMCR_VMG0En);
  gich_write(GICH_HCR, GICH_HCR_EN);
}

static void gicv2_c_init() {
  gicd_write(GICD_ICACTIVER(0), 0xffffffff);
  /* disable PPI */
  gicd_write(GICD_ICENABLER(0), 0xffff0000);
  /* enable SGI */
  gicd_write(GICD_ISENABLER(0), 0x0000ffff);

  gicc_write(GICC_PMR, 0xff);
  gicc_write(GICC_BPR, 0x0);

  gicc_write(GICC_CTLR, GICC_CTLR_EnableGrp0 | GICC_CTLR_EOImode);
}

static void gicv2_d_init() {
  gicd_write(GICD_CTLR, 0);

  u32 lines = gicd_read(GICD_TYPER) & 0x1f;
  u32 nirqs = 32 * (lines + 1);

  gicv2_irqchip.nirqs = nirqs < 1020 ? nirqs : 1020;

  /* all interrupts are group 0 */
  for(int i = 0; i < nirqs; i += 4)
    gicd_write(GICD_IGROUPR(i / 4), 0);

  gicd_write(GICD_CTLR, GICD_CTLR_EnableGrp0);

  isb();
}

static void gicv2_init_cpu(void) {
  gicv2_c_init();
  gicv2_h_init();
}

static int gicv2_dt_init(struct device_node *dev) {
  u64 dbase, dsize, cbase, csize, hbase, hsize, vbase, vsize;

  if(dt_node_prop_addr(dev, 0, &dbase, &dsize) < 0)
    return -1;

  if(dt_node_prop_addr(dev, 1, &cbase, &csize) < 0)
    return -1;

  if(dt_node_prop_addr(dev, 2, &hbase, &hsize) < 0)
    return -1;

  if(dt_node_prop_addr(dev, 3, &vbase, &vsize) < 0)
    return -1;

  gicd_base = iomap(dbase, dsize);
  if(!gicd_base)
    return -1;

  gicc_base = iomap(cbase, csize);
  if(!gicc_base)
    return -1;

  gich_base = iomap(hbase, hsize);
  if(!gich_base)
    return -1;

  gicv_base = iomap(vbase, vsize);
  if(!gicv_base)
    return -1;

  gicv2_d_init();
  gicv2_h_init();

  vgic_v2_pre_init(vbase);

  printf("GICv2: nirqs: %d max_lr: %d\n", gicv2_irqchip.nirqs, gicv2_irqchip.max_lr);

  printf("GICv2: dist base %p\n"
         "        cpu base %p\n"
         "        hyp base %p\n"
         "       virt base %p\n", gicd_base, gicc_base, gich_base, gicv_base);

  localnode.irqchip = &gicv2_irqchip;

  return 0;
}

static struct gic_irqchip gicv2_irqchip = {
  .version = 2,

  .initcore           = gicv2_init_cpu,
  .inject_guest_irq   = gicv2_inject_guest_irq,
  .irq_pending        = gicv2_irq_pending,
  .guest_irq_pending  = gicv2_guest_irq_pending,
  .host_eoi           = gicv2_host_eoi,
  .guest_eoi          = gicv2_guest_eoi,
  .deactive_irq       = gicv2_deactive_irq,
  .send_sgi           = gicv2_send_sgi,
  .irq_enabled        = gicv2_irq_enabled,
  .enable_irq         = gicv2_enable_irq,
  .disable_irq        = gicv2_disable_irq,
  .setup_irq          = gicv2_setup_irq,
  .set_targets        = gicv2_set_targets,
  .route_irq          = NULL,
  .irq_handler        = gicv2_irq_handler,
};

static struct dt_compatible gicv2_compat[] = {
  { "arm,gic-400" },
  { "arm,cortex-a15-gic" },
  {},
};

DT_IRQCHIP_INIT(gicv2, gicv2_compat, gicv2_dt_init);
