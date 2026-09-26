#include <stdarg.h>

#include "printf.h"
#include "spinlock.h"
#include "types.h"
#include "uart.h"

static struct {
    struct spinlock lock;
    int locking;
} printer;

static const char digits[] = "0123456789abcdef";

static void
print_character(int c)
{
    if (c == '\n') {
        uartputc_sync('\r');
    }
    uartputc_sync(c);
}

static void
print_integer(int64 value, uint32 base, int is_signed)
{
    char buffer[32];
    uint32 count = 0;
    uint64 number;
    int negative = 0;

    if (is_signed && value < 0) {
        negative = 1;
        number = (uint64)(-(value + 1)) + 1;
    } else {
        number = (uint64)value;
    }

    do {
        buffer[count++] = digits[number % base];
        number /= base;
    } while (number != 0);

    if (negative) {
        buffer[count++] = '-';
    }

    while (count > 0) {
        print_character(buffer[--count]);
    }
}

static void
print_pointer(uint64 address)
{
    print_character('0');
    print_character('x');

    for (int shift = 60; shift >= 0; shift -= 4) {
        print_character(digits[(address >> shift) & 0x0f]);
    }
}

void
printfinit(void)
{
    initlock(&printer.lock, "printf");
    printer.locking = 1;
}

void
printf(const char *format, ...)
{
    va_list arguments;

    if (printer.locking) {
        acquire(&printer.lock);
    }

    va_start(arguments, format);

    for (const char *p = format; *p != '\0'; p++) {
        if (*p != '%') {
            print_character(*p);
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        switch (*p) {
        case 'd':
            print_integer(va_arg(arguments, int), 10, 1);
            break;
        case 'x':
            print_integer((uint32)va_arg(arguments, unsigned int), 16, 0);
            break;
        case 'p':
            print_pointer((uint64)va_arg(arguments, void *));
            break;
        case 'c':
            print_character(va_arg(arguments, int));
            break;
        case 's': {
            const char *text = va_arg(arguments, const char *);
            if (text == 0) {
                text = "(null)";
            }
            while (*text != '\0') {
                print_character(*text++);
            }
            break;
        }
        case '%':
            print_character('%');
            break;
        default:
            print_character('%');
            print_character(*p);
            break;
        }
    }

    va_end(arguments);

    if (printer.locking) {
        release(&printer.lock);
    }
}
