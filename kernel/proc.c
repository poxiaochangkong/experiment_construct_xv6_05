#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "spinlock.h"
#include "param.h"
#include "proc.h"


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int
cpuid()
{
  int id = r_tp();
  return id;
}

struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

void
procinit(void)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}


void
scheduler(void)
{
  while(1);
}