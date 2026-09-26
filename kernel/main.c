#include "printf.h"
#include "riscv.h"
#include "types.h"
#include "uart.h"

static volatile uint32 started;

void
main(void)
{
    uint32 hartid = (uint32)r_tp();

    if (hartid == 0) {
        uartinit();
        printfinit();

        printf("\nHello, OS!\n");
        printf("Lab 1 kernel entered main in supervisor mode.\n");
        printf("printf test: d=%d x=%x p=%p c=%c s=%s %%\n",
               -123, 0x2026U, (void *)&started, 'A', "ok");

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
