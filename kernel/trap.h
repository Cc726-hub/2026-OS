#ifndef KERNEL_TRAP_H
#define KERNEL_TRAP_H

#include "types.h"

/* Layout shared with kernel_vector.S. x[2] holds the original stack pointer. */
struct kernel_trapframe {
    uint64 x[32];
    uint64 sepc;
    uint64 sstatus;
    uint64 stval;
    uint64 scause;
};

void kernel_vector(void);
void trap_kernel_init(void);
void trap_kernel_inithart(void);
void trap_kernel_handler(struct kernel_trapframe *frame);
void external_interrupt_handler(void);
uint64 trap_external_count(void);

#endif
