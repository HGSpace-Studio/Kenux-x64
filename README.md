# Kenux OS (KenuxK)

Kenux OS 是一个从零构建的 x86_64 操作系统，采用 UEFI 启动，包含完整的内核、GUI 桌面环境、systemd 初始化系统、容器管理子系统、Win32 兼容层以及 16 个用户态组件。

## 项目概览

```
Kenux-os-coding/
├── bootloader/          # UEFI 引导加载程序 (BOOTX64.EFI)
├── kernel/              # 内核主体
│   ├── arch/x86_64/     # 架构相关代码 (GDT, IDT, ACPI, APIC, 内存管理等)
│   ├── kernel/          # 内核子系统 (进程, 系统调用, IPC, 文件系统等)
│   ├── api/             # KAPI 内核 API 层 (60+ 模块)
│   ├── gui/             # GUI 桌面环境 (窗口管理器, 任务栏, 字体渲染等)
│   ├── systemd/         # systemd 兼容初始化系统
│   ├── lib/libc/        # 内核态 libc 实现
│   └── compat/          # LeonOS 兼容层
├── kal/                 # 内核抽象层 (KAL)
├── apps/                # 内核态应用 (计算器, 日历, 文件管理器, 终端等)
│   └── container-os/    # 容器管理子系统
├── compat_layer/        # Win32/NTVDM/ReactOS 兼容层
├── spatiotemporal/      # 时空分析库
├── src/                 # 用户态组件源码 (16 个组件)
├── bin/                 # 用户态构建脚本与辅助文件
├── build.py             # 主构建系统入口 (Python)
├── kernelbuild.ninja    # Ninja 构建文件
└── build_components.sh  # 用户态组件构建脚本
```

## 核心特性

### 内核架构

- **目标平台**: x86_64 长模式 (Long Mode)
- **启动方式**: UEFI (通过 BOOTX64.EFI 引导)
- **内核入口**: `start64.s` → `kernel_main()`
- **链接地址**: 0x2000000 (32MB)，内核最大 64MB
- **编译模型**: `-mcmodel=large`，freestanding 环境

### 硬件支持

| 类别 | 支持的硬件/协议 |
|------|----------------|
| 中断 | PIC, IOAPIC, APIC, IDT |
| 总线 | PCI, AHCI, SATA, USB (EHCI/XHCI), I2C, SPI, VirtIO |
| 存储 | AHCI, NVMe, ATA/SATA |
| 网络 | E1000, Netfilter, Conntrack, DNS, VLAN, Route |
| 显示 | Framebuffer, VGA, KVM |
| 输入 | 键盘, 鼠标, USB HID |
| 音频 | AC97, HDA, PC 扬声器 |
| 电源 | ACPI, ACPI-MADT, ACPI-PM, CPUFreq, CPUIdle, Thermal |
| 固件 | EFI, EFI Runtime, SMBIOS |
| 其他 | HPET, PIT, RTC, UART, Watchdog, IOMMU |

### 内存管理

- **Buddy 分配器**: 物理页帧管理
- **SLAB 分配器**: 小对象高效分配
- **VMA 管理**: 内核虚拟地址空间管理
- **页面缓存**: 文件缓存支持
- **Swap**: 交换分区支持

### 进程与调度

- 进程创建/销毁/信号处理
- CFS (Completely Fair Scheduler) 调度器
- 内核线程 (kthread) 支持
- 工作队列 (workqueue) 支持
- 用户态切换支持

### 文件系统

- ext2, ext4 (含 jbd2 日志)
- NTFS (只读)
- devtmpfs, tmpfs, procfs, sysfs
- VFS 抽象层

### 安全

- LSM (Linux Security Module) 框架
- SELinux 风格强制访问控制
- eBPF 支持
- 容器命名空间与 cgroup
- 沙箱机制

### GUI 桌面环境

Kenux 拥有自研的内核态 GUI 桌面环境：

