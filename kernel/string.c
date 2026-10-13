#include "string.h"

void *
memset(void *dst, int value, size_t len)
{
    unsigned char *p = dst;
    while (len-- != 0)
        *p++ = (unsigned char)value;
    return dst;
}
