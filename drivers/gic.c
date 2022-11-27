#include "types.h"
#include "vcpu.h"
#include "localnode.h"
#include "gic.h"
#include "log.h"
#include "panic.h"

static void gic_inject_pending_irqs() {
  struct vcpu *vcpu = current;

  int head = vcpu->pending.head;

  while(head != vcpu->pending.tail) {
    u32 irq = vcpu->pending.irqs[head];
    localnode.irqchip->inject_guest_irq(irq, 1);

    head = (head + 1) % 4;
  }

  vcpu->pending.head = head;

  dsb(ish);
}

static void gic_sgi_handler(enum gic_sgi sgi_id) {
  switch(sgi_id) {
    case SGI_INJECT:  /* inject guest pending interrupt */
      gic_inject_pending_irqs();
      break;
    default:
      panic("unknown sgi %d", sgi_id);
  }
}

void irqchip_check(struct gic_irqchip *irqchip) {
  int version = irqchip->version;

  if(version != 2 && version != 3)
    panic("GIC?");

  printf("irqchip: GICv%d detected\n", version);

  bool all_implemented = true;

  all_implemented &= !!(irqchip->init);
  all_implemented &= !!(irqchip->initcore);
  all_implemented &= !!(irqchip->read_lr);
  all_implemented &= !!(irqchip->write_lr);
  all_implemented &= !!(irqchip->inject_guest_irq);
  all_implemented &= !!(irqchip->irq_pending);
  all_implemented &= !!(irqchip->read_iar);
  all_implemented &= !!(irqchip->host_eoi);
  all_implemented &= !!(irqchip->guest_eoi);
  all_implemented &= !!(irqchip->deactive_irq);
  all_implemented &= !!(irqchip->send_sgi);
  all_implemented &= !!(irqchip->irq_enabled);
  all_implemented &= !!(irqchip->enable_irq);
  all_implemented &= !!(irqchip->disable_irq);

  if(!all_implemented)
    panic("irqchip: features incomplete");
}

void gic_irq_handler(int from_guest) {
  while(1) {
    u32 iar = localnode.irqchip->read_iar();
    u32 pirq = iar & 0x3ff;

    if(pirq == 1023)    /* spurious interrupt */
      break;

    if(is_ppi_spi(pirq)) {
      isb();

      int handled = handle_irq(pirq);

      if(handled)
        localnode.irqchip->host_eoi(pirq, 1);
    } else if(is_sgi(pirq)) {
      gic_sgi_handler(pirq);

      localnode.irqchip->host_eoi(pirq, 1);
    } else {
      panic("???????");
    }
  }
}

void gic_init_cpu() {
  localnode.irqchip->initcore();
}

void gic_init() {
  if(!localnode.irqchip)
    panic("no irqchip");

  irqchip_check(localnode.irqchip);

  localnode.irqchip->init();
}
