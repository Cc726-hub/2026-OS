# Lab 1：机器启动

本分支从零实现一个最小的 RISC-V 64 位内核，完成课程 Lab 1 的核心目标：从 QEMU 启动进入 C 语言 main 函数，通过 UART 输出格式化信息，并使用自旋锁保护多核共享的 UART。

## 已完成内容

- 使用链接脚本把内核放到物理地址 0x80000000，并指定 _entry 为入口。
- 在 entry.S 中为每个 hart 分配独立的 4 KiB 栈。
- 在 start.c 中设置 RISC-V 控制寄存器，通过 mret 从 M-mode 切换到 S-mode。
- 清空 BSS，并同步多个 hart 的启动过程。
- 通过 MMIO 驱动 QEMU virt 机器上的 16550 UART。
- 实现支持 %d、%x、%p、%c、%s 和 %% 的 printf。
- 实现自旋锁、原子加锁和嵌套关中断。
- 使用两个 hart 验证共享 UART 输出不会互相穿插。

## 目录说明

    Makefile             编译、检查和运行规则
    kernel/kernel.ld     内核内存布局
    kernel/entry.S       最早执行的汇编入口和各 hart 的栈
    kernel/start.c       BSS 初始化和 M-mode 到 S-mode 的切换
    kernel/main.c        Lab 1 主流程和输出测试
    kernel/riscv.h       RISC-V 控制寄存器操作
    kernel/uart.c        UART 轮询驱动
    kernel/printf.c      格式化输出
    kernel/spinlock.c    自旋锁和中断嵌套控制

## 编译与运行

在安装了 RISC-V 交叉编译器和 QEMU 的 Linux 或 WSL 环境中执行：

    make clean
    make
    make run

退出 QEMU：先按 Ctrl+A，松开后按 X。

运行成功时会看到 Hello, OS、printf 格式测试，以及 hart 0 和 hart 1 各自输出的一整行信息。

## 检查 ELF

执行以下命令可以查看 ELF 入口、符号地址并生成反汇编文件 build/kernel.asm：

    make inspect
