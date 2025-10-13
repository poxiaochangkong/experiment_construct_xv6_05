//kalloc.c
void pmm_init(void); // 初始化内存管理器，对应kalloc.c中的kinit函数
void* alloc_page(void); // 分配一个物理页，对应kalloc.c中的kalloc函数
void free_page(void* page); // 释放一个物理页，对应kalloc.c中的kfree函数
void* alloc_pages(int n); // 分配连续的n个页面，尚未正确地实现
//vm.c
// 基本操作接口
pagetable_t create_pagetable(void);
int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm);
void destroy_pagetable(pagetable_t pt);

