# 🔥 KenuxOS vs LeonOS-4 终极对比分析

> 对比日期：2026-08-27  
> 对比范围：KenuxOS（`/Kenux-x64/` 排除 LeonOS-4 目录）vs LeonOS-4（`/Kenux-x64/LeonOS-4/`）

---

## 一、基本规模

| 维度 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **总代码行** | **567,478** | **247,405** |
| **规模比** | **2.3 倍** | 1 |
| **语言** | C / x86 ASM / Python / Shell | C / Rust / x86 ASM |
| **架构** | x86-64 only | x86-64 为主，aarch64/riscv64 预留 |

---

## 二、启动与内核架构

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **Bootloader** | ✅ 自写 UEFI bootloader（`bootloader/`），含 EFI 协议、页表构建、帧缓冲初始化、串口日志、进度条动画 | ✅ GRUB → loader（`boot/`） |
| **内核入口** | ✅ `start64.s` → `kernel_main`，双 ABI 兼容（SysV + ms_abi） | ✅ boot.S → main.c → kernel bootstrap |
| **中断处理** | ✅ `interrupt.S`，完整寄存器保存/恢复 + `interrupt_dispatch` | ✅ IDT + 中断分发 |
| **GDT/IDT** | ✅ `gdt.c` + `idt.c` | ✅ 完整实现 |
| **Ring 隔离** | ✅ `usermode.c` — 用户态进程管理，独立 CR3 页表，Ring 0/3 切换 | ✅ Ring 0 内核 + Ring 3 用户态 |

---

## 三、内存管理

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **Buddy 分配器** | ✅ `buddy.c` — 完整 buddy 系统 | ✅ 物理页分配器 + 引用计数 |
| **SLAB 分配器** | ✅ `slab.c` | ✅ 内核堆 |
| **kmalloc/kfree** | ✅ `memory.c` — SLAB + Buddy 双层分配 | ✅ kernel_malloc/kernel_free |
| **VMA 管理** | ✅ `mmap.c` | ✅ task_vma 动态扩展 |
| **Swap** | ✅ `swap.c` — 完整 swap 区域管理 + bitmap | ❌ 无 |
| **Page Cache** | ✅ `pagecache.c` — LRU + 哈希表 + 脏页追踪 | ✅ page_cache 模块 |

---

## 四、调度器

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **CFS 调度器** | ✅ `cfs.c` — **Linux 风格 CFS**，红黑树，nice-to-weight 映射，完整旋转/插入/删除 | ✅ 轮转调度 + VMA 管理 |
| **进程状态** | ✅ 8 种状态（READY/RUNNING/SLEEPING/WAITING/ZOMBIE/STOPPED/DEAD/UNUSED） | ✅ 类似状态机 |
| **线程支持** | ✅ 每进程 16 线程，全局 1024 线程 | ✅ 内核任务 + 用户态 |
| **SMP** | ✅ `smp.c` + AP trampoline | ✅ APIC/SMP 初始化 |

---

## 五、文件系统

| 文件系统 | KenuxOS | LeonOS-4 |
|---------|---------|----------|
| **FAT32** | ✅ `fat.c` | ✅ FAT32 |
| **ext2** | ✅ `ext2.c` | ✅ ext2 |
| **ext3** | ✅ `ext3.c` | ❌ 无 |
| **ext4** | ✅ `ext4.c` — CRC32c 校验、extent、64bit、flex_bg、metadata_csum | ❌ 无 |
| **exFAT** | ✅ `exfat.c` | ❌ 无 |
| **NTFS** | ✅ `ntfs.c` — MFT 记录、UTF-16、属性解析 | ❌ 无 |
| **ISO 9660** | ✅ `iso9660.c` | ✅ ISO 9660 |
| **JBD2** | ✅ `jbd2.c` — 日志系统 | ❌ 无 |
| **devfs/procfs/tmpfs/pipefs/socketfs** | ✅ 全部有实现 | ✅ 部分 |
| **VFS** | ✅ 统一 VFS 层 | ✅ 统一 VFS 层 |

> 🏆 **KenuxOS 文件系统完胜**：7+ 种文件系统 vs LeonOS 的 3 种，且有 NTFS、ext4、exFAT、JBD2 日志等高级特性。

---

## 六、网络协议栈

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **ARP/IPv4/ICMP/UDP/TCP** | ✅ `net.c` — 完整实现 | ✅ 完整实现 |
| **DHCP** | ✅ | ✅ |
| **DNS** | ✅ `dns.c` | ✅ |
| **IPv6** | ✅ `ipv6.c` — NDP + 校验和 + 地址配置 | ❌ 无 |
| **Netfilter** | ✅ `netfilter.c` — 5 钩子点 + 优先级排序 + hook 注册 | ❌ 无 |
| **Conntrack** | ✅ `conntrack.c` — 连接追踪 + 哈希 + 超时 | ❌ 无 |
| **TCP BBR** | ✅ `tcp_bbr.c` — 完整 BBR 状态机 | ❌ 无 |
| **TCP 拥塞控制框架** | ✅ `tcp_cong.c` | ❌ 无 |
| **VLAN** | ✅ `vlan.c` | ❌ 无 |
| **路由** | ✅ `route.c` | ✅ 基础路由 |

