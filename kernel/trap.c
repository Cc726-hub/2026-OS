#include "memlayout.h"
#include "plic.h"
#include "printf.h"
#include "riscv.h"
#include "timer.h"
#include "trap.h"
#include "uart.h"

_Static_assert(sizeof(struct kernel_trapframe) == 288, "trap frame layout");
static uint64 external_count;

void
trap_kernel_init(void)
{
    plic_init();
    timer_init();
}

void
trap_kernel_inithart(void)
{
    intr_off();
    w_stvec((uint64)kernel_vector);  /* Direct mode; entry is 4-byte aligned. */
    plic_inithart();
    timer_inithart();
    w_sie(SIE_SEIE | SIE_STIE);
}

void
external_interrupt_handler(void)
{
    int irq = plic_claim();
    /* Another hart may already have claimed a pending shared interrupt. */
    if (irq == 0)
        return;
    if (irq == UART_IRQ) {
        __atomic_add_fetch(&external_count, 1, __ATOMIC_RELAXED);
        uart_intr();
    }
    /* Complete every nonzero claim, even if the source is unexpected. */
    plic_complete(irq);
    if (irq != UART_IRQ)
        panic("unexpected PLIC interrupt");
}

void
trap_kernel_handler(struct kernel_trapframe *frame)
{
    if (!(frame->sstatus & SSTATUS_SPP) || intr_get())
        panic("invalid kernel trap state");

    if (frame->scause & SCAUSE_INTERRUPT) {
        switch (frame->scause & ~SCAUSE_INTERRUPT) {
        case IRQ_S_EXTERNAL:
            external_interrupt_handler();
            return;
        case IRQ_S_TIMER:
            timer_interrupt_handler();
            return;
        }
    }
    /* Lab 3 handles interrupts. Exceptions retain the fatal Lab 2 diagnostic. */
    early_trap_report(frame->scause, frame->sepc, frame->stval);
}

uint64
trap_external_count(void)
{
    return __atomic_load_n(&external_count, __ATOMIC_RELAXED);
}
