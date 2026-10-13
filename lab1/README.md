# Lab 1：机器启动

本分支从零实现一个最小的 RISC-V 64 位内核，完成课程 Lab 1 的核心目标：从 QEMU 启动进入 C 语言 main 函数，通过 UART 输出格式化信息，并使用自旋锁保护多核共享的 UART。

## 已完成内容

- 使用课程框架的 OpenSBI 启动方式，将内核放到物理地址 0x80200000，并指定 _entry 为入口。
- OpenSBI 完成 M-mode 初始化及进入 S-mode 的工作，内核通过 a0 接收 hart 编号，通过 a1 接收设备树地址。
- 在 entry.S 中为每个 hart 分配独立的 16 KiB 栈；主核清空 BSS 并记录自身编号，从核使用 secondary_entry，跳过 BSS 清零。
- 在 start.c 中初始化 S-mode 状态，设置早期异常入口，保持分页关闭，然后进入 main。
- 主核通过 SBI HSM 接口启动其他 hart，以 release/acquire 原子操作同步共享初始化；主核编号不固定为 0。
- 通过 MMIO 驱动 QEMU virt 机器上的 16550 UART。
- 实现支持 %d、%x、%p、%c、%s 和 %% 的 printf。
- 实现自旋锁、原子加锁和嵌套关中断。
- 使用两个 hart 验证共享 UART 输出不会互相穿插。

## 目录说明

    Makefile             编译、检查和运行规则
    kernel/kernel.ld     内核内存布局
    kernel/entry.S       OpenSBI 冷启动、从核入口、BSS 清零和各 hart 的栈
    kernel/start.c       S-mode 状态初始化和启动信息定义
    kernel/boot.h        启动入口与主核、设备树地址声明
    kernel/param.h       CPU 数量约束和栈大小
    kernel/sbi.c         通过 SBI HSM 启动从核
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

QEMU 使用 `-bios default` 加载 OpenSBI。运行成功时会先看到 OpenSBI 信息，再看到 Hello, OS、主核编号、内核入口和设备树地址、printf 格式测试，以及 hart 0 和 hart 1 各自输出的一整行信息。

可用 `make run CPUS=4` 改为四核运行。CPU 数量同时用于编译和 QEMU 配置，修改后会自动重编译相关代码；支持 1～8 个连续编号的 QEMU hart。

主核启动路径为 `QEMU → OpenSBI → _entry → start → main`。其他 hart 由主核调用 SBI HSM 启动，路径为 `OpenSBI → secondary_entry → start → main`，在 main 中等待共享初始化完成。

## 启动验证

使用现有 WSL Ubuntu、RISC-V GCC 10.5.0 和 QEMU 8.2.2 实际运行，单核、双核、四核和八核均能完成启动。每个 hart 恰好输出一条完整的共享 UART 信息，主核的 Hello, OS 和 printf 格式测试各输出一次。四核测试中 OpenSBI 选择了 hart 1 作为主核，也能正常启动其余三个 hart。

ELF 检查确认入口为 0x80200000，所有 LOAD 段均位于该地址或之后；每核栈大小为 16 KiB，BSS 边界按 8 字节对齐，内核结束地址按 4 KiB 对齐。内核反汇编中不再包含 mhartid、mstatus、mepc、mret 或 PMP 配置等 M-mode 启动操作。

CPU 配置从 1、4、8 切换回默认 2 核后，构建均能自动更新；保持相同配置再次执行 make，不会重复编译内核。

## 与 Lab 2 的衔接

当前内存布局与课程 Lab 2 的 OpenSBI 方案一致：RAM 起点为 0x80000000，内核起点为 0x80200000，前面的区域留给固件。`_kernel_end` 已按 4 KiB 对齐，并覆盖内核代码、数据、启动栈和 BSS，可以用于确定后续物理页分配器的起点。

分页目前仍然关闭；Lab 2 将继续增加物理页分配器、内核页表及每个 hart 的分页启用操作。保存的 `boot_dtb` 是冷启动传入的设备树地址，从核 HSM 的 a1 是 opaque 参数，两者不能混淆。

## 检查 ELF

执行以下命令可以查看 ELF 入口、符号地址并生成反汇编文件 build/kernel.asm：

    make inspect
