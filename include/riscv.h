#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024) // 128MB,pyhsical memory limit

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// 从虚拟地址提取页表索引
#define VPN_SHIFT(level) (12 + 9 * (level))//
#define VPN_MASK(va, level) (((va) >> VPN_SHIFT(level)) & 0x1FF)//提取第level级页表索引

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)//只保留页的高位地址
#define PTE2PA(pte) (((pte) >> 10) << 12)//移除10位的标志位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)//只保留低10位的标志位
//页表项的标志位
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access
// 页表相关定义
typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs