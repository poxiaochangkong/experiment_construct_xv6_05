#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024) // 128MB,pyhsical memory limit

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))