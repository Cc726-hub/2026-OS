#ifndef KERNEL_UART_H
#define KERNEL_UART_H

#include "types.h"

void uartinit(void);
void uartputc_sync(int c);
int uartgetc(void);
void uart_intr_enable(void);
void uart_intr(void);
uint64 uart_received(void);

#endif
