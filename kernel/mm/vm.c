#include "riscv.h"
#include "types.h"
#include "defs.h"

// 辅助函数（内部使用）
pte_t* walk_create(pagetable_t pt, uint64 va);
pte_t* walk_lookup(pagetable_t pt, uint64 va);