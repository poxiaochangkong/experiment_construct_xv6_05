#include "riscv.h"
#include "types.h"
#include "defs.h"

pagetable_t kernel_pagetable;

// 辅助函数（内部使用）
pde_t* walk_create(pagetable_t pt, uint64 va);//返回va对应的页表项，没有则创建
pde_t* walk_lookup(pagetable_t pt, uint64 va);//查找页表项

pde_t*
walk_lookup(pagetable_t pt, uint64 va){
    pde_t *pte;
    for(int level = 2; level > 0; level--){
        pte = &pt[VPN_MASK(va, level)];
        if((*pte & PTE_V) == 0)//此时该表项无效/不存在
            return 0;
        pt = (pagetable_t)PTE2PA(*pte);
    }
    pte = &pt[VPN_MASK(va, 0)];
    return pte;
}

pde_t*
walk_create(pagetable_t pt, uint64 va){
    pde_t *pte;
    for(int level = 2; level > 0; level--){
        pte = &pt[VPN_MASK(va, level)];
        if((*pte & PTE_V) == 0){//此时该表项无效/不存在
            //分配一个新的页表
            pagetable_t new_page = (pagetable_t)alloc_page();
            if(new_page == 0)
                return 0;
            memset(new_page, 0, PGSIZE);
            //更新页表项
            *pte = PA2PTE(new_page) | PTE_V;
        }
        pt = (pagetable_t)PTE2PA(*pte);
    }
    pte = &pt[VPN_MASK(va, 0)];
    return pte;
}

pde_t *
walk(pagetable_t pagetable, uint64 va, int alloc){
    pde_t *pte;
    if(alloc)
        pte = walk_create(pagetable, va);
    else
        pte = walk_lookup(pagetable, va);
    if(pte == 0)
        panic("walk");
    return pte;
}

int 
map_page(pagetable_t pt, uint64 va, uint64 pa, int perm){
    pde_t *pte;
    if(va % PGSIZE != 0 || pa % PGSIZE != 0)
        return -1;
    pte = walk_create(pt, va);
    if(pte == 0)
        return -1;
    if(*pte & PTE_V)//已经存在映射
        panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}

pagetable_t //没什么大作用，可以替换walk_create中的部分代码
create_pagetable(void){
    pagetable_t pt;
    pt = (pagetable_t)alloc_page();
    if(pt == 0)
        return 0;
    memset(pt, 0, PGSIZE);
    return pt;
}

void 
destroy_pagetable(pagetable_t pt){
  for(int i = 0; i < 512; i++){
    pde_t pte = pt[i];
    if((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0){//有效且是二级或一级页表
      uint64 child = PTE2PA(pte);
      destroy_pagetable((pagetable_t)child);
    }else if(pte & PTE_V){//有效且是叶子节点
      uint64 pa = PTE2PA(pte);
      free_page((void*)pa);
    }
  }
  free_page((void*)pt);
}

void 
dump_pagetable(pagetable_t pt, int level){
    for(int i = 0; i < 512; i++){
        pde_t pte = pt[i];
        if(pte & PTE_V){
        for(int j = 0; j < level; j++)
            printf("  ");
        uint64 va = (uint64)i << VPN_SHIFT(level);
        printf("VA: 0x%lx -> PTE: 0x%lx\n", va, pte);
        if((pte & (PTE_R | PTE_W | PTE_X)) == 0){//不是叶子节点
            uint64 child = PTE2PA(pte);
            dump_pagetable((pagetable_t)child, level - 1);
        }
        }
    }
}

void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    uint64 a, last;
    a = PGROUNDDOWN(va);
    last = PGROUNDUP(va + sz);
    for(; a < last; a += PGSIZE){
        if(map_page(kpgtbl, a, pa, perm) != 0)
        panic("kvmmap");
        pa += PGSIZE;
    }
}

void kvminithart(void) {
    // 激活内核页表
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}
void 
kvminit(void){
    // 1. 创建内核页表
    kernel_pagetable = create_pagetable();

    // 2. 映射内核代码段（R+X权限）
    kvmmap(kernel_pagetable, KERNBASE, KERNBASE,
        (uint64)etext - KERNBASE, PTE_R | PTE_X);
    // 3. 映射内核数据段（R+W权限）
    kvmmap(kernel_pagetable, (uint64)etext, (uint64)etext,
    PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // 4. 映射设备（UART等）
    kvmmap(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
 }
