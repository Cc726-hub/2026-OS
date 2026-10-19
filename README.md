# Lab 3：中断异常初步

本实验根据课程 `lab-3.pptx`，在 Lab 2 的 OpenSBI 多核启动和 Sv39 内核页表基础上建立 S-mode 陷阱系统，实现串口中断回显与时钟中断。实现位于个人仓库的 `lab3` 分支。

前序实验文档：[Lab 0](lab0/README.md)、[Lab 1](lab1/README.md)、[Lab 2](lab2/README.md)。本实验继续使用自己的 `kernel/` 目录，沿用课件中的 `trap_kernel_init`、`trap_kernel_inithart`、`kernel_vector`、`trap_kernel_handler`、`external_interrupt_handler`、`uart_intr` 等接口名称。

## 完成内容

- 建立直接模式的内核陷阱入口，保存并恢复全部整数寄存器，以及 `sepc`、`sstatus`。
- 配置 QEMU virt 的 PLIC，处理 UART IRQ 10 的 claim / complete。
- 开启 UART 接收中断，读取 FIFO 中的全部字符并回显，回车转换为换行。
- 使用 SBI TIME 为每个 hart 设置约 100 ms 的定时事件，处理中断后安排下一次事件。
- 使用主启动 hart 维护系统 `ticks`，每累计 10 次打印一次计时信息。
- 保留非法指令、缺页等异常的诊断信息，输出 hart、`scause`、`sepc`、`stval` 后停止该 hart。
- 验证多核启动、串口重复中断、每核定时器、寄存器恢复和自旋锁的中断状态恢复。

## 编译与运行

在装有 RISC-V GCC、QEMU 和 Python 3 的 Linux / WSL 中进入仓库：

```bash
make
make run
```

