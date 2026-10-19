#include "sbi.h"

#define SBI_EXT_HSM 0x48534dUL
#define SBI_HSM_HART_START 0
#define SBI_EXT_TIME 0x54494d45UL

long
sbi_hart_start(uint64 hartid, uint64 entry_pa, uint64 opaque)
{
    register uint64 a0 __asm__("a0") = hartid;
    register uint64 a1 __asm__("a1") = entry_pa;
    register uint64 a2 __asm__("a2") = opaque;
    register uint64 a6 __asm__("a6") = SBI_HSM_HART_START;
    register uint64 a7 __asm__("a7") = SBI_EXT_HSM;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a2), "r"(a6), "r"(a7)
                     : "memory");

    return (long)a0;
}

long
sbi_set_timer(uint64 deadline)
{
    register uint64 a0 __asm__("a0") = deadline;
    register uint64 a1 __asm__("a1");
    register uint64 a6 __asm__("a6") = 0;
    register uint64 a7 __asm__("a7") = SBI_EXT_TIME;

    __asm__ volatile("ecall"
                     : "+r"(a0), "=r"(a1)
                     : "r"(a6), "r"(a7)
                     : "memory");
    return (long)a0;
}
