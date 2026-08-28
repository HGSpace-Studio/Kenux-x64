# Kenux 操作系统

```
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
```

**Kenux 内核 KNE2.7 | 系统版本 26.8.28 (星尘)**

## 系统概述

Kenux OS 是一个现代化的高性能操作系统，从零开始构建，专注于稳定性、安全性和用户体验。设计用于桌面和服务器环境，提供完整的计算平台，原生支持游戏、网页浏览、多媒体和开发。

### 核心特性

- **先进的内核架构**: 微内核混合架构，模块化驱动程序
- **原生图形栈**: 通过KanvasUI实现硬件加速的2D/3D渲染
- **完整的应用套件**: 全面的生产力、娱乐和开发工具
- **现代安全性**: SELinux风格MAC、全盘加密、安全启动
- **高性能**: 针对x86_64优化，支持多核调度
- **网络就绪**: TCP/IP协议栈、WiFi、蓝牙、VPN支持

## 系统信息

| 组件 | 规格 |
|------|------|
| **内核** | Kenux 内核 KNE2.7 (KNE) |
| **架构** | x86_64 (amd64) |
| **ABI** | Kenux 扩展 ABI v3 |
| **系统调用接口** | 统一系统调用 (kapi_unified_syscall) |
| **内存模型** | 带 ASLR 的平坦 64 位 |
| **进程调度器** | CFS 带实时扩展 |
| **文件系统** | ext4, Btrfs, ZFS, FAT32, NTFS |
| **图形** | Kenux 图形 G5000 (Vulkan/OpenGL) |
| **音频** | PulseAudio 兼容服务器 |

## 快速开始

### 前置条件

```bash
# 构建依赖
sudo apt-get install build-essential ninja-build gcc make nasm qemu-system-x86

# 可选: 交叉编译工具链
sudo apt-get install gcc-x86_64-linux-gnu binutils-x86_64-linux-gnu
```

### 构建系统

```bash
# 克隆仓库
git clone https://github.com/kenux-os/Kenux-x64.git
cd Kenux-x64

# 唯一构建入口
python3 build.py run all

# 仅构建内核、UEFI bootloader 或用户态组件
python3 build.py run kernel
python3 build.py run bootloader
python3 build.py run components
```

兼容脚本 `build_all.sh`、`build_native.sh` 和 `build_components.sh` 只转发到 `build.py`；它们不再生成占位应用或安装文件到宿主机目录。
