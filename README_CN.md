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

# 或单独构建目标:
python3 build.py run kernel       # 内核、flat binary 和 ESP 内核文件
python3 build.py run bootloader   # UEFI bootloader
python3 build.py run components   # 用户态组件
```

兼容脚本 `build_all.sh`、`build_native.sh` 和 `build_components.sh` 只转发到 `build.py`；它们不再生成占位应用或安装文件到宿主机目录。旧的 `sync`、`kex` 和 demo 入口会明确返回错误。

### 在 QEMU 中运行

```bash
# 图形窗口启动
python3 build.py run run

# 仅串口日志启动
python3 build.py run run-debug
```

运行目标需要本机安装 QEMU；启动前会检查 gcc、objcopy、NASM、QEMU 和必要源码，任一项失败都会返回非零状态。

## 应用套件

### 系统工具

| 应用程序 | 描述 | 二进制文件 |
|----------|------|-----------|
| **FastFetch** | 系统信息显示 | `fastfetch.kex` |
| **系统信息** | 详细的硬件/软件信息 | `sysinfo.kex` |
| **任务管理器** | 进程监控和管理 | `tasks.kex` |
| **设置** | 系统配置面板 | `settings.kex` |
| **终端模拟器** | 命令行界面 | `term.kex` |

### 生产力工具

| 应用程序 | 描述 | 二进制文件 |
|----------|------|-----------|
| **记事本** | 简单文本编辑器 | `notepad.kex` |
| **文本编辑器** | 高级代码/文本编辑器 | `textedit.kex` |
| **文件管理器** | 图形化文件浏览器 | `fm.kex` |
| **计算器** | 科学计算器 | `calculator.kex` |
| **网页浏览器** | 现代网络浏览器 | `browser.kex` |

### 娱乐应用

| 应用程序 | 描述 | 二进制文件 |
|----------|------|-----------|
| **贪吃蛇游戏** | 经典贪吃蛇游戏 | `snake.kex` |
| **俄罗斯方块** | 方块拼图游戏 | `tetris.kex` |
| **游戏启动器** | 游戏库管理器 | `games.kex` |
| **音乐播放器** | 音频播放 | `music.kex` |
| **图片查看器** | 照片/图片查看器 | `image.kex` |

### 网络工具

| 应用程序 | 描述 | 二进制文件 |
|----------|------|-----------|
| **网络监控** | 实时网络统计 | `netmon.kex` |
| **浏览器** | 网页浏览客户端 | `browser.kex` |

## 开发指南

### 使用 kex 编译器

`kex` 编译器是 Kenux 的原生应用编译器，生成与 Kenux 运行时兼容的 `.kex` 可执行文件。

#### 基本用法

```bash
# 编译简单程序
./kexC/bin/kex hello.c -o hello.kex --name "Hello World"

# 带优化选项
./kexC/bin/kex app.c -o app.kex --name "My App" -O2 -I include

# 运行编译后的程序
./kexC/bin/kex run app.kex

# 强制图形模式
./kexC/bin/kex run app.kex --gfx --gfx-size 1024x768
```

#### 编译器选项

| 选项 | 描述 |
|------|------|
| `-o <路径>` | 输出文件路径 |
| `--name <名称>` | 程序名称（嵌入头文件） |
| `-O<级别>` | 优化级别 (0,1,2,3,s) |
| `-I<目录>` | 添加包含目录 |
| `-L<目录>` | 添加库目录 |
| `-l<库>` | 链接库 |
| `--format` | 输出格式: kex, kxp, elf |
| `--shared` | 构建共享库 (.kxp) |
| `-g` | 包含调试符号 |
| `-s` | 剥离符号 |
| `--static` | 静态链接 |
| `--verbose` | 详细输出 |

### Shell 命令

Kenux Shell (`ksh`) 提供全面的命令行接口：

#### 文件操作

```bash
ls              # 列出目录内容
cd <路径>       # 切换目录
pwd             # 打印工作目录
cat <文件>      # 显示文件内容
mkdir <目录>    # 创建目录
rm <文件>       # 删除文件/目录
cp <源> <目标>  # 复制文件
mv <源> <目标>  # 移动/重命名文件
```

#### 系统信息

```bash
ps              # 列出进程
top             # 资源使用监控
df              # 磁盘空间使用
free            # 内存使用情况
uname           # 系统信息
uptime          # 系统运行时间
whoami          # 当前用户
hostname        # 系统主机名
neofetch        # 显示带Logo的信息
fastfetch       # 快速系统信息
sysinfo         # 详细系统报告
```

#### 进程管理

```bash
kill <pid>      # 通过PID终止进程
systemctl       # 服务管理
reboot          # 重启系统
poweroff        # 关闭系统
```

#### 应用程序

```bash
snake           # 玩贪吃蛇游戏
tetris          # 玩俄罗斯方块
calc            # 启动计算器
desktop         # 启动图形环境
run <程序>      # 执行ELF二进制文件
```

## 内核架构

### 模块系统

内核采用模块化架构，支持动态加载：

```
内核核心 (kenux_kernel)
├── 内存管理
│   ├── 物理页面分配器
│   ├── 虚拟内存 (分页)
│   ├── Slab 分配器
│   └── 内存保护 (NX, SMEP, SMAP)
├── 进程管理
│   ├── 调度器 (CFS + RT)
│   ├── 线程管理
│   └── 信号处理
├── 文件系统
│   ├── VFS 层
│   ├── ext4 驱动
│   └── 网络文件系统 (NFS, SMB)
├── 设备驱动
│   ├── 块设备 (NVMe, SATA)
│   ├── 图形 (GPU, 显示)
│   ├── 输入 (键盘, 鼠标)
│   └── 网络 (以太网, WiFi)
├── 网络
│   ├── TCP/IP 协议栈
│   ├── Socket API
│   ├── 防火墙 (Netfilter)
│   └── 无线协议栈
└── 安全性
    ├── 强制访问控制
    ├── 权限能力
    ├── 审计系统
    └── 加密 (AES, ChaCha20)
