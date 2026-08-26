# Kenux Kernel 26.7.7K (Initial) - 更新日志

**发布日期：** 2026-07-07
**主题：** KAPI 统一内核接口层 + Linux 兼容系统调用

---

### 新增

**KAPI 统一内核接口层（核心架构）**
- 新增 `include/kapi.h` — KAPI 总头文件，定义版本号和错误码体系
- 新增 `include/kapi_process.h` — 进程与线程管理接口（30+ API）
- 新增 `include/kapi_memory.h` — 内存管理接口（15+ API）
- 新增 `include/kapi_fs.h` — 文件系统接口（25+ API）
- 新增 `include/kapi_device.h` — 设备与 IO 接口（20+ API）

**KAPI 实现层**
- 新增 `kernel/api/kapi_process.c` — 进程管理实现
- 新增 `kernel/api/kapi_memory.c` — 内存管理实现
- 新增 `kernel/api/kapi_fs.c` — 文件系统实现
- 新增 `kernel/api/kapi_device.c` — 设备IO实现
- 新增 `kernel/api/kapi_init.c` — KAPI 层统一初始化入口
- 新增 `kernel/api/kapi_syscall.c` — Linux 兼容系统调用分发器

**Linux 兼容系统调用**
- 新增 `include/kapi_syscall.h` — 定义 1000+ 系统调用号
- 新增用户空间 `syscall` 指令内联包装函数（0-6 参数）
- 已实现 70+ 核心系统调用（read/write/open/close/fork/exec/mmap 等）
- Kenux 扩展系统调用（451-500）

**标准库补全**
- 新增 `strrchr`、`strncmp`、`strncat`、`strstr` 函数实现

### 变更

- 目录结构重组：新增 `include/`、`kernel/api/`、`apps/`、`bin/`、`docs/`
- 内核入口集成 `kapi_init()`，启动时自动初始化 KAPI 层
- 系统调用入口重构：KAPI 分发器 + 旧内核系统调用表回退

### 修复

- 修复 `interrupt.h` 参数类型不匹配
- 修复 `interrupt.S` 缺少外部声明
- 修复 `fs.c` 重复定义冲突
- 修复多处 `#include` 路径错误

### 构建

- 编译环境: WSL (Fedora) + GCC 15.2.1 + NASM 2.16.01
- 输出文件: `kernel.elf`（108KB，587 符号）
- 编译模型: `-mcmodel=large`
