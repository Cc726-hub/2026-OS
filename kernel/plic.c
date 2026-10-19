#include "memlayout.h"
#include "plic.h"
#include "riscv.h"
#include "types.h"

void
plic_init(void)
{
    *(volatile uint32 *)(PLIC_BASE + UART_IRQ * 4) = 1;
}

void
plic_inithart(void)
{
    uint64 hart = r_tp();
    *(volatile uint32 *)PLIC_SENABLE(hart) = 1U << UART_IRQ;
    *(volatile uint32 *)PLIC_STHRESHOLD(hart) = 0;
}

int
plic_claim(void)
{
    return (int)*(volatile uint32 *)PLIC_SCLAIM(r_tp());
}

void
plic_complete(int irq)
{
    *(volatile uint32 *)PLIC_SCLAIM(r_tp()) = (uint32)irq;
}
