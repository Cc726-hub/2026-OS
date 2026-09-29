#include "boot.h"
#include "riscv.h"
#include "types.h"

extern void main(void);

/* Set by the cold-boot assembly entry after clearing BSS. */
uint64 boot_hart;
uint64 boot_dtb;

void
start(uint64 hartid, uint64 opaque)
{
    (void)hartid;
    (void)opaque;

    /* OpenSBI already prepared M-mode state and entered S-mode for us. */
    intr_off();
    w_sie(0);
    w_satp(0);
    sfence_vma();
    w_stvec((uint64)early_trap);

    main();
    for (;;) {
        __asm__ volatile("wfi");
    }
}
