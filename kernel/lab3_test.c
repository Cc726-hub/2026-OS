#include "lab3_test.h"
#if LAB3_TEST
#include "boot.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "spinlock.h"
#include "timer.h"
#include "trap.h"
#include "uart.h"

static uint32 done[NCPU];
extern void lab3_register_test(uint64 observed[34]);

static void
check(int condition, const char *message)
{
    if (!condition)
        panic(message);
}

void
lab3_test_hart(void)
{
    uint32 hart = (uint32)r_tp();
    check(intr_get(), "test: global interrupts disabled");
    check(r_stvec() == (uint64)kernel_vector, "test: wrong stvec");
    check(r_sie() == (SIE_SEIE | SIE_STIE), "test: wrong sie");

    if (TEST_CASE == 1)
        __asm__ volatile(".word 0x00000000");
    if (TEST_CASE == 2)
        *(volatile uint64 *)0x80000000UL = 0;

    /* Hold two nested locks across several timer deadlines on this hart. */
    struct spinlock outer, inner;
    initlock(&outer, "test outer");
    initlock(&inner, "test inner");
    acquire(&outer);
    acquire(&inner);
    uint64 before = timer_hart_ticks(hart);
    uint64 until = r_time() + 3 * TIMER_INTERVAL;
    while (r_time() < until)
        __asm__ volatile("nop");
    check(!intr_get() && timer_hart_ticks(hart) == before,
          "test: interrupted while holding locks");
    release(&inner);
    check(!intr_get(), "test: inner release enabled interrupts");
    release(&outer);
    check(intr_get(), "test: outer release failed to restore interrupts");

    /* A lock acquired with SIE already clear must keep it clear on release. */
    intr_off();
    acquire(&outer);
    release(&outer);
    check(!intr_get(), "test: disabled state was not preserved");
    intr_on();

    uint64 observed[34];
    before = timer_hart_ticks(hart);
    do {
        lab3_register_test(observed);
        for (uint32 reg = 0; reg < 32; reg++) {
            uint64 expected = 0x100UL + reg;
            if (reg == 0) expected = 0;
            if (reg == 2) expected = observed[32];
            if (reg == 4) expected = hart;
            if (reg == 31) expected = (uint64)observed;
            check(observed[reg] == expected, "test: trap corrupted a register");
        }
        check(observed[33] % 16 == 0, "test: misaligned trap stack");
    } while (timer_hart_ticks(hart) == before);
    check(intr_get(), "test: sret did not restore SIE");
    while (timer_hart_ticks(hart) < 5)
        __asm__ volatile("wfi");

    printf("PASS hart %d: register restore, nested locks, timer=%d\n",
           (int)hart, (int)timer_hart_ticks(hart));
    __atomic_store_n(&done[hart], 1, __ATOMIC_RELEASE);

    if (hart == boot_hart) {
        for (uint32 cpu = 0; cpu < NCPU; cpu++)
            while (!__atomic_load_n(&done[cpu], __ATOMIC_ACQUIRE))
                __asm__ volatile("wfi");
        uint64 total = timer_ticks();
        check(total > 0 && total <= timer_hart_ticks(hart),
              "test: system ticks counted on multiple harts");
        printf("LAB3 TEST PASS cpus=%d ticks=%d boot_hart=%d\n",
               NCPU, (int)total, (int)boot_hart);
        uint64 received_before = uart_received();
        printf("LAB3 UART READY\n");
        while (uart_received() == received_before)
            __asm__ volatile("wfi");
        check(trap_external_count() > 0, "test: missing PLIC claim");
        printf("\nLAB3 UART PASS received=%d external=%d\n",
               (int)uart_received(), (int)trap_external_count());
    }
}
#endif
