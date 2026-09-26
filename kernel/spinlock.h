#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include "types.h"

struct spinlock {
    volatile uint32 locked;
    const char *name;
    int owner;
};

void initlock(struct spinlock *lock, const char *name);
void acquire(struct spinlock *lock);
void release(struct spinlock *lock);
int holding(struct spinlock *lock);

void push_off(void);
void pop_off(void);

#endif
