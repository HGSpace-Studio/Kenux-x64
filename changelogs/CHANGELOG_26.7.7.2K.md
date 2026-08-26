# Kenux Kernel 26.7.7.2K - 更新日志

**发布日期：** 2026-07-07
**主题：** 大规模架构升级

---

### 新增

**同步原语体系**
- 新增 `kernel/arch/x86_64/include/arch/spinlock.h` — 自旋锁基础设施
  - 原始自旋锁（`spinlock_t`）：基于 `lock xchg` + `pause` 指令
  - Ticket Lock（`ticketlock_t`）：FIFO 公平锁，消除饥饿
  - 关中断自旋锁（`irqlock_t`）：内核临界区保护，保存/恢复 RFLAGS
  - 读写锁（`rwlock_t`）：读者不互斥，写者独占
  - 顺序锁（`seqlock_t`）：读多写少场景的无锁读
- 新增 `kernel/kernel/wait.h` / `wait.c` — 等待队列
  - 双链表等待队列，与调度器深度集成
  - `wait_queue_wait()`：释放锁后调度出去，被唤醒后自动返回
  - `wait_queue_wake_one()` / `wait_queue_wake_all()`：独占/广播唤醒
  - `wait_queue_wait_timeout()`：带超时等待
- 新增 `kernel/kernel/sync.h` / `sync.c` — 高级同步原语
  - 计数信号量（`semaphore_t`）：P/V 操作，支持超时
  - 互斥锁（`mutex_t`）：支持递归获取、优先级继承就绪
  - 条件变量（`condvar_t`）：自动释放/重新获取 mutex
  - 完成量（`completion_t`）：一次性同步信号
  - 屏障（`barrier_t`）：多线程汇合点

**SMP 多核支持**
- 新增 `kernel/arch/x86_64/include/arch/smp.h` — 多核框架头文件
  - Local APIC / IO APIC 寄存器定义和操作接口
  - per-CPU 数据结构（`cpu_info_t`、`percpu_area_t`）
  - IPI 向量定义（RESCHED/TLBFLUSH/HALT/CALL）
  - 内存屏障宏：`mb()`、`wmb()`、`rmb()`
- 新增 `kernel/arch/x86_64/smp.c` — 多核实现
  - LAPIC 驱动：读写寄存器、EOI、定时器设置
  - IOAPIC 驱动：外部中断路由到指定 APIC ID
  - AP 启动序列：INIT-SIPI-SIPI 协议，trampoline 代码复制
  - per-CPU 管理：GS_BASE 指向 `percpu_area_t`，`smp_current_cpu()` 快速获取
  - IPI 发送与处理：调度唤醒、TLB 全局刷新、CPU 停机、跨 CPU 函数调用

**Buddy + Slab 内存分配器**
- 新增 `kernel/arch/x86_64/include/arch/buddy.h` / `buddy.c` — Buddy 物理页分配器
  - 11 级 order（0~11），支持 4KB~8MB 连续物理页分配
  - 页框描述符数组（`page_t`），含 PFN、order、标志位
  - 分配时从大块拆分，释放时与伙伴合并
  - 全局接口：`alloc_pages()` / `free_pages()` / `alloc_pages_virt()` / `free_pages_virt()`
- 新增 `kernel/arch/x86_64/include/arch/slab.h` / `slab.c` — Slab 对象分配器
  - `kmem_cache_t`：管理一类固定大小对象的缓存
  - Slab 内部管理：全/部分/空三链表，空闲对象单链表
  - `kmem_cache_create()` / `kmem_cache_alloc()` / `kmem_cache_free()`
  - `kmalloc()` / `kzalloc()` / `kfree()`：通用小对象分配（8B~2048B，9 个 size cache）
  - 大对象（>2048B）自动路由到 Buddy 分配器

**CFS 完全公平调度器**
- 新增 `kernel/arch/x86_64/include/arch/cfs.h` / `cfs.c` — CFS 调度器
  - 红黑树（`rb_tree_t`）：完整的左旋/右旋/插入修复/删除/最小值查找
  - `cfs_task_t`：嵌入进程控制块，含 vruntime、exec_start、sum_exec_runtime、nice
  - Linux 标准 40 级 nice-to-weight 权重表（-20~19）
  - `cfs_enqueue_task()` / `cfs_dequeue_task()`：入队/出队
  - `cfs_pick_next_task()`：选择 vruntime 最小的任务
  - `cfs_account_exec()`：vruntime += delta * NICE_0_LOAD / weight
  - `cfs_scheduler_tick()`：tick 驱动的 vruntime 累加和抢占检查

**ELF 可执行文件加载器**
- 新增 `kernel/arch/x86_64/include/arch/elf.h` / `elf.c` — ELF64 加载器
  - ELF64 头验证（magic、class、endian、type、machine）
  - PT_LOAD 段映射：按页对齐分配物理页、复制文件数据、设置 R/W/X 标志
  - PT_INTERP 段解析：动态链接器路径提取
  - 用户栈设置（2MB）：`elf_setup_stack()` 构建 argc/argv/envp/auxv 栈框架
  - `elf_load_from_memory()`：从内存缓冲区加载
  - `elf_load_from_file()`：通过 VFS 从文件系统加载

