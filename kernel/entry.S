    #include "param.h"

    .section .text.entry
    .globl _entry, secondary_entry

_entry:
    # OpenSBI enters S-mode with a0 = hart id and a1 = DTB address.
    csrci sstatus, 2
    csrw sie, zero
    li t0, NCPU
    bgeu a0, t0, .Lpark

    # Only the cold-boot hart clears BSS. HSM-started harts skip this step.
    la t0, _bss_start
    la t1, _bss_end
.Lclear_bss:
    bgeu t0, t1, .Lbss_ready
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss
.Lbss_ready:
    la t0, boot_hart
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)
    j .Lstack

secondary_entry:
    # SBI HSM passes a0 = hart id and a1 = the supplied opaque value.
    csrci sstatus, 2
    csrw sie, zero
    li t0, NCPU
    bgeu a0, t0, .Lpark

.Lstack:
    mv tp, a0
    # Each hart gets its own 16 KiB stack, matching the course framework.
    la sp, _stack_start
    addi t1, a0, 1
    slli t0, t1, STACK_SHIFT
    add sp, sp, t0
    andi sp, sp, -16

    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    call start

.Lpark:
    wfi
    j .Lpark

    .section .text
    .balign 4
    .globl early_trap
early_trap:
    csrci sstatus, 2
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    tail early_trap_report

    .section .bss.stack,"aw",@nobits
    .balign 16
    .space NCPU * STACK_SIZE
