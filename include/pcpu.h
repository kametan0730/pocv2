#ifndef PCPU_H
#define PCPU_H

#include "types.h"
#include "aarch64.h"
#include "param.h"
#include "mm.h"
#include "irq.h"
#include "msg.h"
#include "spinlock.h"
#include "compiler.h"

extern char _stack[PAGESIZE*NCPU_MAX] __aligned(PAGESIZE);

struct cpu_enable_method {
  int (*init)(int cpu);
  int (*boot)(int cpu, u64 entrypoint);
};

struct pcpu {
  void *stackbase;
  int mpidr;

  bool online;
  bool wakeup;

  struct device_node *device;

  const struct cpu_enable_method *enable_method;
  
  struct pocv2_msg_queue recv_waitq;

  int irq_depth;
  bool lazyirq_enabled;
  int lazyirq_depth;
  u64 nirq;

  union {
    struct {
      void *gicr_base;
    } v3;
    struct {
      ;
    } v2;
  };
} __cacheline_aligned;

extern struct pcpu pcpus[NCPU_MAX];

void cpu_stop_local(void);
void cpu_stop_all(void);

int cpu_boot(int cpu, u64 entrypoint);

void pcpu_init_core(void);
void pcpu_init(void);

#define mycpu         (&pcpus[cpuid()])
#define localcpu(id)  (&pcpus[id]) 

static inline struct pcpu *get_cpu(int cpu) {
  if(cpu >= NCPU_MAX)
    panic("no cpu!");

  return &pcpus[cpu];
}

#define local_lazyirq_enable()      (mycpu->lazyirq_enabled = true)
#define local_lazyirq_disable()     (mycpu->lazyirq_enabled = false)
#define local_lazyirq_enabled()     (mycpu->lazyirq_enabled)

#define foreach_up_cpu(cpu)    \
  for(cpu = pcpus; cpu < &pcpus[NCPU_MAX]; cpu++) \
    if(cpu->wakeup)


#define lazyirq_enter()         \
  do {                          \
    local_lazyirq_disable();    \
    mycpu->lazyirq_depth++;     \
  } while(0);

#define lazyirq_exit()        \
  do {                        \
    local_lazyirq_enable();   \
    mycpu->lazyirq_depth--;   \
  } while(0);

#define in_lazyirq()      (mycpu->lazyirq_depth != 0)

static inline bool in_interrupt() {
  return mycpu->irq_depth != 0;
}

#endif
