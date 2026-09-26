#include "riscv.h"
#include "types.h"

extern uint8 _bss_start[];
extern uint8 _bss_end[];
extern void main(void);

/*
 * This variable must live in .data, not .bss. Secondary harts read it while
 * hart 0 clears .bss, so its non-zero initial value is intentional.
 */
static volatile int bss_ready = -1;

void start(void) __attribute__((noreturn));

void
start(void)
{
    uint64 hartid = r_tp();

    if (hartid == 0) {
        for (uint8 *p = _bss_start; p < _bss_end; p++) {
            *p = 0;
        }
        __atomic_store_n(&bss_ready, 1, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&bss_ready, __ATOMIC_ACQUIRE) != 1) {
            __asm__ volatile("nop");
        }
    }

    /* Return from mret in supervisor mode and begin at main. */
    uint64 status = r_mstatus();
    status &= ~MSTATUS_MPP_MASK;
    status |= MSTATUS_MPP_S;
    w_mstatus(status);
    w_mepc((uint64)main);

    /* Lab 1 uses physical addresses directly, so paging stays disabled. */
    w_satp(0);
    __asm__ volatile("sfence.vma zero, zero");

    /* Allow supervisor mode to access the whole physical address space. */
    w_pmpaddr0(~0UL >> 10);
    w_pmpcfg0(0x0f);

    /* Machine-mode interrupts are not used in this lab. */
    w_mie(0);

    __asm__ volatile("mret");
    __builtin_unreachable();
}
