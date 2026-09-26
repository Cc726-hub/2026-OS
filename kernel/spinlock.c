#include "riscv.h"
#include "spinlock.h"
#include "types.h"

#define NCPU 8

struct cpu_interrupt_state {
    int nesting;
    int was_enabled;
};

static struct cpu_interrupt_state cpu_states[NCPU];

static uint32
cpuid(void)
{
    return (uint32)r_tp();
}

static struct cpu_interrupt_state *
current_cpu_state(void)
{
    return &cpu_states[cpuid()];
}

void
initlock(struct spinlock *lock, const char *name)
{
    lock->locked = 0;
    lock->name = name;
    lock->owner = -1;
}

int
holding(struct spinlock *lock)
{
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0 &&
           lock->owner == (int)cpuid();
}

void
acquire(struct spinlock *lock)
{
    push_off();

    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE) != 0) {
        __asm__ volatile("nop");
    }

    lock->owner = (int)cpuid();
}

void
release(struct spinlock *lock)
{
    if (!holding(lock)) {
        for (;;) {
            __asm__ volatile("wfi");
        }
    }

    lock->owner = -1;
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
    pop_off();
}

void
push_off(void)
{
    int old_state = intr_get();
    struct cpu_interrupt_state *state;

    intr_off();
    state = current_cpu_state();
    if (state->nesting == 0) {
        state->was_enabled = old_state;
    }
    state->nesting++;
}

void
pop_off(void)
{
    struct cpu_interrupt_state *state = current_cpu_state();

    if (intr_get() || state->nesting < 1) {
        for (;;) {
            __asm__ volatile("wfi");
        }
    }

    state->nesting--;
    if (state->nesting == 0 && state->was_enabled) {
        intr_on();
    }
}
