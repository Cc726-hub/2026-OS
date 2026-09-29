#include "boot.h"
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
        printf("Lab 1 kernel entered main in supervisor mode.\n");
        printf("OpenSBI boot hart %d, kernel=%p, dtb=%p.\n",
               (int)hartid, (void *)_entry, (void *)boot_dtb);
        printf("printf test: d=%d x=%x p=%p c=%c s=%s %%\n",
               -123, 0x2026U, (void *)&started, 'A', "ok");

        for (uint32 cpu = 0; cpu < NCPU; cpu++) {
            if (cpu == hartid) {
                continue;
            }
            long error = sbi_hart_start(cpu, (uint64)secondary_entry, 0);
            if (error != 0) {
                printf("SBI hart_start failed: hart=%d error=%d\n",
                       (int)cpu, (int)error);
                for (;;) {
                    __asm__ volatile("wfi");
                }
            }
        }

        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0) {
            __asm__ volatile("nop");
        }
    }

    /* The printf lock keeps each complete line together across all harts. */
    printf("hart %d: shared UART output is protected by a spinlock.\n",
           (int)hartid);

    for (;;) {
        __asm__ volatile("wfi");
    }
}