> 🏆 **KenuxOS 网络栈完胜**：IPv6、Netfilter、Conntrack、BBR、VLAN 全部有真实内核实现。

---

## 七、驱动支持

| 驱动 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **AHCI/SATA** | ✅ `ahci.c` | ✅ AHCI |
| **NVMe** | ✅ `nvme.c` — 完整 NVMe 控制器 + 提交/完成队列 | ❌ 无 |
| **IDE** | ✅ `ide.c` | ❌ 无 |
| **e1000** | ✅ `e1000.c` | ✅ e1000 |
| **RTL8139/RTL8169** | ✅ `rtl8139.c` / `rtl8169.c` | ❌ 无 |
| **AC97/HDA** | ✅ `ac97.c` / `hda.c` | ✅ AC97 |
| **USB (EHCI/XHCI)** | ✅ `ehci.c` / `xhci.c` | ✅ USB |
| **VirtIO GPU** | ✅ `virtio_gpu.c` | ❌ 无 |
| **VirtIO** | ✅ `virtio.c` | ❌ 无 |
| **I2C/SPI** | ✅ `i2c.c` / `spi.c` | ❌ 无 |
| **TPM** | ✅ `tpm.c` — FIFO 传输 + TPM 2.0 命令 | ❌ 无 |
| **Watchdog** | ✅ `watchdog.c` | ❌ 无 |
| **KVM/VMX** | ✅ `kvm.c` — **内核级虚拟化**，VMCS/EPT/VPID | ❌ 无 |

> 🏆 **KenuxOS 驱动完胜**：NVMe、RTL 网卡、VirtIO、TPM、KVM 虚拟化等 LeonOS 完全没有。

---

## 八、安全模型

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **SELinux** | ✅ `selinux.c` — **完整 SELinux 实现**：SID 表、AV 表、访问向量缓存、TE 规则、user:role:type:level 上下文 | ❌ 无（仅有账户/ACL） |
| **LSM** | ✅ `lsm.c` — Linux Security Module 框架 | ❌ 无 |
| **TPM** | ✅ 硬件可信平台模块 | ❌ 无 |
| **账户/ACL** | ✅ | ✅ |

> 🏆 **KenuxOS 安全模型完胜**：SELinux + LSM + TPM 是企业级安全特性。

---

## 九、独有高级特性

| 特性 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **eBPF** | ✅ `bpf.c` — 完整 eBPF VM：map 操作、helper 函数、map_lookup/update/delete、ktime_get_ns、prandom | ❌ 无 |
| **KVM 虚拟化** | ✅ 内核级 KVM/VMX — 可在 KenuxOS 内运行虚拟机 | ❌ 无 |
| **Win32 兼容层** | ✅ kernel32/ntdll/advapi32/GDI/msvcrt/shell — **可在内核中运行 Win32 程序** | ❌ 无 |
| **NTVDM** | ✅ `compat_ntvdm.c` — 16 位 DOS 虚拟机 + WOW64 桥接 | ❌ 无 |
| **PowerShell 引擎** | ✅ `powershell.c` — 内核级 PS 脚本解释器 | ❌ 无 |
| **PE 加载器** | ✅ `pe.c` — Windows 可执行文件加载 | ❌ 无 |
| **ELF 加载器** | ✅ `elf.c` + LeonOS 兼容检测 | ✅ ELF 加载 |
| **动态链接器** | ✅ `ldso.c` — 完整 .so 加载 + 符号解析 + 重定位 | ✅ ld-leonos |
| **KAL 抽象层** | ✅ `kal/` — **内核适配层**：多后端路由、熔断器、治理策略、SSI 接口 | ❌ 无 |
| **Swap** | ✅ 完整 swap in/out | ❌ 无 |
| **CPU 调频/空闲** | ✅ `cpufreq.c` / `cpuidle.c` | ❌ 无 |
| **热插拔/PnP** | ✅ `pnp.c` | ❌ 无 |
| **ftrace** | ✅ `ftrace.c` — 内核函数追踪 | ❌ 无 |
| **HDR 色调映射** | ✅ `hdr_tonemap.c` | ❌ 无 |
| **WMI** | ✅ `wmi.c` | ❌ 无 |

---

## 十、用户态生态