Windows PowerShell 可以直接调用现有 Ubuntu：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/e/操作系统lab/2026-OS && make run'
```

默认双核，支持 `make run CPUS=1`、`CPUS=4`、`CPUS=8`。内存固定为 `128M`，与 Lab 2 的内存映射一致。退出 QEMU：按 Ctrl+A，松开后按 X。

启动后每个 hart 输出分页和中断初始化信息。键盘输入会立即回显，主程序使用 `wfi` 等待，无需循环轮询 UART。大约每秒出现：

```text
timer: ticks=10 (~1 s)
timer: ticks=20 (~2 s)
```

串口回显和计时信息共享控制台，按键过程中可能插入完整的计时输出。这里仅实现字符回显，尚无输入行编辑、命令解析或用户进程。

## 初始化顺序

主核只执行一次全局初始化：

```text
OpenSBI -> _entry -> start -> main
UART / printf -> pmem_init -> kvm_init -> kvm_inithart
trap_kernel_init -> trap_kernel_inithart -> uart_intr_enable
SBI HSM 启动从核 -> release 发布 started -> intr_on -> wfi
```

从核等待 `started` 后启用自己的分页和中断：

```text
OpenSBI HSM -> secondary_entry -> start -> main
acquire 等待 started -> kvm_inithart -> trap_kernel_inithart
intr_on -> wfi
```

全局初始化设置 UART 中断源的 PLIC 优先级，初始化系统计时锁。每核初始化设置自己的 `stvec`、PLIC S-mode 中断使能位与阈值、首次定时事件，以及 `sie.SEIE` 和 `sie.STIE`。最后打开 `sstatus.SIE`，保证不会在入口或共享状态尚未就绪时处理中断。

主启动 hart 使用 OpenSBI 提供的编号，不假定其为 0。共享状态通过 release / acquire 发布，每个 hart 使用独立的 16 KiB 启动栈和自己的中断寄存器。

## 陷阱进入与返回

中断是异步事件，异常由当前指令同步引起。硬件进入 S-mode 陷阱时会设置 `sepc`、`scause`、`stval`，更新 `sstatus.SPP`、`SPIE`、`SIE`，跳转到 `stvec`。硬件不会自动保存全部通用寄存器，需要入口汇编处理。

`kernel_vector.S` 在当前内核栈上申请 288 字节的陷阱帧，维持 C ABI 要求的 16 字节对齐：

| 字节偏移 | 内容 |
| --- | --- |
| 0～255 | `x[0]`～`x[31]`，`x[2]` 保存陷入前的 `sp` |
| 256 | `sepc` |
| 264 | `sstatus` |
| 272 | `stval` |
| 280 | `scause` |

保存现场后把帧地址传给 `trap_kernel_handler`。处理函数要求陷阱来自 S-mode，且处理期间全局中断关闭。`scause` 最高位表示中断，其余位表示原因：5 是 S-mode 时钟中断，9 是 S-mode 外部中断。

返回时恢复原来的 `sepc`、`sstatus` 和整数寄存器，最后执行 `sret`。**中断返回不对 `sepc` 加 4**。硬件已记录正确的恢复地址，压缩指令也可能只有 2 字节；统一加 4 会跳过正常指令。异常是否需要重试、跳过或终止要根据原因决定，本实验的异常统一打印诊断后停止。

当前执行流均位于内核，本章不实现 U-mode 栈切换、进程陷阱帧或嵌套中断。内核和处理函数只使用整数运算；浮点及向量上下文管理留到需要它们的执行环境中实现。

## 串口中断

Lab 2 已把 UART 和 PLIC 映射为内核可读写 MMIO。本章直接使用这些映射，不新增 CLINT 映射。

QEMU virt 的 UART IRQ 为 10，PLIC 优先级设为 1，每核 S-mode 阈值设为 0，使该中断源可以到达全部 hart。地址计算与 [xv6 的 PLIC 实现](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/plic.c)一致：

| 寄存器 | 地址 |
| --- | --- |
| UART 中断优先级 | `0x0c000000 + 10 * 4` |
| hart 的 S-mode enable | `0x0c002080 + hart * 0x100` |
| hart 的 S-mode threshold | `0x0c201000 + hart * 0x2000` |
| hart 的 S-mode claim / complete | `threshold + 4` |

处理路径：

```text
键盘输入 -> UART 接收 FIFO -> PLIC -> S-mode 外部中断
kernel_vector -> trap_kernel_handler -> external_interrupt_handler
plic_claim -> uart_intr -> plic_complete -> 恢复现场 -> sret
```

`plic_claim()` 返回 0 时表示没有可领取的请求，直接返回，不写 complete。多个 hart 收到通知时，也可能已有另一个 hart 先领取请求。对于非零 IRQ，必须在处理结束后 complete，否则后续同一中断源无法正常再次通知。

`uart_intr()` 循环读取接收 FIFO，直到 `uartgetc()` 返回 -1，确保本次已到达的字符全部处理。每个字符通过带锁的 `printf` 输出，避免和其他 hart 的输出竞争。只开启接收中断，发送仍然调用同步 UART 输出，不启用发送空闲中断。

## 时钟中断

QEMU virt 的时间基准为 10 MHz，本次运行的 OpenSBI 输出也确认了 `aclint-mtimer @ 10000000Hz`。这表示 `time` 每秒增加约一千万，与 CPU 指令执行频率不同。本章设定 `TIMER_INTERVAL = 1000000`，即约 100 ms。

`sbi_set_timer(r_time() + TIMER_INTERVAL)` 使用 TIME 扩展的 EID `0x54494d45`、FID 0。参数是绝对时间，设为未来时间会清除挂起的定时中断并安排新事件，见 [RISC-V SBI TIME 规范](https://github.com/riscv-non-isa/riscv-sbi-doc/blob/master/src/ext-time.adoc)。底层定时器由 OpenSBI 管理，内核不直接访问 `mtime` / `mtimecmp`，也不自行编写 M-mode 定时器入口。

S-mode 处理 `scause = interrupt | 5`，无需把它当成软件中断。每次处理先设定下一次绝对时间，再递增该 hart 的中断计数。只有 `boot_hart` 在 `ticks_lock` 内增加系统 `ticks`，因此四核运行不会把系统计时放大为四倍。

本实现按“当前时间 + 间隔”重置定时事件。长时间关闭中断会推迟处理，并且不补记已经错过的周期，所以 `ticks / 10` 是实验用近似计时，精确经过时间应读取 `time`。计时打印在释放 `ticks_lock` 后进行，缩短共享锁的持有时间。

## 中断与自旋锁

串口回显和时钟输出都可能在中断上下文调用 `printf`。Lab 1 的 `acquire` 会先关闭本核中断，避免同一 hart 正在持有打印锁时被中断再次申请该锁而自锁。其他 hart 仍可处理中断，通过原子操作竞争共享锁。

`push_off` / `pop_off` 按 hart 记录嵌套深度。仅最后一层锁释放时恢复最外层记录的中断状态。进入处理函数时 SIE 已关闭，处理中使用锁也不会意外开启嵌套中断；原执行流的中断状态由 `sret` 恢复。

## 自动测试与实测结果

默认双核测试：

```bash
make test
```

完整验证 1、2、4、8 核：

```bash
python3 tests/qemu_lab3.py --cpus 1 2 4 8
```

Lab 2 回归测试：

```bash
make test-lab2
```

测试分别使用 `build/lab3-tests/` 和 `build/tests/` 下的独立构建，不覆盖正常的 `build/kernel.elf`。每个目录保存构建日志、启动日志和 `results.json`。正常编译不包含测试执行逻辑，`LAB3_TEST=1` 时才启用寄存器和锁检查。`LAB2_TEST=1` 时沿用早期异常入口，不打开新中断，以保持前序内存测试的条件。

2026-10-13，使用现有 WSL Ubuntu、RISC-V GCC 和 QEMU 8.2.2 实测，**Lab 3 的 10 项检查全部通过**：

| 核数 | 启动、重复回显、系统计时 | 整数寄存器、嵌套锁、每核定时器、PLIC | 上下文测试主核 |
| --- | --- | --- | --- |
| 1 | 通过 | 通过 | hart 0 |
| 2 | 通过 | 通过 | hart 1 |
| 4 | 通过 | 通过 | hart 3 |
| 8 | 通过 | 通过 | hart 0 |

每种核数执行 2 项测试，另有非法指令和未映射写入两项独立异常测试。双核和四核实测主核不是 0，仍然正常启动和计时。

此外，双核 Lab 2 回归测试的 24 项检查全部通过，包括正常启动、内存与分页，以及 22 项预期失败用例。本次合计 34 项 QEMU 检查通过。

寄存器检查使用汇编给整数寄存器设置不同值，保留有效的 `sp`、hart 编号 `tp` 和结果指针，经过真实定时中断后核对全部 `x[0]`～`x[31]`。锁检查在持有两层锁期间等待三个定时周期，确认本核没有执行定时处理，再验证内层释放、最外层释放和原本关闭中断的状态。每个 hart 至少处理五次定时中断。串口测试分批输入三组文本，核对回显，检查同一 IRQ 完成后还能再次触发；上下文测试另检查收到一个字符且 PLIC 完整处理过该请求。

双核实际输出节选：

```text
PASS hart 1: register restore, nested locks, timer=5
PASS hart 0: register restore, nested locks, timer=5
LAB3 TEST PASS cpus=2 ticks=6 boot_hart=1
LAB3 UART READY
Z
LAB3 UART PASS received=1 external=1
```

非法指令输出 `scause=2`，未映射的固件地址写入输出 `scause=15` 和 `stval=0x0000000080000000`，随后进入 `panic`。不同构建下 `sepc` 地址和各核输出顺序可以变化。

## 文件对应关系

| 文件 | 内容 |
| --- | --- |
| `kernel/kernel_vector.S` | 内核陷阱现场保存、恢复和 `sret` |
| `kernel/trap.h`、`kernel/trap.c` | 陷阱帧、全局/每核初始化、中断分派 |
| `kernel/plic.h`、`kernel/plic.c` | PLIC 优先级、使能、阈值、claim / complete |
| `kernel/timer.h`、`kernel/timer.c` | 定时事件、每核统计、系统 ticks |
| `kernel/sbi.h`、`kernel/sbi.c` | 增加 SBI TIME 调用 |
| `kernel/uart.h`、`kernel/uart.c` | 接收中断开关和 FIFO 回显 |
| `kernel/riscv.h`、`kernel/memlayout.h` | CSR 定义和 PLIC / 时间基准参数 |
| `kernel/main.c` | 接入多核陷阱与定时器初始化 |
| `kernel/lab3_test.c`、`kernel/lab3_register_test.S` | 条件编译的内核测试 |
| `tests/qemu_lab3.py` | 自动构建、QEMU 输入、结果校验和日志 |
| `lab2/README.md` | 归档前序实验文档 |

Lab 1 提供安全的多核启动、输出和锁，Lab 2 提供内存分配及设备映射，Lab 3 提供安全打断并恢复执行流的机制。这些功能为后续用户进程、系统调用和调度建立基础。
