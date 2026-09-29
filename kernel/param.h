#ifndef KERNEL_PARAM_H
#define KERNEL_PARAM_H

#ifndef NCPU
#define NCPU 2
#endif

#if NCPU < 1 || NCPU > 8
#error "QEMU CPU count must be between 1 and 8"
#endif

#define STACK_SHIFT 14
#define STACK_SIZE (1 << STACK_SHIFT)

#endif
