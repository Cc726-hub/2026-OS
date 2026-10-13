#include "boot.h"
#include "mem.h"
#include "lab2_test.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "sbi.h"
#include "types.h"
#include "uart.h"

static volatile uint32 started;

void
main(void)
{
    uint32 hartid = (uint32)r_tp();

    if (hartid == boot_hart) {
        uartinit();
        printfinit();

        printf("\nHello, OS!\n");
        printf("Lab 2: physical memory and Sv39 kernel paging.\n");
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
    }

    printf("hart %d: Sv39 enabled, satp=%p; shared UART is locked.\n",
           (int)hartid, (void *)r_satp());
#if LAB2_TEST
    lab2_test_hart();
#endif

    for (;;) {
        __asm__ volatile("wfi");
    }
}
