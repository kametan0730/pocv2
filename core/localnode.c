/*
 *  localnode
 */

#include "types.h"
#include "node.h"
#include "s2mm.h"
#include "param.h"

struct localnode localnode;    /* me */

void localvm_init(int nvcpu, u64 nalloc, struct guest *guest_fdt) {
  vmm_log("node n vCPU: %d total RAM: %p byte\n", nvcpu, nalloc);

  localvm.nvcpu = nvcpu;
  localvm.nalloc = nalloc;

  if(localvm.nalloc != MEM_PER_NODE)
    panic("localvm.nalloc != NR_CACHE_PAGES %p", MEM_PER_NODE);

  localvm.pmap = NULL;
  spinlock_init(&localvm.lock);

  /* TODO: determines vm's device info from fdt file */
  (void)guest_fdt;

  s2mmu_init();
  s2mmu_init_core();

  map_guest_peripherals(localvm.vttbr);

  vgic_init();
}
