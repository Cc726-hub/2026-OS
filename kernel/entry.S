    .section .text.entry
    .globl _entry

    .equ NCPU, 8
    .equ STACK_SIZE, 4096

_entry:
    # Every hart starts here in machine mode. Keep the hart id in tp so C code
    # can identify the current CPU after switching to supervisor mode.
    csrr a0, mhartid
    li t0, NCPU
    bgeu a0, t0, .Lpark
    mv tp, a0

    # Give each hart its own 4 KiB stack. Stacks grow toward lower addresses.
    la sp, _stack_start
    addi t1, a0, 1
    li t0, STACK_SIZE
    mul t0, t0, t1
    add sp, sp, t0
    andi sp, sp, -16

    call start

.Lpark:
    wfi
    j .Lpark

    .section .bss.stack,"aw",@nobits
    .balign 16
    .space NCPU * STACK_SIZE
