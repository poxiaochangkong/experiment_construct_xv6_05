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
extern struct spinlock tickslock;
extern int ticks;

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

void
procinit(void)
{
  struct proc *p;
  initlock(&wait_lock, "wait_lock");
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
// 设置父进程：如果当前有进程在运行，则新进程是当前进程的孩子
  struct proc *curr_p = myproc();
  if(curr_p) {
    acquire(&wait_lock); // 必须持有 wait_lock 才能修改父子关系
    p->parent = curr_p;
    release(&wait_lock);
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

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)//需要持有锁才能调用sleep
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // 【修正点 1】：只有当 lk 不是 p->lock 时，才需要获取 p->lock
  // 如果 lk 就是 p->lock，说明我们已经持有它了，不需要（也不能）再次 acquire
  if(lk != &p->lock){
    acquire(&p->lock);
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // 【修正点 2】：恢复锁的状态
  // 如果 lk 不是 p->lock，我们需要释放 p->lock 并重新获取 lk
  // 如果 lk 就是 p->lock，我们一直持有它，不需要做任何操作
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
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

// 退出当前进程。如果不返回，则变为 ZOMBIE 状态。
void
exit_process(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // 关闭所有打开的文件 (实验5阶段文件系统可能未就绪，可暂时注释)
  // for(int fd = 0; fd < NOFILE; fd++){
  //   if(p->ofile[fd]){
  //     struct file *f = p->ofile[fd];
  //     fileclose(f);
  //     p->ofile[fd] = 0;
  //   }
  // }

  // 释放当前目录 (同上，文件系统相关)
  // begin_op();
  // iput(p->cwd);
  // end_op();
  // p->cwd = 0;

  // 获取 wait_lock，因为我们要改变父子关系并唤醒父进程
  acquire(&wait_lock);

  // 将当前进程的所有子进程“过继”给 init 进程
  // 这样确保子进程退出后总有人能回收它们
  struct proc *pp;
  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      if(pp->state == ZOMBIE && initproc != 0){
        wakeup(initproc);
      }
    }
  }

  // 唤醒父进程，让它在 wait_process 中醒来回收我们
  if(p->parent)
    wakeup(p->parent);

  // 获取自己的锁，准备修改状态并调度
  acquire(&p->lock);

  p->xstate = status; // 记录退出状态
  p->state = ZOMBIE;  // 变更为僵尸状态

  // 此时已经持有了 p->lock，可以释放 wait_lock
  release(&wait_lock);

  // 调度器会释放 p->lock 并切换进程
  // 当前进程将永远停在这里，直到被父进程回收（free_process）
  sched();

  panic("zombie exit");
}

// 等待子进程退出。
// 如果找到退出的子进程，返回其 PID 并通过 status 返回退出状态。
// 如果没有子进程，返回 -1。
int
wait_process(int *status)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // 必须持有 wait_lock 才能查看和修改子进程的 parent 指针及状态
  acquire(&wait_lock);

  for(;;){
    // 扫描进程表，查找当前进程的子进程
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // 获取子进程锁以检查其状态
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // 找到一个僵尸子进程，准备回收
          pid = np->pid;
          
          // 如果提供了 status 地址，将退出状态复制过去
          // 注意：如果 status 是用户空间地址，正式版 xv6 需要用 copyout
          // 实验5阶段若在内核测试，直接赋值即可
          if(status != 0)
            *status = np->xstate;
          
          // 释放进程资源（内核栈、页表、trapframe等）
          // free_process 定义在 proc.c 中，且假设已持有 np->lock
          free_process(np);
          
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        
        // 子进程还没退出
        release(&np->lock);
      }
    }

    // 如果没有子进程，或者当前进程被杀死了，就不再等待
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }

    // 还有子进程在运行，在此休眠等待子进程唤醒（在 exit_process 中唤醒）
    // sleep 会原子地释放 wait_lock 并让出 CPU
    sleep(p, &wait_lock);
  }
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


// init 进程的主体函数
void
init_task(void)
{
  printf("init: starting\n");
  for(;;){
    // 尝试回收僵尸子进程
    int status;
    int pid = wait_process(&status);

    if(pid >= 0){
      // 成功回收了一个僵尸进程
      printf("init: reaped zombie pid %d\n", pid);
    } else {
      // 目前没有需要回收的子进程，或者没有子进程
      // 让出 CPU，避免占满时间片，等待下一次被唤醒
      // 注意：在 exit_process 中，会有 wakeup(initproc) 唤醒我们
      yield(); 
    }
  }
}

// 创建系统第一个进程
void
userinit(void)
{
  struct proc *p;

  // 1. 分配进程控制块
  p = alloc_process();
  
  // 2. [关键] 设置全局 initproc 指针
  initproc = p;
  
  // 3. 设置上下文，让它运行 init_task
  p->context.s0 = (uint64)init_task; // 按照我们之前的约定，s0存放入口地址
  p->context.ra = (uint64)forkret;   // 返回地址设为 forkret
  
  // 4. 设置状态为就绪
  p->state = RUNNABLE;

  // 5. 释放锁（alloc_process 返回时持有锁）
  release(&p->lock);
  
  printf("userinit: created init process pid %d\n", p->pid);
}

// kernel/proc.c

void simple_task(void) {
    struct proc *p = myproc();
    
    // 判定角色逻辑：
    // 1. p->parent == 0: 说明是由 main 函数(无进程上下文)创建的，它是测试的发起者。
    // 2. p->parent == initproc: 如果你实现了 userinit，它可能是由 init 收养的(视具体实现而定)，也可以视为独立任务。
    // 3. 其他情况: 说明是由上面的父进程创建的，它是子进程。
    
    int is_tester = (p->parent == 0 || (initproc && p->parent == initproc));

    printf("Process PID %d is running...\n", p->pid);

    if (is_tester) {
        // === 父进程逻辑 ===
        printf("\n=== TEST START: Single Function Exit/Wait ===\n");
        printf("[Parent] PID %d: I am the parent. Spawning child...\n", p->pid);

        // 递归调用 create_process，让子进程也执行 simple_task
        // 但子进程进来后，会进入 else 分支
        int child_pid = create_process(simple_task);
        
        if (child_pid < 0) {
            printf("[Parent] Failed to create child.\n");
            while(1);
        }

        printf("[Parent] Created child PID %d. Waiting for it to exit...\n", child_pid);
        
        int status = -999;
        int waited_pid = wait_process(&status);
        
        if (waited_pid == child_pid) {
            printf("[Parent] Catch exited child PID %d.\n", waited_pid);
            printf("[Parent] Exit status: %d (Expected: 88)\n", status);
            
            if (status == 88) {
                printf("=== TEST PASSED ===\n");
            } else {
                printf("=== TEST FAILED: Wrong status ===\n");
            }
        } else {
            printf("[Parent] Error: wait_process returned %d\n", waited_pid);
        }
        
        // 测试结束，父进程进入死循环，打印心跳证明系统没挂
        printf("[Parent] Test finished. Idling...\n");
        while(1) {
            // 空循环或简单的延时，避免刷屏
            for(volatile int i=0; i<10000000; i++); 
            printf(".");
        }
    } else {
        // === 子进程逻辑 ===
        printf("  [Child] PID %d: I am the child.\n", p->pid);
        
        // 验证父指针是否正确
        if (p->parent) {
            printf("  [Child] My parent is PID %d.\n", p->parent->pid);
        } else {
            printf("  [Child] Error: I have no parent!\n");
        }

        printf("  [Child] Exiting with status 88...\n");
        
        // 子进程退出，测试 wait 能否获取到 88 这个状态码
        exit_process(88);
    }
}