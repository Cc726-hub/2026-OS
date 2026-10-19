#include "boot.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "sbi.h"
#include "spinlock.h"
#include "timer.h"

uint64 ticks;
static struct spinlock ticks_lock;
static uint64 hart_ticks[NCPU];

void
timer_init(void)
{
    initlock(&ticks_lock, "ticks");
}

void
timer_inithart(void)
{
    /* SBI TIME takes an absolute deadline in timebase units. */
    if (sbi_set_timer(r_time() + TIMER_INTERVAL) != 0)
        panic("SBI set_timer failed");
}

void
timer_interrupt_handler(void)
{
    uint32 hart = (uint32)r_tp();
    /* A future deadline clears STIP and schedules the next interrupt. */
    timer_inithart();
    __atomic_add_fetch(&hart_ticks[hart], 1, __ATOMIC_RELAXED);

    /* Count system time once per interval, independently of CPU count. */
    if (hart == boot_hart) {
        acquire(&ticks_lock);
        uint64 now = ++ticks;
        release(&ticks_lock);
#if !LAB3_TEST
        if (now % 10 == 0)
            printf("timer: ticks=%d (~%d s)\n", (int)now, (int)(now / 10));
#else
        (void)now;
#endif
    }
}

uint64
timer_ticks(void)
{
    acquire(&ticks_lock);
    uint64 now = ticks;
    release(&ticks_lock);
    return now;
}

uint64
timer_hart_ticks(uint32 hart)
{
    if (hart >= NCPU)
        panic("timer_hart_ticks: invalid hart");
    return __atomic_load_n(&hart_ticks[hart], __ATOMIC_RELAXED);
}
