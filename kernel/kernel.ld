OUTPUT_ARCH(riscv)
ENTRY(_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* OpenSBI occupies the firmware area beginning at 0x80000000. */
    . = 0x80200000;
    _kernel_start = .;

    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } :text

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(4096);
    .data : {
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data

    . = ALIGN(16);
    _stack_start = .;
    .stack (NOLOAD) : {
        *(.bss.stack)
    } :data
    _stack_end = .;

    . = ALIGN(16);
    _bss_start = .;
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    } :data
    . = ALIGN(8);
    _bss_end = .;

    . = ALIGN(4096);
    _kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note .note.*)
    }
}
