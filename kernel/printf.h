#ifndef KERNEL_PRINTF_H
#define KERNEL_PRINTF_H
#include "types.h"

void panic(const char *message) __attribute__((noreturn));
void early_trap_report(uint64 cause, uint64 epc, uint64 tval) __attribute__((noreturn));

void printfinit(void);
void printf(const char *format, ...);

#endif