```

### 关键API

- **kapi**: 驱动开发的内核API
- **kapi_window**: 窗口管理器接口
- **kapi_graphics2d**: 2D图形原语
- **kapi_input**: 输入设备处理
- **kapi_network**: 网络编程
- **kapi_vfs**: 文件系统操作
- **kapi_process**: 进程管理
- **kapi_memory**: 内存分配
- **kapi_sync_ext**: 同步原语

## 目录结构

```
Kenux-x64/
├── apps/                  # 用户空间应用程序
│   ├── snake_game.c      # 贪吃蛇游戏实现
│   ├── tetris.c          # 俄罗斯方块游戏
│   ├── calculator.c      # 计算器应用
│   ├── notepad.c         # 文本编辑器
│   ├── fastfetch.c       # 系统信息显示
│   ├── browser.c         # 网页浏览器
│   └── ...
├── bin/                   # 编译后的二进制文件 (*.kex)
├── include/               # 公共头文件
│   ├── stdio.h           # 标准输入输出
│   ├── stdlib.h          # 标准库
│   ├── string.h          # 字符串函数
│   ├── time.h            # 时间函数
│   ├── vga.h             # VGA 控制台 API
│   └── kapi_*.h          # 内核 API
├── kernel/                # 内核源码
│   ├── arch/x86_64/      # 架构特定代码
│   ├── lib/libc/         # 内核 libc
│   └── kernel/           # 核心内核
├── kexC/                  # kex 编译器
│   ├── src/kex.c         # 编译器源码
│   ├── include/          # 编译器头文件
│   └── bin/kex           # 编译器二进制
├── LeonOS-4/             # 参考实现
├── build_all.sh          # 构建脚本
├── kernelbuild.ninja     # Ninja 构建文件
└── README.md             # 英文文档
    README_CN.md          # 中文文档（本文件）
```

## 安全特性

- **地址空间布局随机化 (ASLR)**: 随机内存布局
- **栈保护**: Canary值和NX位强制执行
- **控制流完整性 (CFI)**: 间接调用验证
- **内核页表隔离 (KPTI)**: Meltdown缓解措施
- **全盘加密**: LUKS兼容加密
- **安全启动**: UEFI 安全启动支持
- **强制访问控制**: SELinux风格策略
- **沙箱隔离**: 每个应用独立隔离
- **审计日志**: 全面安全事件记录
- **内存安全**: 边界检查、释放后使用检测

## 性能特征

- **启动时间**: < 3秒进入桌面 (SSD)
- **内存占用**: ~256MB基础 (含GUI)
- **调度延迟**: < 1ms 实时任务
- **图形吞吐量**: 1080p下60+ FPS
- **网络性能**: 10GbE线速
- **I/O吞吐量**: 支持NVMe Gen4速度

## 兼容性

### 支持的硬件

- **CPU**: Intel (Haswell+), AMD (Zen+), 仅 x86_64
- **内存**: 最小512MB, 推荐4GB+
- **存储**: SATA SSD/NVMe, 支持USB启动
- **图形**: VGA模式回退, GPU加速可选
- **网络**: 千兆以太网, 802.11ac WiFi
- **输入**: PS/2 和 USB 键盘/鼠标

### 文件格式支持

| 格式 | 读取 | 写入 | 说明 |
|------|------|------|------|
| **ext4** | ✅ | ✅ | 主要文件系统 |
| **FAT32** | ✅ | ✅ | 可移动介质 |
| **NTFS** | ✅ | ⚠️ | 实验性写入 |
| **Btrfs** | ✅ | ✅ | 快照, 压缩 |
| **ISO9660** | ✅ | ❌ | CD/DVD镜像 |

## 贡献指南

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

### 代码规范

- 使用 C11 标准配合 GNU 扩展
- 遵循内核编码风格 (Tab缩进)
- 无尾随空格
- 在头文件中记录公共API
- 提交前在 QEMU 上测试

## 许可证

本项目基于 GPL v3 许可证发布 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 致谢

- **LeonOS**: 参考架构和设计模式
- **Linux**: 许多子系统的灵感来源
- **StardustUI**: GUI框架基础
- **TinyCC**: 编译器技术基础
- **PicoLibC**: 嵌入式C库参考

## 版本历史

### 26.8.28 (当前版本)
- 增强 Shell 支持 35+ 命令
- 完整的应用套件 (15+ 应用)
- 改进的 fastfetch 自定义 ASCII 艺术
- 新增文件管理器 GUI
- 系统信息实用工具
- 游戏应用 (贪吃蛇, 俄罗斯方块)

### 26.7.9
- 初始稳定版本
- 基础窗口管理器
- 核心系统工具
- kex 编译器 v1.0

---

**由 Kenux 团队用 ❤️ 构建**

更多信息请访问: https://kenux-os.org

*系统 Logo: Kenux ASCII 艺术字*
*内核: KNE2.7 | 代号: 星尘*
