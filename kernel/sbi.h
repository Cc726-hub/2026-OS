#ifndef KERNEL_SBI_H
#define KERNEL_SBI_H

#include "types.h"

long sbi_hart_start(uint64 hartid, uint64 entry_pa, uint64 opaque);
long sbi_set_timer(uint64 deadline);

#endif
