#ifndef KERNEL_MEMLAYOUT_H
#define KERNEL_MEMLAYOUT_H

#define DRAM_BASE 0x80000000UL
#define DRAM_SIZE (128UL * 1024 * 1024)
#define DRAM_END (DRAM_BASE + DRAM_SIZE)
#define UART_BASE 0x10000000UL
#define PLIC_BASE 0x0c000000UL
#define PLIC_SIZE 0x04000000UL
#define UART_IRQ 10
#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2080UL + (hart) * 0x100UL)
#define PLIC_STHRESHOLD(hart) (PLIC_BASE + 0x201000UL + (hart) * 0x2000UL)
#define PLIC_SCLAIM(hart) (PLIC_STHRESHOLD(hart) + 4)

/* QEMU virt's fixed timebase is 10 MHz; this is not the CPU frequency. */
#define TIMEBASE_FREQ 10000000UL

#endif
