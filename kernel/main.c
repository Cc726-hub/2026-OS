#include "boot.h"
#include "mem.h"
#include "lab2_test.h"
#include "lab3_test.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "sbi.h"
#include "types.h"
#include "uart.h"
#include "trap.h"

static volatile uint32 started;

void
main(void)
{
    uint32 hartid = (uint32)r_tp();

    if (hartid == boot_hart) {
        uartinit();
        printfinit();

        printf("\nHello, OS!\n");
        printf("Lab 3: supervisor traps, UART and timer interrupts.\n");
        printf("OpenSBI boot hart %d, kernel=%p, dtb=%p.\n",
               (int)hartid, (void *)_entry, (void *)boot_dtb);
        printf("printf test: d=%d x=%x p=%p c=%c s=%s %%\n",
               -123, 0x2026U, (void *)&started, 'A', "ok");

        pmem_init();
#if LAB2_TEST
        lab2_test_physical();
#endif
        kvm_init();
        kvm_inithart();
#if LAB2_TEST
        lab2_test_virtual();
#endif

#if !LAB2_TEST
        trap_kernel_init();
        trap_kernel_inithart();
        uart_intr_enable();
#endif

        for (uint32 cpu = 0; cpu < NCPU; cpu++) {
            if (cpu == hartid) {
                continue;
            }
            long error = sbi_hart_start(cpu, (uint64)secondary_entry, 0);
            if (error != 0) {
                printf("SBI hart_start failed: hart=%d error=%d\n",
                       (int)cpu, (int)error);
                panic("SBI hart_start failed");
            }
        }

        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0) {
            __asm__ volatile("nop");
        }
        kvm_inithart();
#if !LAB2_TEST
        trap_kernel_inithart();
#endif
    }

    printf("hart %d: Sv39 enabled, satp=%p; shared UART is locked.\n",
           (int)hartid, (void *)r_satp());
#if LAB2_TEST
    lab2_test_hart();
#else
    printf("hart %d: trap entry ready, UART and timer interrupts enabled.\n",
           (int)hartid);
    intr_on();
#if LAB3_TEST
    lab3_test_hart();
#endif
#endif

    for (;;) {
        __asm__ volatile("wfi");
    }
}
