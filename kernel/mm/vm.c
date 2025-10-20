#include "types.h"
#include "riscv.h"
#include "defs.h"

extern char etext[];  // kernel.ld sets this to end of kernel code segment
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

    if (sz == 0) // 处理大小为0的情况
        return;

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + sz - 1); // 修改这里
    for(;;){ // 使用无限循环，在内部判断
        if(map_page(kpgtbl, a, pa, perm) != 0)
            panic("kvmmap");
        if (a == last)
            break;
        a += PGSIZE;
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


 void test_pagetable(void) {
 pagetable_t pt = create_pagetable();

 // 测试基本映射
 uint64 va = 0x1000000;
 uint64 pa = (uint64)alloc_page();
 assert(map_page(pt, va, pa, PTE_R | PTE_W) == 0);

 // 测试地址转换
 pde_t *pte = walk_lookup(pt, va);
 assert(pte != 0 && (*pte & PTE_V));
 assert(PTE2PA(*pte) == pa);

 // 测试权限位
 assert(*pte & PTE_R);
 assert(*pte & PTE_W);
 assert(!(*pte & PTE_X));
 }

 // 定义一个全局变量用于测试数据段的访问
volatile static int test_data_integrity = 0xDEADBEEF;

void test_virtual_memory(void) {
  printf("--- Starting test_virtual_memory ---\n");
  
  // 1. 分页启用前的状态检查
  printf("Value before enabling paging: 0x%x\n", test_data_integrity);
  printf("Calling printf before paging works.\n");
  
  // 2. 启用分页
  printf("Enabling virtual memory...\n");
  kvminit();
  kvminithart();
  printf("Virtual memory enabled.\n");
  
  // 3. 分页启用后的状态验证

  // 测试1: 内核代码仍然可执行
  // 如果程序能执行到这里并打印信息，说明PC可以正确地从虚拟地址翻译，代码段映射成功。
  printf("Kernel code execution test: PASSED (still running after kvminithart)\n");

  // 测试2: 内核数据仍然可访问
  // 检查我们之前定义的全局变量的值是否仍然正确。
  if (test_data_integrity == 0xDEADBEEF) {
    printf("Kernel data access test: PASSED (value is 0x%x)\n", test_data_integrity);
  } else {
    printf("Kernel data access test: FAILED\n");
    panic("Data integrity check failed after paging.");
  }

  // 尝试修改数据
  test_data_integrity = 0x12345678;
  if (test_data_integrity == 0x12345678) {
      printf("Kernel data write test: PASSED (new value is 0x%x)\n", test_data_integrity);
  } else {
      printf("Kernel data write test: FAILED\n");
      panic("Data write failed after paging.");
  }
  
  // 测试3: 设备访问仍然正常
  // 再次调用printf，并使用不同的格式化字符，来全面地测试UART设备访问。
  printf("Device access test (UART): PASSED (printf with %%d and %%s works: %d, %s)\n", 100, "OK");

  printf("--- test_virtual_memory finished successfully ---\n\n");
}