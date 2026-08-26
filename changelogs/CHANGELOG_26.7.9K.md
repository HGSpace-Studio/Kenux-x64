# Kenux Kernel v26.7.9K 更新日志

**发布日期：** 2026-07-09

**主题：** 骨架全面补全 + K API 46 模块 + 双层 API 架构

---

## 总览

本版本是 Kenux Kernel 迄今为止最大规模的一次更新。完成了对所有子系统骨架/桩函数的审计与补全，新增 46 个 K API 内核模块，建立了 K API / KA API 双层架构体系，并将编译产物从 362 KB 扩展到 457 KB。

---

## 核心子系统补全

### 进程调度
- 实现 `process_yield`：轮转调度，遍历就绪队列切换上下文
- 实现 `process_switch`：x86_64 内联汇编上下文切换（rbp/rbx/r12-r15/rsp）
- 实现 `enqueue_process`：进程入队状态管理
- 实现 `thread_create`：基于 process_create_ex 创建线程
- 实现 `process_create_ex`：参数化创建，支持 entry/arg/priority/flags/parent
- 新增 `scheduler_get_stats`：调度统计信息接口

### 内存管理
- 实现 `pmap_init`：从 CR3 读取当前 PML4
- 实现 `pmap_map_page`：四级页表遍历，动态分配中间页表，invlpg 刷新
- 实现 `pmap_unmap_page`：页表清除 + TLB 刷新
- 实现 `pmap_get_physical`：虚拟地址转物理地址，支持 2MB 大页
- 实现 `ksize`：遍历 slab/伙伴系统返回实际分配大小

### 中断与同步
- 实现 `interrupt_dispatch`：PIC ISR 读取 + handler 分发 + EOI
- 实现 `semaphore_down_timeout`：基于 jiffies 的真实超时
- 实现 `condvar_wait_timeout`：条件变量超时等待
- 实现 `wait_queue_wait_timeout`：等待队列超时版本
- 实现 `queue_delayed_work`：timer 注册 + 到期自动入队
- 修复 `__rcu_all_readers_exited`：多核感知（sizeof 自动计算）

### IPC
- 实现 `ipc_init`：共享内存 + 消息队列 + 信号量 + 环形缓冲区初始化
- 实现 `ipc_send`：自旋锁保护环形缓冲区写入 + 唤醒目标
- 实现 `ipc_receive`：按源过滤 + 安全移除

### 调试
- 实现 `__addr_to_symbol`：内核符号表线性查找
- 实现 `__print_frame`：栈帧符号名输出
- 实现 `module_lookup_symbol`：全局 exports + 模块 symtab 遍历

### 文件系统
- `fs_open/close/read/write/seek`：全部对接 VFS
- `vfs_mount/umount`：挂载点表管理
- `ext2`：完整间接块（单/双/三间接）+ fd 管理 + 路径解析
- `ext4`：真实 AHCI 磁盘 IO + 修复空指针解引用
- `tmpfs`：实现 unlink + 引用计数
- `procfs`：从实际内核数据读取 CPU/中断/进程统计
- `jbd2`：真实 AHCI 设备块读写

### 网络栈
- DHCP 完整状态机：Discover -> Offer -> Request -> ACK
- DNS 解析：UDP socket + 轮询接收（500ms 超时）+ 缓存
- `nf_hook_slow`：Netfilter 钩子遍历执行
- `conntrack_put`：引用计数管理
- TCP 拥塞控制：BBR/CUBIC per-socket 状态隔离

### 设备驱动
- AHCI：H2D FIS + PRD + DMA 扇区读写
- xHCI/EHCI：PORTSC 寄存器端口状态检测
- USB HID：控制传输 + 中断传输 + 报告描述符解析
- VGA：对接 keyboard_read 实现 vga_getchar
- Framebuffer：分辨率设置 + 缓冲区重分配
- HDA/Sound：MMIO 寄存器读写 + 播放/录音分发

### ACPI 与虚拟化
- 修复 RSDP 签名："_RSD_PT_" -> "RSD PTR "
- 实现 MADT IOAPIC 解析
- KVM：修复 vmxon_region 变量名 + VMX 退出处理（HLT/IO/EPT/CR/MSR）
- VirtIO：MMIO 探测 + 设备初始化

---

## K API 新增模块（46 个）

