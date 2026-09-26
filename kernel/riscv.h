#ifndef KERNEL_RISCV_H
#define KERNEL_RISCV_H

#include "types.h"

#define MSTATUS_MPP_MASK (3UL << 11)
#define MSTATUS_MPP_S    (1UL << 11)

#define SSTATUS_SIE      (1UL << 1)

static inline uint64
r_mstatus(void)
{
    uint64 value;
    __asm__ volatile("csrr %0, mstatus" : "=r"(value));
    return value;
}

static inline void
w_mstatus(uint64 value)
{
    __asm__ volatile("csrw mstatus, %0" : : "r"(value));
}

static inline void
w_mepc(uint64 value)
{
    __asm__ volatile("csrw mepc, %0" : : "r"(value));
}

static inline void
w_mie(uint64 value)
{
    __asm__ volatile("csrw mie, %0" : : "r"(value));
}

static inline void
w_satp(uint64 value)
{
    __asm__ volatile("csrw satp, %0" : : "r"(value));
}

static inline void
w_pmpaddr0(uint64 value)
{
    __asm__ volatile("csrw pmpaddr0, %0" : : "r"(value));
}

static inline void
w_pmpcfg0(uint64 value)
{
    __asm__ volatile("csrw pmpcfg0, %0" : : "r"(value));
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
    __asm__ volatile("csrw sstatus, %0" : : "r"(value));
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