- **窗口管理器**: 窗口创建/销毁/移动/缩放/焦点管理
- **桌面**: 左侧垂直胶囊任务栏 + 三列开始菜单
- **主题**: Classic / Metro / Material Design 3 三种主题
- **色彩方案**: Blue, Teal, Green, Purple, Red, Graphite
- **字体渲染**: 内嵌 TTF 字体引擎 (stb_truetype), CJK 字体, Emoji 支持
- **组件**: 浏览器, 终端, 文件管理器, 系统信息, 任务管理器, 文本编辑器
- **图标系统**: 图标注册表与内嵌图标数据
- **壁纸/Logo**: 内嵌壁纸与启动 Logo

### systemd 初始化系统

内核内置 systemd 兼容层，支持以下单元类型：

- Service, Target, Socket, Timer, Mount, Slice
- Device, Automount, Path, Swap
- systemctl / journalctl 接口
- udev 设备管理

### KAPI 内核 API

60+ 个内核 API 模块，涵盖：

- 数据结构: list, rbtree, bitmap, idr, kfifo, mempool, sort, crc, hash
- 同步: mutex, spinlock, completion, wait, RCU, notifier
- 内存: memory, mempool, percpu, mmap, dma, iommu
- 进程: process, kthread, sched, smp, syscall
- 设备: device, cdev, blkdev, pci, irq, kobject, module, params
- 文件系统: fs, vfs, seq_file, debugfs
- 网络: netdevice, socket, netlink, skbuff
- 安全: security, ebpf, kprobe, ftrace
- 其他: io, time, random, atomic, cpumask, profiler, debugger

### KAL (内核抽象层)

提供统一的内核抽象接口：

- **Capability**: 能力模型
- **Governance**: 治理层
- **SSI**: 稳定系统调用接口
- **Registry**: 后端注册表
- **Dispatcher**: 调度分发
- **Adapters**: Linux-like / Microkernel / Kenux 适配器

### 容器管理

内核态容器管理子系统 (container-os)：

- 容器创建/启动/停止/销毁
- 镜像管理
- 卷管理
- 资源限制 (CPU/内存)
- 容器日志

### Win32 兼容层[默哀Invitedivineprincess]

自研 的 NTVDM 兼容层：

- Win32 API 子系统 (kernel32, ntdll, advapi32, gdi, shell)
- DOS/VDM 虚拟化
- 应用兼容性数据库 (SDB)
- Shim 引擎

### 用户态组件

16 个用户态组件（宿主编译，非 freestanding）：

| 组件 | 说明 |
|------|------|
| bash | Shell |
| fastfetch | 系统信息快速显示 |
| git | 版本控制 |
| make | 构建工具 |
| gcc / clang | 编译器 |
| ld | 链接器 |
| mkfs | 文件系统创建 |
| dd | 磁盘复制 |
| xorriso | ISO 镜像工具 |
| qemu | 虚拟化 |
| wayland | 显示协议 |
| kde / gnome | 桌面环境 |
| firefox | 浏览器 |
| systemd | 初始化系统 |

## 构建系统

### 前置依赖

| 工具 | 用途 |
|------|------|
| GCC (x86_64) | C 编译器 |
| NASM | 汇编器 |
| objcopy | 二进制转换 |
| Python 3 | 构建系统 |
| Ninja | 增量构建 |
| QEMU | 运行/调试 |

**Linux / WSL 安装示例**：

```bash
# Ubuntu/Debian
sudo apt install gcc nasm binutils python3 ninja-build qemu-system-x86

# Arch Linux
sudo pacman -S gcc nasm binutils python ninja qemu-headless
```

**Windows**：需要 MinGW64、NASM、QEMU，可通过环境变量配置路径：

```bat
set MINGW_BIN=D:\mingw64\bin
set NASM_PATH=C:\Program Files\NASM
set QEMU_PATH=D:\qemu
```

### 使用 build.py (推荐)

`build.py` 是基于 Python 的主构建系统，支持增量构建和依赖追踪：

