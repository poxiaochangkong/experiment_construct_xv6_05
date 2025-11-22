struct spinlock; // 前向声明，避免循环依赖



// defs.h
void
memset(void *dst, int c, unsigned int n);
//printf.c
void panic(char *s);
int printf(char *fmt, ...);
void assert(int condition);

//uart.c
void uart_intr(void);// called by devintr in trap.c ，读取键盘输入给控制台

//console.c
void consoleintr(int c);// called by uartintr in uart.c， 接受来自uart_intr的输入，do nothing for now

//kalloc.c
void pmm_init(void); // 初始化内存管理器，对应kalloc.c中的kinit函数
void* alloc_page(void); // 分配一个物理页，对应kalloc.c中的kalloc函数
void free_page(void* page); // 释放一个物理页，对应kalloc.c中的kfree函数
void* alloc_pages(int n); // 分配连续的n个页面，尚未正确地实现

//vm.c
// 基本操作接口
pagetable_t create_pagetable(void);// 创建一个新的页表，对应vm.c中的uvmcreate函数
int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm);// 在页表pt中映射虚拟地址va到物理地址pa，权限为perm，对应vm.c中的mappages函数
void destroy_pagetable(pagetable_t pt);// 销毁页表pt，对应vm.c中的uvmfree函数
void dump_pagetable(pagetable_t pt, int level);// 实现页表打印功能用于调试
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
void kvminit(void);// 初始化内核页表，对应vm.c中的kvminit函数
void kvminithart(void);// 激活内核页表，对应vm.c中的kvminithart函数

//trap.c
void trapinithart(void); // 初始化trap处理函数，对应trap.c中的trapinithart函数
void timer_init(void); // 初始化时钟中断，对应trap.c中的timer_init函数
void intr_on(void); // 开启中断，对应trap.c中的intr_on函数

//plic.c
void plicinit(void); // 初始化PLIC，对应plic.c中的plicinit函数
void plicinithart(void); // 初始化PLIC的hart相关设置，对应plic.c
void plic_complete(int irq); // 通知PLIC中断处理完成，对应plic.c中的plic_complete函数
int plic_claim(void); // 从PLIC获取待处理的中断号，对应plic.c中的plic_claim函数

//proc.c
void scheduler(void); // 进程调度函数，对应proc.c中的scheduler函数
struct cpu* mycpu(void);// 获取当前CPU信息，对应proc.c中的mycpu函数
void procinit(void); // 初始化进程表，对应proc.c中的procinit函数

//spinlock.c
void initlock(struct spinlock *lk, char *name); // 初始化自旋锁，对应spinlock.c中的initlock函数
void acquire(struct spinlock *lk); // 获取自旋锁，对应spinlock.c中的acquire函数
void release(struct spinlock *lk); // 释放自旋锁，对应spinlock.c中的release函数
int holding(struct spinlock *lk); // 检查当前CPU是否持有自旋锁，对应spinlock.c中的holding函数


