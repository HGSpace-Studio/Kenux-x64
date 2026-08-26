# Kenux Kernel 26.7.7K - 更新日志

**发布日期：** 2026-07-07
**主题：** 调度器升级 + 虚拟内存 + VFS 框架 + Ninja 构建

---

### 新增

**Ninja 构建系统**
- 新增 `build.ninja` — 完整的 Ninja 构建配置，支持 `-j$(nproc)` 并行编译
- 56 个编译单元全部支持并行构建
- 编译输出统一到 `build/` 目录，避免源码目录污染

**进程调度器升级**
- 重写 `process.c`，实现 5 级优先级轮转调度（IDLE/LOW/NORMAL/HIGH/REALTIME）
- 新增优先级运行队列系统（每级独立 FIFO）
- 新增 `process_create_ex()` — 扩展进程创建
- 新增 `process_kill()` / `process_wait()` — 进程终止与等待
- 新增 `process_sleep()` / `process_wakeup()` — 进程休眠与定时唤醒
- 新增 `process_current_id()` / `process_get()` — 进程查询
- 新增 `process_find_by_name()` / `process_set_priority()` — 查找与优先级
- 新增 `scheduler_tick()` / `scheduler_get_stats()` — 调度器心跳与统计
- 进程上限从 128 提升到 256
- 新增进程标志位系统（KTHREAD/USER/FIXED/EXITING）
- 每个进程支持独立的文件描述符表和工作目录

**虚拟内存管理升级**
- 重写 `memory.c`，内存池从 1024 提升到 2048 块
- 新增 `memory_alloc_aligned()` / `memory_zalloc()` / `memory_realloc()`
- 新增 `memory_alloc_physical()` / `memory_free_physical()` — 物理页分配
- 新增 VMA 虚拟内存区域管理 (`vma_t`)
- 新增页表操作接口：`pmap_create()` / `pmap_switch()` / `pmap_get()`
- 新增 `pmap_map_page()` / `pmap_unmap_page()` / `pmap_get_physical()`
- 内存上限从 1MB 提升到 1GB

**VFS 虚拟文件系统**
- 重写 `fs.c`，新增完整的 VFS 框架
- 新增 `vfs_node_t` 树形节点结构，支持父子关系
- 新增函数指针驱动的文件操作
- 新增 `vfs_register_driver()` — 文件系统驱动注册
- 新增全局文件描述符表（1024 个 FD）
- 新增挂载点管理（16 个挂载点，8 种文件系统）
- 新增 `vfs_open/close/read/write/lseek/stat/mkdir/rmdir/mount/umount`

**架构头文件增强**
- `process.h` — 新增 `process_context_t`、`PROCESS_STACK_SIZE`、`THREAD_MAX`
- `memory.h` — 新增 `PAGE_*` 标志位、`vma_t`、`pmap_*` 页表操作接口
- `fs.h` — 新增 `vfs_node_t`、`open_file_t`、`mount_point_t`、`fs_driver_t`

### 修复

- 修复 Ninja 构建文件中多行变量续行导致路径解析错误
- 修复 `build.ninja` 在 Windows (CRLF) 环境下无法正确解析
- 修复 `fs.c` 升级后 `file_t` 类型定义缺失

### 构建

- 编译环境: WSL (Fedora) + GCC 15.2.1 + NASM 2.16.01 + Ninja 1.13.2
- 输出文件: `kernel.elf`（123KB，651 符号）
- 编译单元: 56 个 .o 文件
