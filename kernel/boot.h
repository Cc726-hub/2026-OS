#ifndef KERNEL_BOOT_H
#define KERNEL_BOOT_H

#include "types.h"

extern uint64 boot_hart;
extern uint64 boot_dtb;

void start(uint64 hartid, uint64 opaque) __attribute__((noreturn));
void _entry(void);
void secondary_entry(void);
void early_trap(void);

#endif
