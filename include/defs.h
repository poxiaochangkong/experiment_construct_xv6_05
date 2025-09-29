//kalloc.c
void pmm_init(void); // 初始化内存管理器
void* alloc_page(void); // 分配一个物理页
void free_page(void* page); // 释放一个物理页
void* alloc_pages(int n); // 分配连续的n个页面