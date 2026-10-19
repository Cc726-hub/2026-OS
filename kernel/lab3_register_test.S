#if LAB3_TEST
    .section .text
    .balign 4
    .globl lab3_register_test
lab3_register_test:
    # Preserve the caller's ABI state; fill registers with distinct sentinels.
    addi sp, sp, -144
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd s0, 16(sp)
    sd s1, 24(sp)
    sd s2, 32(sp)
    sd s3, 40(sp)
    sd s4, 48(sp)
    sd s5, 56(sp)
    sd s6, 64(sp)
    sd s7, 72(sp)
    sd s8, 80(sp)
    sd s9, 88(sp)
    sd s10, 96(sp)
    sd s11, 104(sp)
    mv t6, a0
    sd sp, 256(t6)
    sd sp, 264(t6)
    li ra, 0x101
    li gp, 0x103
    # tp must remain the hart id because the interrupt handler uses it.
    li t0, 0x105
    li t1, 0x106
    li t2, 0x107
    li s0, 0x108
    li s1, 0x109
    li a0, 0x10a
    li a1, 0x10b
    li a2, 0x10c
    li a3, 0x10d
    li a4, 0x10e
    li a5, 0x10f
    li a6, 0x110
    li a7, 0x111
    li s2, 0x112
    li s3, 0x113
    li s4, 0x114
    li s5, 0x115
    li s6, 0x116
    li s7, 0x117
    li s8, 0x118
    li s9, 0x119
    li s10, 0x11a
    li s11, 0x11b
    li t3, 0x11c
    li t4, 0x11d
    li t5, 0x11e
    wfi
    # Capture all registers before using any of them as scratch registers.
    sd zero, 0(t6)
    sd ra, 8(t6)
    sd sp, 16(t6)
    sd gp, 24(t6)
    sd tp, 32(t6)
    sd t0, 40(t6)
    sd t1, 48(t6)
    sd t2, 56(t6)
    sd s0, 64(t6)
    sd s1, 72(t6)
    sd a0, 80(t6)
    sd a1, 88(t6)
    sd a2, 96(t6)
    sd a3, 104(t6)
    sd a4, 112(t6)
    sd a5, 120(t6)
    sd a6, 128(t6)
    sd a7, 136(t6)
    sd s2, 144(t6)
    sd s3, 152(t6)
    sd s4, 160(t6)
    sd s5, 168(t6)
    sd s6, 176(t6)
    sd s7, 184(t6)
    sd s8, 192(t6)
    sd s9, 200(t6)
    sd s10, 208(t6)
    sd s11, 216(t6)
    sd t3, 224(t6)
    sd t4, 232(t6)
    sd t5, 240(t6)
    sd t6, 248(t6)
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld s0, 16(sp)
    ld s1, 24(sp)
    ld s2, 32(sp)
    ld s3, 40(sp)
    ld s4, 48(sp)
    ld s5, 56(sp)
    ld s6, 64(sp)
    ld s7, 72(sp)
    ld s8, 80(sp)
    ld s9, 88(sp)
    ld s10, 96(sp)
    ld s11, 104(sp)
    addi sp, sp, 144
    ret
#endif
