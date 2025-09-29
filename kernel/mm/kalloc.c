#include "types.h"
#include "defs.h"
#include "riscv.h"
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct run *freelist;
} kmem;

void freerange(void *pa_start, void *pa_end);
void memset(void *dst, int c, uint64 n);
void printf(char*, ...);

void
pmm_init(void)
{
  
  freerange(end, (void*)PHYSTOP);
}
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    free_page(p);//最初的run的next是NULL(bss段清零)
}

void
free_page(void *page)
{
  struct run *r;

  if(((uint64)page % PGSIZE) != 0 || (char*)page < end || (uint64)page >= PHYSTOP)
    printf("kfree");

  // Fill with junk to catch dangling refs.
  memset(page, 1, PGSIZE);

  r = (struct run*)page;

 
  r->next = kmem.freelist;
  kmem.freelist = r;
  
}
void* 
alloc_page(void){
  struct run *r;
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
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