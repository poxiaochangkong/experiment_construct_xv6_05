#include "types.h"
#include "defs.h"
#include "riscv.h"

extern void kernelvec();  //defined in kernelvec.S

void
trapinithart(void) //call by main to initialize stvec for this hart
{
  w_stvec((uint64)kernelvec);
}

void kerneltrap()
{
  // currently we don't handle any traps
  panic("kerneltrap");
}

