#include "types.h"
#include "uart.h"

#define UART0 0x10000000UL

#define UART_RHR 0
#define UART_THR 0
#define UART_IER 1
#define UART_FCR 2
#define UART_LCR 3
#define UART_LSR 5

#define UART_LCR_EIGHT_BITS 3
#define UART_LCR_BAUD_LATCH (1U << 7)
#define UART_FCR_FIFO_ENABLE (1U << 0)
#define UART_FCR_FIFO_CLEAR  ((1U << 1) | (1U << 2))
#define UART_LSR_DATA_READY  (1U << 0)
#define UART_LSR_TX_IDLE     (1U << 5)

static volatile uint8 *
uart_register(uint32 offset)
{
    return (volatile uint8 *)(UART0 + offset);
}

static void
uart_write(uint32 offset, uint8 value)
{
    *uart_register(offset) = value;
}

static uint8
uart_read(uint32 offset)
{
    return *uart_register(offset);
}

void
uartinit(void)
{
    /* Use polling in Lab 1, so UART interrupts remain disabled. */
    uart_write(UART_IER, 0x00);

    /* Set the baud-rate divisor to 3, then select 8 data bits. */
    uart_write(UART_LCR, UART_LCR_BAUD_LATCH);
    uart_write(0, 0x03);
    uart_write(1, 0x00);
    uart_write(UART_LCR, UART_LCR_EIGHT_BITS);

    /* Enable and clear the transmit and receive FIFOs. */
    uart_write(UART_FCR, UART_FCR_FIFO_ENABLE | UART_FCR_FIFO_CLEAR);
}

void
uartputc_sync(int c)
{
    while ((uart_read(UART_LSR) & UART_LSR_TX_IDLE) == 0) {
        __asm__ volatile("nop");
    }
    uart_write(UART_THR, (uint8)c);
}

int
uartgetc(void)
{
    if ((uart_read(UART_LSR) & UART_LSR_DATA_READY) == 0) {
        return -1;
    }
    return uart_read(UART_RHR);
}
