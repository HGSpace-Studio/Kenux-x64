# Kenux Kernel 26.7.7.3K - 更新日志

**发布日期：** 2026-07-07
**主题：** 内核核心子系统深化

---

### 新增

**Page Fault 处理器与 COW 写时复制**
- 新增 `kernel/arch/x86_64/include/arch/pagefault.h` / `kernel/arch/x86_64/pagefault.c`
  - `#PF` 异常完整处理：错误码解析、CR2 读取、原因分类
  - COW 写时复制：`cow_mark_page()` 标记共享页为只读，写入触发 #PF 时 `cow_resolve_fault()` 分配新页、复制内容、恢复写权限
  - Demand paging：首次访问未映射页面时按需分配物理页
  - 栈自动扩展：用户栈向下生长时自动映射新页（最大 8MB）
  - 缺页统计：`pf_stats_t` 追踪 COW/demand paging/stack expand/protection/kernel 各类缺页计数

**内核线程（kthread）框架**
- 新增 `kernel/kernel/kthread.h` / `kernel/kernel/kthread.c`
  - `kthread_create()` / `kthread_run()`：创建并启动内核态线程
  - `kthread_stop()`：请求线程停止（非阻塞），`kthread_join()`：等待退出
  - `kthread_should_stop()`：线程内部轮询是否应退出
  - 独立内核栈、独立进程描述符、`PROCESS_FLAG_KTHREAD` 标记

**定时器轮（Timer Wheel）**
- 新增 `kernel/kernel/timer.h` / `kernel/kernel/timer.c`
  - 4 层分层时间轮（256 槽/层），最大覆盖 2^32 ms
  - `timer_add()` / `timer_add_oneshot()` / `timer_mod()` / `timer_del()`
  - `timer_tick()`：由 PIT/HPET 中断每 1ms 驱动
  - 级联机制：低层轮转一圈后自动将高层定时器降级
  - `msleep()` / `usleep()`：基于定时器轮的延迟睡眠
  - 高精度定时器接口（`hrtimer_t`）

**Workqueue 与 Tasklet 延迟执行框架**
- 新增 `kernel/kernel/workqueue.h` / `kernel/kernel/workqueue.c`
  - Workqueue：在进程上下文中执行工作（可睡眠），每个 wq 关联一个 kthread
  - `create_workqueue()` / `queue_work()` / `queue_delayed_work()` / `cancel_work()` / `flush_workqueue()`
  - 系统默认工作队列 `system_wq`
  - Tasklet：软中断上下文快速执行（不可睡眠），支持高优先级 `tasklet_hi_schedule()`
  - `tasklet_schedule()` / `tasklet_disable()` / `tasklet_enable()` / `tasklet_kill()`

**RCU（Read-Copy Update）无锁读取**
- 新增 `kernel/kernel/rcu.h` / `kernel/kernel/rcu.c`
  - 读者零开销：`rcu_read_lock()` / `rcu_read_unlock()` 仅递增计数器
  - 宽限期管理：`call_rcu()` 注册回收回调，`rcu_barrier()` 同步等待
  - 两阶段回调队列（`RCU_GP_STAGES`），宽限期结束后依次执行
  - `rcu_assign_pointer()` / `rcu_dereference()` 宏保证内存顺序
  - SRCU（可睡眠 RCU）：双索引读者计数，支持在进程上下文同步

**IPC 进程间通信**
- 新增 `kernel/kernel/ipc.h` / `kernel/kernel/ipc.c`
  - System V 共享内存：`shmget()` / `shmat()` / `shmdt()` / `shmctl()`，64 个段，引用计数
  - System V 消息队列：`msgget()` / `msgsnd()` / `msgrcv()` / `msgctl()`，按 mtype 匹配，阻塞/非阻塞
  - System V 信号量集：`semget()` / `semop()` / `semctl()`，16 个信号量/集，等待/唤醒
  - Futex：`futex_wait()` / `futex_wake()` / `futex_requeue()`，哈希桶管理等待者
  - `ipc_syscall()` 统一分发

