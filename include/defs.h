// defs.h
void
memset(void *dst, int c, unsigned int n);
//printf.c
void panic(char *s);
int printf(char *fmt, ...);

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