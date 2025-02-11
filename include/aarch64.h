#ifndef CORE_AARCH64_H
#define CORE_AARCH64_H

#define SCTLR_M   (1 << 0)
#define SCTLR_A   (1 << 1)
#define SCTLR_C   (1 << 2)
#define SCTLR_I   (1 << 12)

#define SCR_NS    (1 << 0)
#define SCR_SMD   (1 << 7)
#define SCR_HCE   (1 << 8)
#define SCR_RW    (1 << 10)

#define SCR_RES1  ((1 << 4) | (1 << 5))

#ifdef __ASSEMBLER__

#define INTR_DISABLE    msr daifset, #0xf
#define IRQ_ENABLE      msr daifclr, #0x2

#else /* !__ASSEMBLER__ */

#include "types.h"
#include "compiler.h"

#define arm_sysreg(op1, crn, crm, op2)  \
  s3_ ## op1 ## _ ## crn ## _ ## crm ## _ ## op2

#define HFGITR_EL2    arm_sysreg(4, c1, c1, 6)

#define __read_sysreg(reg) \
  ({u64 val; asm volatile("mrs %0, " #reg : "=r"(val)); val;})
#define read_sysreg(reg)  __read_sysreg(reg)

#define __write_sysreg(reg, val)  \
  asm volatile("msr " #reg ", %0" : : "r"(val))
#define write_sysreg(reg, val)  \
  do { u64 x = (u64)(val); __write_sysreg(reg, x); } while(0)

#define intr_enable()         asm volatile("msr daifclr, #0xf" ::: "memory")
#define intr_disable()        asm volatile("msr daifset, #0xf" ::: "memory")

#define local_irq_enable()    asm volatile("msr daifclr, #0x2" ::: "memory")
#define local_irq_disable()   asm volatile("msr daifset, #0x2" ::: "memory")

#define isb()     asm volatile("isb");
#define dsb(ty)   asm volatile("dsb " #ty);

#define wfi()     asm volatile("wfi" ::: "memory");
#define wfe()     asm volatile("wfe" ::: "memory");

#define sev()     asm volatile("sev");
#define sevl()    asm volatile("sevl");

#define HCR_VM            (1u << 0)
#define HCR_SWIO          (1u << 1)
#define HCR_PTW           (1u << 2)
#define HCR_FMO           (1u << 3)
#define HCR_IMO           (1u << 4)
#define HCR_AMO           (1u << 5)
#define HCR_TWI           (1u << 13)
#define HCR_TWE           (1u << 14)
#define HCR_TID3          (1u << 18)
#define HCR_TSC           (1u << 19)
#define HCR_TGE           (1u << 27)
#define HCR_TDZ           (1u << 28)
#define HCR_RW            (1u << 31)
#define HCR_CD            (1ul << 32)
#define HCR_ID            (1ul << 33)

#define HPFAR_FIPA_MASK   0xffffffffffful

#define MPIDR_AFFINITY_LEVEL0(m)    ((m) & 0xff)
#define MPIDR_AFFINITY_LEVEL1(m)    (((m) >> 8) & 0xff)
#define MPIDR_AFFINITY_LEVEL2(m)    (((m) >> 16) & 0xff)
#define MPIDR_AFFINITY_LEVEL3(m)    (((m) >> 32) & 0xff)

#define PSR_EL1H      (5)

#define SPSR_EL(spsr) (((spsr) & 0xf) >> 2)

#define __cacheline_aligned   __aligned(64)

static inline int cpuid() {
  int mpidr = read_sysreg(mpidr_el1);
  return mpidr & 0xf;
}

/*
 *  translation by at instruction
 *  e.g. at s12e1r, %0
 */
#define do_at_trans(ipa, stage, el, rw)   \
  asm volatile("at " #stage #el #rw ", %0" :: "r"(ipa) : "memory")

static inline bool local_irq_enabled() {
  return !((read_sysreg(daif) >> 7) & 0x1);
}

static inline bool local_irq_disabled() {
  return (read_sysreg(daif) >> 7) & 0x1;
}

static inline u64 r_sp() {
  u64 x;
  asm volatile("mov %0, sp" : "=r"(x));
  return x;
}

static inline u64 __irqsave() {
  u64 flags = read_sysreg(daif);

  local_irq_disable();

  return flags;
}

static inline void __irqrestore(u64 flags) {
  write_sysreg(daif, flags);  
}

#define irqsave(flags)      do { flags = __irqsave(); } while(0)
#define irqrestore(flags)   __irqrestore(flags)

void trapinit(void);

#endif    /* __ASSEMBLER__ */

#endif