**信号传递机制（Signal Delivery）**
- 新增 `kernel/kernel/signal.h` / `kernel/kernel/signal.c`
  - 31 种标准信号（SIGHUP~SIGSYS）+ 32 个实时信号（SIGRTMIN~SIGRTMAX）
  - 实时信号支持排队（`SIGQUEUE_MAX=8`），标准信号不排队
  - `sigaction()`：注册用户自定义信号处理函数
  - `sigprocmask()`：信号掩码管理（BLOCK/UNBLOCK/SETMASK）
  - `signal_send()` / `signal_send_info()`：向目标进程投递信号
  - `signal_do_pending()`：返回用户态前处理待处理信号
  - SIGKILL 不可阻塞、SIGSTOP 不可捕获

**GDB 远程调试 Stub**
- 新增 `kernel/kernel/gdb_stub.h` / `kernel/kernel/gdb_stub.c`
  - 通过 COM1 串口（0x3F8）与 GDB 通信
  - 实现部分 RSP 命令：`?`（停止原因）、`g/G`（寄存器）、`m/M`（内存）、`c/s`（继续/单步）、`k`（杀死）
  - 断点管理：`Z/z` 命令，INT3（0xCC）软件断点
  - `gdb_stub_enter()`：异常/断点触发后进入调试循环

**ACPI MADT 解析（多核 CPU 枚举）**
- 新增 `kernel/arch/x86_64/include/arch/acpi_madt.h` / `kernel/arch/x86_64/acpi_madt.c`
  - RSDP 搜索：EBDA（扩展 BIOS 数据区）+ 传统 BIOS 区域（0xE0000-0xFFFFF）
  - RSDT/XSDT 解析：根据 RSDP revision 选择 32 位/64 位表
  - MADT 条目遍历：LAPIC（CPU 枚举）、IOAPIC、中断源覆盖、LAPIC NMI
  - 输出 `acpi_madt_info_t`：最多 64 个 CPU 的 APIC ID 和启用状态

**Virtio 设备驱动**
- 新增 `kernel/arch/x86_64/include/arch/virtio.h` / `kernel/arch/x86_64/virtio.c`
  - Virtio 1.0 MMIO 传输层
  - Virtqueue 管理：描述符表 + Available Ring + Used Ring，空闲描述符链表
  - virtio-blk：`virtio_blk_init()` / `virtio_blk_read()` / `virtio_blk_write()`，支持读/写/Flush
  - virtio-net：`virtio_net_init()` / `virtio_net_send()` / `virtio_net_recv()`，双队列（RX/TX）

**内核模块加载器（.ko）**
- 新增 `kernel/kernel/module.h` / `kernel/kernel/module.c`
  - 解析 ELF 可重定位文件（ET_REL）
  - `.text` / `.data` / `.bss` 段提取与加载
  - 符号表（SHT_SYMTAB）解析，查找 `init_module` / `cleanup_module` 入口
  - 简化重定位：R_X86_64_64（绝对）、R_X86_64_PC32（相对）、R_X86_64_PLT32
  - `module_load()` / `module_unload()` / `module_find()` / `module_export_symbol()`
  - 内核符号导出表（256 条目），供模块解析外部引用

**sysfs 设备属性文件系统**
- 新增 `kernel/kernel/sysfs.h` / `kernel/kernel/sysfs.c`
  - 标准目录结构：`/sys/kernel/`、`/sys/devices/`
  - 属性文件：动态 show/store 回调 + 静态值
  - VFS 集成：`sysfs_get_root()` 挂载到 VFS

**devtmpfs 自动设备节点**
- 新增 `kernel/kernel/devtmpfs.h` / `kernel/kernel/devtmpfs.c`
  - 内核启动时自动创建 `/dev` 下标准设备节点
  - `devtmpfs_register()` / `devtmpfs_unregister()`：动态注册/注销设备
  - 默认设备：null、zero、full、random、urandom、tty、console、sda/sda1/sda2、hda/hda1、loop0/loop1/loop2
