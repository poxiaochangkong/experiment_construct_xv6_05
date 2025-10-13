#include "riscv.h"
#include "types.h"
#include "defs.h"

// 辅助函数（内部使用）
pte_t* walk_create(pagetable_t pt, uint64 va);//返回va对应的页表项，没有则创建
pte_t* walk_lookup(pagetable_t pt, uint64 va);//查找页表项

pte_t*
walk_lookup(pagetable_t pt, uint64 va){
    pte_t *pte;
    for(int level = 2; level > 0; level--){
        pte = &pt[VPN_MASK(va, level)];
        if((*pte & PTE_V) == 0)//此时该表项无效/不存在
            return 0;
        pt = (pagetable_t)PTE2PA(*pte);
    }
    pte = &pt[VPN_MASK(va, 0)];
    return pte;
}

pte_t*
walk_create(pagetable_t pt, uint64 va){
    pte_t *pte;
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

pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc){
    pte_t *pte;
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
    pte_t *pte;
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
    pte_t pte = pt[i];
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