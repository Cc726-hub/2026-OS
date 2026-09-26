#ifndef KERNEL_UART_H
#define KERNEL_UART_H

void uartinit(void);
void uartputc_sync(int c);
int uartgetc(void);

#endif
