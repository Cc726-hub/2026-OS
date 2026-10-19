# Lab 2：内存管理初步

在 Lab 1 的 OpenSBI 多核启动、UART、格式化输出和自旋锁基础上，本实验增加物理页管理与 Sv39 内核虚拟内存。实现位于个人仓库的 `lab2` 分支，使用 QEMU virt、128 MiB RAM，内核从 `0x80200000` 开始运行。

实验要求参考[教师 Lab 2 指导书](https://github.com/2018wzh/ecnu-oslab-2026/blob/lab-2/README.md)和本地 Lab 2 课件。仓库延续自己的 `kernel/` 结构，内存接口与教师框架一致。Lab 1 文档归档到 [lab1/README.md](../lab1/README.md)，Lab 0 文档见 [lab0/README.md](../lab0/README.md)。

## 完成内容

- 两个带哨兵的单链表分别管理内核页和普通数据页，每页 4 KiB，两池分别加锁。
- 前 1024 个可分配页属于内核池，其余属于普通数据池；分配时清零整页。
- 检查释放地址的对齐、边界、所属池和重复释放；耗尽及非法操作触发 `panic`。
- 实现 Sv39 三级页表查找、映射、权限更新、解除映射、页表打印。
- 实现内核直接映射和每个 hart 的分页启用。
- 独立测试入口验证多核并发、计数恢复、实际 MMU 翻译和页权限异常。

## 编译、运行和测试

在已安装 RISC-V GCC、QEMU 和 Python 3 的 Linux / WSL 中，进入本仓库执行：

```bash
make
make run
```

Windows PowerShell 可直接使用现有 WSL：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/e/操作系统lab/2026-OS && make run'
```

退出 QEMU：按 Ctrl+A，松开后按 X。默认双核，可用 `make run CPUS=4` 改为四核，支持 1～8 核。内存保持 `128M`，与 `memlayout.h` 的静态配置一致。

自动验证默认双核及全部异常用例：

```bash
make test
```

完整验证 1、2、4、8 核及全部异常用例：

```bash
python3 tests/qemu_lab2.py --cpus 1 2 4 8
```

只执行双核正常功能测试：

```bash
python3 tests/qemu_lab2.py --cpus 2 --skip-negative
```

测试使用 `build/tests/` 下的独立构建，日志及汇总保存在该目录，不覆盖正常的 `build/kernel.elf`。手动查看重复释放诊断可执行 `make run LAB2_TEST=1 TEST_CASE=4 CPUS=1`；恢复正常启动使用 `make run LAB2_TEST=0 TEST_CASE=0`。构建配置变化会自动重编译，无需先清理。

## 物理页管理

以下区间均为左闭右开，链接边界按 4096 字节对齐：

| 区间 | 用途 |
| --- | --- |
| `0x80000000` ～ `0x80200000` | OpenSBI 固件保留区，不参与分配 |
| `_kernel_start` ～ `text_end` | 内核代码 |
| `text_end` ～ `rodata_end` | 内核只读数据 |
| `rodata_end` ～ `kernel_end` | 可写数据、每核启动栈、BSS |
| `kernel_end` ～ `kernel_end + 1024 × 4096` | 内核页池，包括页表页 |
| 内核池末尾 ～ `0x88000000` | 普通数据页池，排除设备树占用页 |

QEMU 把启动设备树放在 RAM 中，本次测试的地址为 `0x87e00000`。分配器读取 DTB 头中的长度，保留覆盖它的页面，避免破坏后续实验需要的启动信息。因此普通池页数略少于整个区间的页数。这里仅保留启动 DTB，完整设备树保留表和动态硬件发现尚未实现。

每个空闲页的开头保存 `next` 指针，哨兵 `list_head` 位于常驻管理结构。申请时在池锁内摘除首节点、减少 `allocable`，解锁后清零已独占的页面。释放时在池锁内检查状态、填充调试数据、插入首节点并增加计数。链表申请/释放为 O(1)，整页清零另需处理 4096 字节。

额外的 4 KiB 位图记录空闲状态，用来发现重复释放。两池可能共用某个位图字，因此采用 RISC-V 支持的 32 位原子位操作，避免不同池锁下的并发更新丢失。

```c
void pmem_init(void);                 // 只在主核执行一次
void *pmem_alloc(bool kernel);        // true 内核池；false 普通池；返回清零页
void pmem_free(uint64 pa, bool kernel);
uint32 pmem_available(bool kernel);   // 加锁读取当前空闲数量
```

内存耗尽按教师要求触发 `panic`。释放者必须已停止使用该页，不能释放仍被其他 CPU、页表或别名引用的页面。

## Sv39 页表

本实验使用 Sv39 低半区：`0 <= va < VA_MAX`，`VA_MAX = 1UL << 38`。三级索引来自 VA 的 `[38:30]`、`[29:21]`、`[20:12]`，页内偏移为低 12 位。一张页表占 4096 字节，包含 512 个 8 字节 PTE。

`vm_getpte` 从 level 2 向下查找。有效且 R/W/X 全为 0 的非叶 PTE 指向下一级页表；路径缺失且 `alloc=true` 时，从内核池申请清零页。level 0 的叶项记录最终数据页。本章只支持 4 KiB 叶项，遇到上层大页会诊断并停止。

`vm_mappages` 要求 VA/PA 页对齐、长度非零，最后不足一页时覆盖整页。检查 VA 上界、56 位物理地址范围和算术溢出；W 要求同时设置 R，R/X 至少有一个。叶项设置 V/A/D。对同一 VA/PA 可更新权限，改指向另一 PA 时必须先解除旧映射。

`vm_unmappages` 清除叶项；`free_pages=true` 时把普通数据页交回普通池。它按课程约定保留中间页表。额外的 `vm_freewalk` 可以在所有叶项移除后回收非活动页表，便于验证测试不泄漏页表页。

内核叶项不设置 U，映射关系如下：

| 区域 | 映射 | 权限 |
| --- | --- | --- |
| UART `0x10000000`，1 页 | VA = PA | R/W |
| PLIC `0x0c000000`，64 MiB | VA = PA | R/W |
| 内核代码 | VA = PA | R/X |
| `.rodata` 及页尾填充 | VA = PA | R |
| 可写数据、栈、BSS、动态 RAM | VA = PA | R/W |
| OpenSBI 固件区 | 不映射 | — |
| CLINT | 不映射，由 OpenSBI 管理 | — |

代码可以执行但不能写，只读数据不能写，普通数据不能执行。页表页在 RAM 直接映射中可读写；指向下级页表的非叶 PTE 不设置 R/W/X，表示 MMU 继续查找。

## 多核启用与同步

主核：`OpenSBI -> _entry -> start -> main -> UART/printf -> pmem_init -> kvm_init -> kvm_inithart -> SBI HSM 启动从核 -> release 发布 started`。

从核：`OpenSBI HSM -> secondary_entry -> start -> main -> acquire 等待 started -> kvm_inithart`。

主核编号不固定为 0。只有主核初始化分配器和共享页表，每个 hart 都要设置自己的 `satp = (8UL << 60) | (root_pa >> 12)` 并执行 `sfence.vma`。直接映射保证分页前后的 PC、栈、全局变量和 UART 地址可继续使用。

页表修改要求调用者独占操作。当前只刷新本核 TLB，尚未实现跨 CPU 的 TLB shootdown；不能在其他核正使用共享映射时直接改变它。正常启动在发布页表后不再修改，活动映射测试在启动从核之前完成。

## 测试设计与结果

测试代码仅在 `LAB2_TEST=1` 时编译。实际使用现有 WSL Ubuntu、RISC-V GCC 10.5.0、QEMU 8.2.2 执行。2026-10-13 完整运行结果：**30 项全部通过**。

| 核数 | 正常启动 | 物理页、页表与多核测试 | 测试结束内核空闲页 | 测试结束普通空闲页 |
| --- | --- | --- | --- | --- |
| 1 | 通过 | 通过 | 925 | 31125 |
| 2 | 通过 | 通过 | 925 | 31121 |
| 4 | 通过 | 通过 | 925 | 31113 |
| 8 | 通过 | 通过 | 925 | 31096 |

每种核数包含 2 项测试，另有 22 项独立异常测试，总计 30 项。测试版本的静态检查数组占用额外 BSS，空闲普通页数量与正常内核不同。不同核数也改变启动栈和 DTB 大小。内核池初始均为 1024 页，建立内核页表后剩余 925 页，其余 99 页由页表持有。

测试覆盖：

1. 双池各申请/释放 10 页，写入 `0xAA`，验证计数与重新申请后的全页清零。
2. 申请双池全部页面，验证地址唯一、耗尽计数为 0，以及释放后的完整容量恢复。
3. 教师示例的 5 个映射：VA 0、半页长度、2 MiB 边界、1 GiB 边界、低半区最后一页；验证 PA、权限、权限更新、解映射及数据页/页表页回收。
4. `vm_print` 显示修改前后；核对代码、只读区、可写区、UART 和 PLIC 两端的映射，确认固件及 CLINT 未映射。
5. 分页开启后，使用不同 VA 写入普通 RAM 页，再通过直接映射读取。解除后改指向另一个物理页，验证实际 MMU 翻译与 TLB 刷新。
6. 全部 hart 并发申请内核池剩余页，等待所有核完成后检查数据并释放。随后每核对双池各执行 128 轮、每轮 8 页的申请/清零检查/写入/释放，最终计数完全恢复。
7. 独立异常测试：双池耗尽、错误池、重复释放、未对齐释放、固件页、池边界、非法 VA/PA/长度/权限、冲突映射、解除缺失映射。
8. 真实 CPU 访问验证代码页和只读页禁止写入、固件未映射、解除后的别名失效，以及数据页禁止执行。store page fault 为 `scause=15`，instruction page fault 为 `scause=12`。

四核正常启动选过 hart 1，八核正常启动选过 hart 6；四核内存测试由 hart 2 主启动，也正常完成，验证了主核不固定为 0 的情况。

双核测试结束输出：

```text
PASS virtual: boundaries, flags, remap, unmap, MMU alias and recovery
hart 0: Sv39 enabled, satp=0x800000000008026d; shared UART is locked.
hart 1: Sv39 enabled, satp=0x800000000008026d; shared UART is locked.
PASS hart 0: concurrent allocation and Sv39
PASS hart 1: concurrent allocation and Sv39
LAB2 TEST PASS cpus=2 kernel_free=925 user_free=31121
```

以上各核打印顺序可能变化；地址受构建配置影响，复现时以实际输出为准。异常诊断同时输出 hart、`scause`、`sepc` 和 `stval`，便于定位错误。

## 文件对应关系

| 文件 | 内容 |
| --- | --- |
| `kernel/mem.h` | 页表定义、内存类型和课程接口 |
| `kernel/memlayout.h` | QEMU 内存与 MMIO 范围 |
| `kernel/pmem.c` | 双物理页池、锁、清零和释放检查 |
| `kernel/kvm.c` | 页表操作、内核映射和每核启用 |
| `kernel/string.c` | 内核 `memset` |
| `kernel/kernel.ld` | 页对齐的内核边界 |
| `kernel/main.c` | 初始化顺序及多核同步 |
| `kernel/entry.S`、`kernel/printf.c` | 早期异常及 panic 诊断 |
| `kernel/lab2_test.c` | 条件编译的内核测试 |
| `tests/qemu_lab2.py` | 构建、启动、自动校验及日志 |

本次完成 Lab 2 的基础任务。buddy 分配器、内核堆和大页是教师列出的进阶目标，当前未实现。用户进程及用户页表运行环境将在后续实验接入；普通数据池的“用户”表示分配用途，当前还未建立 U-mode 隔离。