```bash
# 构建所有内容 (内核 + bootloader + ESP 更新)
python3 build.py run all

# 仅构建内核
python3 build.py run kernel

# 仅构建 UEFI bootloader
python3 build.py run bootloader

# 构建用户态组件
python3 build.py run components

# 构建并启动 QEMU (VGA 窗口)
python3 build.py run run

# 构建并启动 QEMU (无显示, 仅串口日志)
python3 build.py run run-debug

# 清理构建目录
python3 build.py run clean

# 清理并重新构建
python3 build.py run rebuild
```

**其他命令**：

```bash
# 显示帮助
python3 build.py help

# 显示目标信息
python3 build.py info kernel

# 解释为什么需要重建
python3 build.py why kernel

# 显示受影响的文件
python3 build.py affected kernel/kernel/kernel.c

# 构建并显示性能分析
python3 build.py profile all

# 缓存管理
python3 build.py cache stats
python3 build.py cache prune

# 显示依赖图
python3 build.py map
```

### 使用 Ninja (直接)

项目提供了预生成的 Ninja 构建文件：

```bash
# 构建内核
ninja -f kernelbuild.ninja

# WSL 环境
ninja -f kernelbuild_wsl.ninja

# 清理
ninja -f kernelbuild.ninja clean
```

### 使用 Shell 脚本

```bash
# 构建 bootloader
./bootloader/build.sh

# 构建用户态应用
./bin/build.sh

# 构建用户态组件
./build_components.sh
```

### 使用 Makefile

```bash
# 构建用户态应用 (bin/ 目录)
make -C bin

# 构建 bootloader
make -C bootloader

# 构建 KAL
make -C kal
```

## 运行

### QEMU (UEFI 模式)

构建完成后，使用 QEMU 以 UEFI 模式运行：

```bash
python3 build.py run run
```

或手动启动：

```bash
qemu-system-x86_64 \
    -m 256M \
    -pflash OVMF_CODE.fd \
    -drive if=none,id=kenuxesp,file=fat:rw:esp,format=raw \
    -device ahci,id=ahci \
    -device ide-hd,bus=ahci.0,drive=kenuxesp,bootindex=0 \
    -serial file:serial.log \
    -display gtk \
    -no-reboot
```

### 创建可启动 USB

1. 将 USB 格式化为 FAT32
2. 复制 `bootloader/BOOTX64.EFI` 到 `/EFI/BOOT/BOOTX64.EFI`
3. 复制 `img/boot/kernel.bin` 到 `/KENUXK.BIN`

## 内核启动流程

```
UEFI 固件
  └─> BOOTX64.EFI (bootloader/bootx64.c)
        ├─ 通过 EFI Boot Services 定位内核文件
        ├─ 读取内核到 0x2000000
        ├─ 退出 EFI Boot Services
        └─ 跳转到 _start (start64.s)
              ├─ 设置内核栈
              ├─ 传递 framebuffer 配置和内存映射
              └─> kernel_main() (kernel/kernel/kernel.c)
                    ├─ GDT / IDT / PIC 初始化
                    ├─ ACPI / HPET / RTC 初始化
                    ├─ 内存管理初始化 (buddy + slab)
                    ├─ 进程管理初始化
                    ├─ 设备驱动初始化
                    ├─ 文件系统初始化
                    ├─ 网络栈初始化
                    ├─ systemd 初始化
                    ├─ KAPI 初始化
                    ├─ GUI 桌面环境启动
                    └─ 进入调度循环
```

## 编译标志

### 内核编译

```bash
gcc -Wall -Wextra -Wpedantic -Wshadow -Wcast-align -Wconversion \
    -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 \
    -std=c17 -O2 -m64 -mcmodel=large -ffreestanding -fno-pic \
    -nostdlib -nostartfiles -nodefaultlibs \
    -mno-stack-arg-probe -fno-asynchronous-unwind-tables \
    -fno-unwind-tables -DKAL_KERNEL \
    -Iinclude -Ikernel/include -Ikernel/kernel \
    -Ikernel/arch/x86_64/include -Ikernel/lib/libc/include \
    -Ikernel/systemd/include -Iapps/container-os/include \
    -Ikernel/gui/include -Ikernel/compat -Ikal/include
```

### 汇编编译

```bash
nasm -f elf64 source.s -o output.o    # Linux/WSL
nasm -f win64 source.s -o output.o    # Windows
```

