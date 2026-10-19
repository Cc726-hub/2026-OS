#ifndef KERNEL_RISCV_H
#define KERNEL_RISCV_H

#include "types.h"

#define SSTATUS_SIE      (1UL << 1)
#define SSTATUS_SPP      (1UL << 8)
#define SIE_STIE         (1UL << 5)
#define SIE_SEIE         (1UL << 9)
#define SCAUSE_INTERRUPT (1UL << 63)
#define IRQ_S_TIMER      5
#define IRQ_S_EXTERNAL   9

static inline uint64
r_time(void)
{
    uint64 value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static inline uint64
r_sie(void)
{
    uint64 value;
    __asm__ volatile("csrr %0, sie" : "=r"(value));
    return value;
}

static inline uint64
r_stvec(void)
{
    uint64 value;
    __asm__ volatile("csrr %0, stvec" : "=r"(value));
    return value;
}

static inline uint64
r_satp(void)
{
    uint64 value;
    __asm__ volatile("csrr %0, satp" : "=r"(value));
    return value;
}

static inline void
w_sie(uint64 value)
{
    __asm__ volatile("csrw sie, %0" : : "r"(value) : "memory");
}

static inline void
w_stvec(uint64 value)
{
    __asm__ volatile("csrw stvec, %0" : : "r"(value) : "memory");
}

static inline void
w_satp(uint64 value)
{
    __asm__ volatile("csrw satp, %0" : : "r"(value) : "memory");
}

static inline void
sfence_vma(void)
{
    __asm__ volatile("sfence.vma zero, zero" : : : "memory");
}

static inline uint64
r_sstatus(void)
{
    uint64 value;
    __asm__ volatile("csrr %0, sstatus" : "=r"(value));
    return value;
}

static inline void
w_sstatus(uint64 value)
{
    __asm__ volatile("csrw sstatus, %0" : : "r"(value) : "memory");
}

static inline uint64
r_tp(void)
{
    uint64 value;
    __asm__ volatile("mv %0, tp" : "=r"(value));
    return value;
}

static inline void
intr_on(void)
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

static inline void
intr_off(void)
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

static inline int
intr_get(void)
{
    return (r_sstatus() & SSTATUS_SIE) != 0;
}

#endif