| 应用 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **终端** | ✅ `terminal.c` | ✅ PTY 终端 |
| **文件管理器** | ✅ `file_manager.c` | ✅ 文件管理器 |
| **文本编辑器** | ✅ `text_editor.c` | ✅ Nano/Notepad |
| **计算器** | ✅ `calculator.c` | ❌ 无 |
| **日历** | ✅ `calendar.c` | ❌ 无 |
| **网络工具** | ✅ `network_tools.c` | ❌ 无 |
| **系统工具** | ✅ `system_tools.c` | ✅ 任务管理器等 |
| **容器运行时** | ✅ `container-os/` — 完整容器管理（create/start/stop/kill/remove + 镜像/卷管理） | ❌ 无 |
| **桌面管理器** | ✅ `desktop_manager.c` — 任务栏、开始菜单、图标网格、窗口管理 | ✅ 完整桌面 |
| **StardustUI** | ✅ `stardustui_integration.c` — 窗口消息、主题、组件 | ✅ StardustUI |
| **浏览器** | ❌ 无 | ✅ HTTP 浏览器 |
| **Doom** | ❌ 无 | ✅ Doom |
| **C 编译器** | ❌ 无 | ✅ TCC |
| **Lua** | ❌ 无 | ✅ Lua 5.x |
| **SQLite** | ❌ 无 | ✅ SQLite |
| **init/systemd** | ✅ `init.c` + `systemd_init.c` | ✅ init |

---

## 十一、工程化与文档

| 维度 | KenuxOS | LeonOS-4 |
|------|---------|----------|
| **构建系统** | Shell 脚本 + Makefile | ✅ Python 构建图 + CI + Kconfig |
| **文档** | 较少 | ✅ 14+ 专业文档 |
| **安装器** | ❌ 无 | ✅ 完整 ISO 安装器 + OOBE |
| **第三方库集成** | ❌ 无 | ✅ TCC/Lua/SQLite/mbedTLS/litehtml/libpng |
| **CI/CD** | ❌ 无 | ✅ GitHub Actions |

---

## 十二、综合评分

| 评分维度 | KenuxOS | LeonOS-4 | 胜者 |
|---------|---------|----------|------|
| **代码量** | 567K (2.3×) | 247K | 🏆 KenuxOS |
| **文件系统** | 7+ 种 (含 ext4/NTFS/exFAT/JBD2) | 3 种 | 🏆 KenuxOS |
| **网络栈** | IPv4+IPv6+Netfilter+Conntrack+BBR+VLAN | IPv4+TCP/UDP/DHCP/DNS | 🏆 KenuxOS |
| **驱动广度** | NVMe/TPM/VirtIO/KVM/RTL/I2C/SPI/Watchdog | AHCI/e1000/AC97/USB | 🏆 KenuxOS |
| **安全模型** | SELinux + LSM + TPM | 账户/ACL | 🏆 KenuxOS |
| **调度器** | Linux CFS (红黑树) | 轮转调度 | 🏆 KenuxOS |
| **虚拟化** | KVM/VMX 内核虚拟化 | 无 | 🏆 KenuxOS |
| **兼容性** | Win32 + POSIX + NTVDM + PE + LeonOS ELF | Linux 兼容 ELF | 🏆 KenuxOS |
| **eBPF** | 完整 eBPF VM | 无 | 🏆 KenuxOS |
| **KAL 抽象层** | 多后端路由 + 熔断 + 治理 | 无 | 🏆 KenuxOS |
| **Swap** | 完整 swap in/out | 无 | 🏆 KenuxOS |
| **用户态应用丰富度** | 基础应用 + 容器运行时 | 30+ 应用（浏览器/游戏/编译器/DB） | 🏆 LeonOS |
| **第三方库集成** | 无 | TCC/Lua/SQLite/mbedTLS/litehtml/libpng | 🏆 LeonOS |
| **构建系统** | Shell 脚本 + Makefile | Python 构建图 + CI + Kconfig | 🏆 LeonOS |
| **文档** | 较少 | 14+ 专业文档 | 🏆 LeonOS |
| **安装器** | 无 | 完整 ISO 安装器 + OOBE | 🏆 LeonOS |
| **可运行成熟度** | 内核功能广但用户态生态较薄 | 完整可启动运行 + 日常可用 | 🏆 LeonOS |

---

## 十三、最终裁决

### 🏆 KenuxOS 胜出（技术深度），LeonOS-4 胜出（完成度/可用性）

**KenuxOS** 是一个**技术野心极大、内核深度惊人**的操作系统：

- 2.3 倍于 LeonOS 的代码量
- Linux 级 CFS 调度器、SELinux、eBPF、KVM 虚拟化、Netfilter/Conntrack
- 7+ 文件系统（含 NTFS、ext4、exFAT）
- Win32 兼容层 + NTVDM + PowerShell
- KAL 内核抽象层（多后端路由、熔断器）
- 这些都是**真实内核实现**，不是桩代码

**LeonOS-4** 是一个**更成熟、更可用**的操作系统：

- 完整的用户态生态（浏览器、游戏、编译器、数据库）
- 专业的构建系统和文档
- 第三方库集成（TCC、Lua、SQLite、mbedTLS）
- 实际可启动、可日常使用

---

### 结论

> **如果比"谁的技术更深、内核更强" → KenuxOS 🏆**
>
> **如果比"谁更完整、更能用" → LeonOS-4 🏆**
>
> **综合来看：KenuxOS 以 2.3 倍代码量和远超的内核技术深度，在纯技术层面更强更厉害。但 LeonOS-4 在工程化、文档、用户态生态和实际可用性上更胜一筹。**