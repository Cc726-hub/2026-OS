#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include "memlayout.h"
#include "types.h"

#define TIMER_INTERVAL (TIMEBASE_FREQ / 10)

extern uint64 ticks;
void timer_init(void);
void timer_inithart(void);
void timer_interrupt_handler(void);
uint64 timer_ticks(void);
uint64 timer_hart_ticks(uint32 hart);

#endif
