#include "types.h"
#include "spinlock.h"
#include "defs.h"
#include "riscv.h"
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock; // 新增锁
  struct run *freelist;
} kmem;

void freerange(void *pa_start, void *pa_end);


void
pmm_init(void)
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);//控制自end起始的地址
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);//地址对齐
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    free_page(p);//最初的run的next是NULL(bss段清零)
}

void
free_page(void *page)
{
  struct run *r;

  if(((uint64)page % PGSIZE) != 0 || (char*)page < end || (uint64)page >= PHYSTOP)
    panic("free_page");

  // Fill with junk to catch dangling refs.
  memset(page, 1, PGSIZE);

  r = (struct run*)page;

  acquire(&kmem.lock);//新增锁
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);//新增锁
  
}

void* 
alloc_page(void){
  struct run *r;

  acquire(&kmem.lock);//新增锁
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);//新增锁

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  if(!r)//分配失败,未来可考虑由异常来解决
    panic("alloc_page");
  return (void*)r;//返回分配的页的起始地址，分配失败为0
}

void*
alloc_pages(int n){
  struct run *r;
  r = kmem.freelist;
  for(int i = 1; i < n; i++){
    if(!kmem.freelist) //不足n页
      return 0;
    kmem.freelist = kmem.freelist->next;
  }
  return (void*)r;
}

// void 
// destroy_pagetable(pagetable_t pt){
//   for(int i = 0; i < 512; i++){
//     pde_t pte = pt[i];
//     if((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0){//有效且是二级或一级页表
//       uint64 child = PTE2PA(pte);
//       destroy_pagetable((pagetable_t)child);
//     }else if(pte & PTE_V){//有效且是叶子节点
//       uint64 pa = PTE2PA(pte);
//       free_page((void*)pa);
//     }
//   }
//   free_page((void*)pt);
// }

void test_physical_memory(void) {
// 测试基本分配和释放
 void *page1 = alloc_page();
 void *page2 = alloc_page();
 assert(page1 != page2);
 assert(((uint64)page1 & 0xFFF) == 0); // 页对齐检查

 // 测试数据写入
 *(int*)page1 = 0x12345678;
 assert(*(int*)page1 == 0x12345678);

 // 测试释放和重新分配
 free_page(page1);
 void *page3 = alloc_page();
 // page3可能等于page1（取决于分配策略）

 free_page(page2);
 free_page(page3);
 }

 