**页缓存（Page Cache）**
- 新增 `kernel/arch/x86_64/include/arch/pagecache.h` / `pagecache.c` — 页缓存
  - 哈希表索引（1024 桶）+ LRU 淘汰链表
  - 缓存页状态：UPTODATE / DIRTY / LOCKED / WRITEBACK
  - `pagecache_get_page()`：查找或创建缓存页，未命中时从文件读取
  - `pagecache_set_dirty()` / `pagecache_clear_dirty()`：脏页标记管理
  - `pagecache_sync_node()` / `pagecache_sync_all()`：脏页回写到文件系统
  - `pagecache_get_stats()`：缓存统计

**TMPFS 内存文件系统**
- 新增 `kernel/arch/x86_64/tmpfs.c` — TMPFS
  - 所有数据存储在内存中，基于 VFS 节点
  - 动态数组存储文件数据，自动扩容
  - 支持目录创建、文件读写、目录遍历

**procfs 伪文件系统**
- 新增 `kernel/arch/x86_64/procfs.c` — /proc 文件系统
  - 系统级文件：`/proc/cpuinfo`、`/proc/meminfo`、`/proc/uptime`、`/proc/version`、`/proc/stat`
  - 进程级目录：`/proc/<pid>/status`、`/proc/<pid>/cmdline`
  - 动态内容生成：每次读取时实时计算（内存、运行时间、进程状态）

**完整 TCP/IP 网络协议栈**
- 新增 `kernel/arch/x86_64/include/arch/net.h` — 网络协议头文件
  - 以太网帧头（`eth_header_t`）、ARP 包（`arp_packet_t`）
  - IPv4 头（`ip_header_t`）、ICMP 头（`icmp_header_t`）
  - UDP 头（`udp_header_t`）、TCP 头（`tcp_header_t`）
  - TCP 控制块（64 个 socket，64KB 收发缓冲区，MSS=1460）
  - UDP 控制块（32 个 socket）
  - BSD Socket API 函数声明
- 新增 `kernel/arch/x86_64/net.c` — 网络协议栈实现
  - ARP：请求/应答、32 项缓存、子网网关自动查找
  - IPv4：校验和计算、TTL=64、分片头支持
  - ICMP：Echo Reply（支持 ping）
  - UDP：发送/接收、端口匹配
  - TCP：完整状态机（CLOSED/SYN_SENT/SYN_RECV/ESTABLISHED/FIN_WAIT/CLOSE_WAIT/LAST_ACK/TIME_WAIT）
  - TCP 三次握手、四次挥手、滑动窗口、环形收发缓冲区
  - BSD Socket API：`socket()`/`bind()`/`listen()`/`accept()`/`connect()`/`send()`/`recv()`/`sendto()`/`recvfrom()`/`shutdown()`/`close()`

**安全子系统**
- 新增 `kernel/arch/x86_64/security.c` — 安全框架
  - 32 个标准能力位（CAP_CHOWN ~ CAP_SETFCAP）
  - 能力集操作：`cap_test()`/`cap_set()`/`cap_clear()`/`cap_set_all()`
  - 进程凭证（`cred_t`）：UID/GID/EUID/EGID/SUID/SGID/FSUID/FSGID
  - ASLR：16 位熵随机化，支持栈基址和 mmap 基址随机化
  - seccomp：disabled/strict/filter 三种模式
  - SMEP/SMAP：通过 CR4 寄存器 bit 20/21 启用

**printk 内核日志系统**
- 新增 `kernel/kernel/printk.c` — printk + dmesg
  - 8 级日志：EMERG(0) / ALERT(1) / CRIT(2) / ERR(3) / WARNING(4) / NOTICE(5) / INFO(6) / DEBUG(7)
  - `<N>` 前缀自动提取日志级别
  - 64KB 环形缓冲区，spinlock 保护
  - `printk()` + 各级别快捷函数
  - `dmesg_read()` / `dmesg_clear()`：读取/清空日志缓冲区

**栈回溯与 panic 处理**
- 新增 `kernel/kernel/stacktrace.c` — 调试诊断
  - `stack_trace()`：基于 RBP 链表的栈帧遍历（最多 32 帧）
  - `panic()`：关中断 + 打印消息 + 调用栈 + 停机
  - `__assert_fail()`：断言失败自动触发 panic

**增强版 Shell**
- 新增 `kernel/kernel/shell_enhanced.c` — Shell 升级
  - 管道支持（`|`）、重定向（`>` / `>>` / `<`）
  - 环境变量（`$VAR`）、引号支持、后台执行（`&`）
  - 命令历史（16 条）、内建命令（export/env）

**硬件抽象层（HAL）**
- 新增 `kernel/arch/x86_64/include/arch/hal.h` — 统一架构抽象
  - 中断控制、内存屏障、CPU 控制
  - 页表操作接口、上下文切换、定时器
  - 为 ARM64/RISC-V 移植预留统一接口

### 构建

- 新增编译单元需加入 `build.ninja` / `Makefile`（20+ 个新 .c 文件）
- 新增头文件路径：`kernel/kernel/wait.h`、`kernel/kernel/sync.h`
- 新增头文件路径：`kernel/arch/x86_64/include/arch/spinlock.h`、`smp.h`、`buddy.h`、`slab.h`、`cfs.h`、`elf.h`、`pagecache.h`、`net.h`、`hal.h`
