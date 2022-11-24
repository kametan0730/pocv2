/*
 *  vmm initialization sequeunce
 */

#include "uart.h"
#include "aarch64.h"
#include "guest.h"
#include "pcpu.h"
#include "allocpage.h"
#include "mm.h"
#include "log.h"
#include "vgic.h"
#include "vtimer.h"
#include "pci.h"
#include "log.h"
#include "psci.h"
#include "node.h"
#include "virtio-mmio.h"
#include "malloc.h"
#include "arch-timer.h"
#include "power.h"
#include "panic.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

extern char _binary_virt_dtb_start[];
extern char _binary_virt_dtb_size[];

struct guest virt_dtb = {
  .name = "virt dtb",
  .start = (u64)_binary_virt_dtb_start,
  .size = (u64)_binary_virt_dtb_size,
};

void _start(void);
void vectable();

volatile static int cpu0_ready = 0;

static void hcr_setup() {
  u64 hcr = HCR_VM | HCR_SWIO | HCR_FMO | HCR_IMO |
            HCR_RW | HCR_TSC | HCR_TDZ;

  write_sysreg(hcr_el2, hcr);

  isb();
}

int vmm_init_secondary() {
  vmm_log("cpu%d activated...\n", cpuid());
  write_sysreg(vbar_el2, (u64)vectable);
  pcpu_init();
  vcpu_init_core();
  gic_init_cpu();
  s2mmu_init();
  hcr_setup();
  arch_timer_init_core();
  write_sysreg(vttbr_el2, localnode.vttbr);

  localnode.ctl->startcore();

  panic("unreachable");
}

int vmm_init_cpu0() {
  uart_init();
  printf("vmm booting...\n");
  write_sysreg(vbar_el2, (u64)vectable);
  pcpu_init();
  vcpu_init_core();
  pageallocator_init();
  malloc_init();
  virtio_mmio_init();
  // pci_init();
  gic_init();
  gic_init_cpu();
  arch_timer_init();
  arch_timer_init_core();
  vtimer_init();
  s2mmu_init();
  powerctl_init();
  hcr_setup();

  nodectl_init();

  localnode_preinit(1, 256 * MiB, &virt_dtb);

  localnode.ctl->init();
  localnode.ctl->startcore();

  panic("unreachable");
}
