#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "spinlock.h"
#include "param.h"
#include "proc.h"
#include "memlayout.h"


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;
struct spinlock wait_lock;//避免父进程和子进程在wait和exit时对proc表的并发访问，以及进程切换导致的多个子进程同一个pid


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
  //此处不需要使用锁，因为是在系统初始化阶段，尚无并发
  for(p = proc; p < &proc[NPROC]; p++) {//初始化进程槽
      initlock(&p->lock, "proc");
      p->state = UNUSED;//空进程槽，未挂载程序
      p->kstack = KSTACK((int) (p - proc));//紧随着trampoline的内核栈
  }
}


// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      printf("scheduler: checked process %d, state %d\n", p->pid, p->state);
      release(&p->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
}