### 数据结构
| 模块 | 对标 Linux |
|------|-----------|
| kapi_list | list.h — 双向链表 |
| kapi_rbtree | rbtree.h — 红黑树 |
| kapi_kfifo | kfifo.h — 内核环形 FIFO |
| kapi_idr | idr.h — ID 分配器 |
| kapi_bitmap | bitmap.h — 位图操作 |
| kapi_hash | hlist/hash.h — 哈希表 |

### 同步原语
| 模块 | 对标 Linux |
|------|-----------|
| kapi_mutex | mutex.h — 互斥锁 |
| kapi_completion | completion.h — 完成量 |
| kapi_wait | wait.h — 等待队列 |
| kapi_rcu | rcu.h — RCU 无锁读取 |
| kapi_atomic | atomic.h — 原子操作（lock 前缀） |

### 内核基础
| 模块 | 对标 Linux |
|------|-----------|
| kapi_kthread | kthread.h — 内核线程 |
| kapi_module | module.h — 模块系统 |
| kapi_notifier | notifier.h — 通知链 |
| kapi_irq | irq.h — 中断管理 |
| kapi_params | moduleparam.h — 模块参数 |

### 调度与 SMP
| 模块 | 对标 Linux |
|------|-----------|
| kapi_sched | sched.h — 调度器 |
| kapi_smp | smp.h — 多核 |
| kapi_cpumask | cpumask.h — CPU 掩码 |

### 工具库
| 模块 | 对标 Linux |
|------|-----------|
| kapi_string | string.h/kernel.h — 字符串工具 |
| kapi_time | timekeeping.h — 时间管理 |
| kapi_random | random.h — 随机数 |
| kapi_sort | sort.h — 排序算法 |
| kapi_crc | crc16/crc32.h — 校验和 |

### IO 与设备
| 模块 | 对标 Linux |
|------|-----------|
| kapi_io | io.h — IO 端口/MMIO/ioremap |
| kapi_pci | pci.h — PCI 设备管理 |
| kapi_dma | dma-mapping.h — DMA 映射 |
| kapi_cdev | cdev.h — 字符设备 |
| kapi_blkdev | blkdev.h — 块设备 |
| kapi_security | lsm.h — LSM 安全框架 |

### 网络栈
| 模块 | 对标 Linux |
|------|-----------|
| kapi_skbuff | skbuff.h — 套接字缓冲区 |
| kapi_netdevice | netdevice.h — 网络设备 |
| kapi_socket | socket.h — Socket 核心 |
| kapi_netlink | netlink.h — Netlink 协议 |

### 文件系统与调试
| 模块 | 对标 Linux |
|------|-----------|
| kapi_vfs | vfs.h — VFS 挂载/卸载 |
| kapi_seq_file | seq_file.h — procfs 序列文件 |
| kapi_debugfs | debugfs.h — 调试文件系统 |
| kapi_kobject | kobject.h — sysfs 内核对象 |
| kapi_mempool | mempool.h — 内存池 |
| kapi_ftrace | ftrace.h — 函数跟踪 |

---

## 双层 API 架构

- **K API**（`kapi_` 前缀）— 内核内部 API，46 个模块，对标 Linux include/linux/
- **KA API**（已有 kapi_process/memory/fs/device/syscall）— 用户态 POSIX 系统调用封装
- `kapi.h` 统一头文件，`kapi_init()` 一键初始化

---

## 构建系统

- 编译单元从 87 个增加到 127 个
- Makefile + build.ninja 双构建系统同步更新
- WSL (Fedora) gcc 16.1.1 + ninja 1.13.2 编译通过
- kernel.elf 从 362 KB 增长到 457 KB

---

## Bug 修复

| 文件 | 问题 |
|------|------|
| acpi.c | RSDP 签名 "_RSD_PT_" 修正为 "RSD PTR " |
| kvm.c | vmx_region 变量名修正为 vmxon_region |
| ext4.c | 移除空指针解引用的 ext4_dir_iterate(NULL) 调用 |
| tcp_bbr.c / tcp_cong.c | 消除全局 BBR/CUBIC 状态共享，改为 per-socket |
| dns.c | 修复 dns_get_time 永远返回 0 的问题 |
| conntrack.c | 修复连接超时永远不触发的问题 |

---

## 文件统计

| 指标 | v26.7.7.4K | v26.7.9K |
|------|------------|----------|
| C 源文件 | ~120 | ~165 |
| 头文件 | ~100 | ~180 |
| 编译单元 | 87 | 127 |
| kernel.elf | 362 KB | 457 KB |
| K API 模块 | 5 | 46 |
