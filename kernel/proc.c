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

extern pagetable_t kernel_pagetable;
extern char trampoline[]; // trampoline.S

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

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}


void
procinit(void)
{
  struct proc *p;
  //此处不需要使用锁，因为是在系统初始化阶段，尚无并发
  for(p = proc; p < &proc[NPROC]; p++) {//初始化进程槽
      initlock(&p->lock, "proc");
      p->state = UNUSED;//空进程槽，未挂载程序
     char *pa = alloc_page(); // 1. 静态分配物理页,进程较为复杂时，此处可能出错
      if(pa == 0) panic("kstack");
      
      uint64 va = KSTACK((int) (p - proc)); // 2. 计算虚拟地址
      
      kvmmap(kernel_pagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W); // 3. 建立映射
      
      p->kstack = va; // 4. 赋值
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
    intr_on();//允许中断，如果已有中断，则进入中断处理程序
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        //新进程由forkret释放锁，已有进程由yield释放锁
        p->state = RUNNING;
        c->proc = p;
        
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;//调度器模式
        found = 1;
      }
      //printf("scheduler: checked process %d, state %d\n", p->pid, p->state);
      release(&p->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
}

void
forkret(void)
{
  static int first = 1;
  struct proc *p = myproc(); // 获取当前进程

  // 1. 释放调度器(scheduler)在调用 swtch 前获取的锁
  // 这是 forkret 最重要的职责之一
  release(&p->lock); 

  if (first) {
    // 实验早期可能不需要文件系统初始化，仅打印提示
    first = 0;
    printf("init: first process started\n");
  }

  // 2. 适配实验五：执行内核线程任务
  // 我们约定：在 create_process 时，将任务函数的地址保存在 context.s0 中
  if (p->context.s0 != 0) {
    // 将 s0 转换为函数指针并调用
    void (*fn)(void) = (void(*)(void))p->context.s0;
    fn();
    
    // 如果任务函数返回了（通常不应该返回，应该调用 exit），我们暂时 panic
    panic("kernel thread returned");
  }

  // 3. 如果运行到这里，说明没有内核任务，且尚未实现用户态返回
  // 在实现实验六之前，这里作为一个占位符
  printf("forkret: no kernel task and userret not implemented\n");
  panic("forkret");
}

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = create_pagetable();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(map_page(pagetable, TRAMPOLINE, (uint64)trampoline, PTE_R | PTE_X) < 0){
    destroy_pagetable(pagetable);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(map_page(pagetable, TRAPFRAME, PGSIZE, PTE_R | PTE_W) < 0){
    unmap_page(pagetable, TRAMPOLINE);
    destroy_pagetable(pagetable);
    return 0;
  }

  return pagetable;
}

void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  unmap_page(pagetable, TRAMPOLINE);
  unmap_page(pagetable, TRAPFRAME);
  destroy_pagetable(pagetable);
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
void
free_process(struct proc *p)
{
  if(p->trapframe)
    free_page((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

static struct proc*
alloc_process(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  printf("alloc_process: allocated process %d\n", p->pid);
  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)alloc_page()) == 0){
    free_process(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    free_process(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;//伪造返回地址
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// entrypoint: 进程启动后要执行的函数指针
int
create_process(void (*entrypoint)(void))
{
  struct proc *p;

  // 1. 调用底层的分配函数获取一个进程结构
  // 注意：alloc_process 返回时已经持有了 p->lock
  if((p = alloc_process()) == 0){
    panic("create_process: alloc_process failed");
    return -1;
  }

  // 2. 设置内核线程的入口点
  // 在 forkret 函数中（proc.c），你会看到它检查 p->context.s0。
  // 如果 s0 不为 0，它就会跳转到 s0 指向的地址执行。
  // alloc_process 已经将 p->context.ra 设置为 forkret，所以
  // 调度器 context switch 到这里时，会先跳到 forkret，然后 forkret 跳到 entrypoint。
  p->context.s0 = (uint64)entrypoint;

  // 3. 将进程状态设置为 RUNNABLE
  // 这样调度器 (scheduler) 在遍历进程表时就能看到它，并选中它运行。
  p->state = RUNNABLE;

  // 4. 释放锁
  // alloc_process 在返回前获取了锁，目的是防止其他 CPU 在我们配置好之前就看到这个进程。
  // 现在配置完成，可以释放锁了。
  release(&p->lock);

  return p->pid;
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

void simple_task(void) {
    printf("Hello from process! PID: %d\n", mycpu()->proc->pid);
    while(1) {
        // 让出 CPU，打印字符证明还在运行
        printf("A"); 
        // 简单的延时循环，防止打印太快
        //yield();
        // 在实现了 yield 后可以使用 yield();
        printf("B");
        printf("\n");
        // 目前如果没有 yield，这个循环可能会一直占用 CPU 直到时钟中断（如果开了中断）
    }
}