### 用户态组件编译

```bash
gcc -Wall -Wextra -std=c17 -O2 -Isrc/xxx src/xxx/xxx.c -o output
```

## 项目结构详解

### kernel/arch/x86_64/ — 架构层

x86_64 架构相关的所有代码，包括：

- `boot/`: 启动入口 (start64.s, boot.S, linker.ld)
- 硬件驱动: ACPI, APIC, AHCI, NVMe, EHCI, XHCI, E1000, VirtIO 等
- 核心: GDT, IDT, 内存管理, 中断处理, 页错误
- 文件系统: ext2, ext4, NTFS
- 网络: Net, Netfilter, Conntrack, DNS, VLAN, Route
- 显示: Framebuffer, VGA, HW Display
- 音频: AC97, HDA, Sound, PC Speaker
- 兼容: Win32 子系统, NTVDM, PE 加载器, Registry
- 电源: ACPI-PM, CPUFreq, CPUIdle, Thermal, Watchdog

### kernel/kernel/ — 内核子系统

- `kernel.c`: 内核主入口与初始化
- `process.c/h`: 进程管理
- `syscall.c/h`: 系统调用
- `fs.c/h`: 文件系统核心
- `ipc.c/h`: 进程间通信
- `sync.c/h`: 同步原语
- `kthread.c/h`: 内核线程
- `module.c/h`: 模块加载
- `cgroup.c/h`: 控制组
- `namespace.c/h`: 命名空间
- `bpf.h`: BPF 支持
- `ftrace.h`: 函数追踪
- `selinux.h`: SELinux
- `gdb_stub.c/h`: GDB 远程调试

### kernel/api/ — KAPI 层

60+ 个内核 API 模块，为内核和驱动提供标准化接口。

### kernel/gui/ — GUI 桌面环境

- `src/desktop.c`: 桌面与任务栏绘制
- `src/window_manager.c`: 窗口管理器
- `src/framebuffer.c`: 帧缓冲抽象
- `src/font.c`, `font_ttf.c`, `cjk_font.c`: 字体渲染
- `src/graphics.c`: 图形绘制原语
- `src/browser.c`, `terminal.c`, `filemgr.c`: 内置应用
- `src/kenux_ui.c`: Kenux UI 抽象层 (Surface/Theme/Widget)
- `src/msf.c`: Material Style Framework 主题引擎

### kernel/systemd/ — systemd 兼容层

- `systemd.c`: 主初始化
- `service.c`, `target.c`, `socket.c`, `timer.c`, `mount.c`: 各单元类型
- `systemctl.c`, `journalctl.c`: 管理接口
- `udev.c`: 设备管理
- `journal.c`: 日志系统

### kal/ — 内核抽象层

- `src/`: 核心实现 (capability, governance, SSI, registry, dispatcher, marshalling)
- `adapters/`: 适配器 (Linux-like, Microkernel, Kenux, Custom)
- `include/`: 公共头文件

### compat_layer/ — Win32 兼容层

基于 ReactOS 项目的 NTVDM 兼容实现，支持 16 位 DOS 应用和 Win32 应用兼容。

## 许可证

**Kenux OS Non-Commercial Open Source License (KNCOSL) v1.0**

Copyright (c) 2026 HGSpace-Studio | https://github.com/hgspace-studio

核心条款：

- **非商业使用** — 禁止将本项目代码用于任何商业目的（销售、商业产品/服务、商业组织内部运营等）
- **来源保留** — 禁止更名、换标、重新包装后冒充自有作品；必须保留所有版权声明和归属信息
- **开源传染** — 使用本项目代码的任何项目必须以 OSI 认可的开源许可证发布，且完整源代码必须公开免费提供
- **归属声明** — 使用本项目代码的项目必须在显著位置声明：

  > This project includes code from Kenux OS (KenuxK) by HGSpace-Studio.
  > Original project: https://github.com/hgspace-studio
  > Licensed under the Kenux OS Non-Commercial Open Source License (KNCOSL).

完整许可证文本请参阅 [LICENSE](LICENSE)。