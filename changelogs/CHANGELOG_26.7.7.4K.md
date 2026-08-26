# Kenux Kernel 26.7.7.4K - 更新日志

**发布日期：** 2026-07-07
**主题：** 驱动与 IPC 全面补全

---

### 新增

**PS/2 键盘驱动（独立文件）**
- 新增 `kernel/arch/x86_64/include/arch/keyboard.h` / `kernel/arch/x86_64/keyboard.c`
  - 完整 Set 1 scancode 解码表（128 正常 + 128 Shift + E0 扩展键）
  - PS/2 协议实现（端口 0x60/0x64，命令发送、数据读取）
  - 修饰键跟踪（L/R Shift/Ctrl/Alt，CapsLock/NumLock/ScrollLock 含 LED 更新）
  - 1024 容量 FIFO 键事件环形缓冲区
  - 阻塞读取 `keyboard_read()` / 非阻塞轮询 `keyboard_poll()`
  - 键事件回调机制 `keyboard_set_callback()`

**独立 LAPIC 驱动**
- 新增 `kernel/arch/x86_64/include/arch/apic.h` / `kernel/arch/x86_64/apic.c`
  - 从 `smp.c` 提取为独立驱动模块
  - LAPIC 初始化：启用 SVR，屏蔽 thermal/perf/LINT/error LVT
  - MMIO 寄存器读写（volatile uint32_t*，mfence 内存屏障）
  - IPI 发送：ICR 空闲等待，支持目标 APIC 和 0xFF 广播
  - 定时器校准：PIT channel 2 产生 1ms 精确间隔，5 次采样取平均
  - 周期定时器设置：16 分频，ms 粒度
  - LVT 掩码/取消掩码控制

**独立 IOAPIC 驱动**
- 新增 `kernel/arch/x86_64/include/arch/ioapic.h` / `kernel/arch/x86_64/ioapic.c`
  - 从 `smp.c` 提取为独立驱动模块
  - 间接寄存器访问（index/data 双寄存器，spinlock 保护）
  - 重定向条目 64 位构造：向量、投递模式、极性、触发模式、掩码、目标 APIC
  - 全量初始化：所有条目路由到 BSP，向量 0x20+
  - 单条目设置、掩码控制、读取当前配置

**JBD2 文件系统日志**
- 新增 `kernel/arch/x86_64/include/arch/jbd2.h` / `kernel/arch/x86_64/jbd2.c`
  - 日志超级块（魔数 0xC03B3998）和日志块头格式
  - 事务管理：tid 单调递增，5 种状态（RUNNING/LOCKED/FLUSHING/COMMIT/FINISHED）
  - 同步提交流程：先写描述块和数据块到日志设备 → 写 commit block → 标记 FINISHED
  - 崩溃恢复：启动时两遍扫描日志，只重放有 COMMIT 块的事务
  - 定期检查点：将已提交事务的脏数据写回主设备，释放日志空间
  - 事务中止：标记 REVOKED，释放资源
  - 双锁保护（journal_lock + commit_lock）

**NVMe 存储驱动**
- 新增 `kernel/arch/x86_64/include/arch/nvme.h` / `kernel/arch/x86_64/nvme.c`
  - PCI 设备探测（class 0x01/0x08/prog 0x02），BAR0 32/64 位映射
  - 完整控制器初始化：CAP 解析 → 禁用 CC.EN → 创建 Admin SQ/CQ → 配置 AQA/ASQ/ACQ → 启用 CC.EN → 等待 CSTS.RDY
  - Admin 命令：Identify Controller（CNS=1）、Identify Namespace（CNS=0）
  - I/O 队列创建（Create I/O SQ/CQ opcode）
  - 命令提交：spinlock 保护 SQ tail 更新，门铃寄存器敲击（含 DSTRD 间距）
  - CQ 轮询：phase 位验证、head 更新、门铃敲击
  - NVMe Read/Write：PRP1 物理连续缓冲区映射
  - NVMe Flush：无数据传输刷新命令
  - NVMe Shutdown：CC.SHN=1 关机序列

**命名管道 FIFO**
- 新增 `kernel/kernel/fifo.h` / `kernel/kernel/fifo.c`
  - 4096 字节环形缓冲区，读/写指针 + 字节计数
  - 阻塞读取：缓冲区空且写端存活时 wait_queue_wait
  - 阻塞写入：缓冲区满时 wait_queue_wait，循环写入处理环绕
  - EOF 检测：写端关闭后返回 0
  - poll 状态查询：可读/可写/挂断位掩码
  - 引用计数管理：读端+写端各一份，归零时销毁

**Unix Domain Socket (AF_UNIX)**
- 新增 `kernel/kernel/unixsock.h` / `kernel/kernel/unixsock.c`
  - 支持 STREAM 和 DGRAM 两种类型
  - STREAM：listen/accept/connect 三次握手，挂起队列（最多 16 个）
  - DGRAM：无连接，sendto/recvfrom 路径寻址
  - 双向收发环形缓冲区（各 4096 字节）
  - 半关闭支持（shutdown SHUT_RD/SHUT_WR）
  - 全局注册表：通过路径名查找绑定 socket
  - 阻塞连接/接收/发送/读取

**用户态动态链接器 ld.so**
- 新增 `kernel/arch/x86_64/include/arch/ldso.h` / `kernel/arch/x86_64/ldso.c`
  - 共享库加载：VFS 打开 ELF ET_DYN，PT_LOAD 段虚拟地址映射
  - PT_DYNAMIC 解析：DT_SYMTAB/DT_STRTAB/DT_HASH/DT_JMPREL/DT_REL/DT_INIT/DT_FINI
  - ELF hash 符号查找：bucket[] + chain[]，256 项查找缓存
  - 重定位处理：GLOB_DAT（GOT）、JUMP_SLOT（PLT）、RELATIVE、PC32
  - 依赖库递归加载（DT_NEEDED）
  - 初始化/终止函数按序调用（init_array/fini_array）
  - 库卸载：引用计数，解除映射

**Swap 交换机制**
- 新增 `kernel/arch/x86_64/include/arch/swap.h` / `kernel/arch/x86_64/swap.c`
  - 交换区管理：分区和文件两种类型，最多 1GB swap
  - 位图高效 slot 分配（位级 test/set/clear/find 操作）
  - 换出（swap_out）：分配 entry → 写入设备 → 标记页表为 swap entry → 释放物理页
  - 换入（swap_in）：从设备读取 → 映射到虚拟地址 → 恢复页表 → 释放 entry
  - 缓存收缩（swap_shrink_caches）：从 page cache LRU 链表回收页面到 swap
  - swap entry 编码：高 8 位 area index + 低 56 位 slot index

**Epoll 事件轮询**
- 新增 `kernel/kernel/epoll.h` / `kernel/kernel/epoll.c`
  - 每实例最多监听 1024 个 fd，全局最多 128 个实例
  - epoll_ctl：ADD/MOD/DEL 操作
  - epoll_wait：阻塞等待就绪事件，支持毫秒级超时
  - 就绪事件环形队列（256 条目），无锁 push/pop
  - 支持 LT（水平触发）和 ET（边缘触发）两种模式
  - 外部通知接口 `epoll_notify()`：VFS/网络栈调用

**文件 mmap 机制**
- 新增 `kernel/arch/x86_64/include/arch/mmap.h` / `kernel/arch/x86_64/mmap.c`
  - MAP_ANONYMOUS：匿名映射（malloc/brk 后端），预分配清零物理页
  - MAP_SHARED + 文件：与 page cache 集成，修改回写文件
  - MAP_PRIVATE + 文件：COW 语义，写入时缺页触发复制
  - 地址分配：从 0x7F0000000000 向下搜索空闲虚拟地址
  - munmap：MAP_SHARED 脏页回写，取消页表映射
  - mprotect：更新保护标志和页表 R/W/X 位
  - 缺页处理：COW 复制、demand paging、匿名首次分配
  - fork 继承：MAP_SHARED 共享物理页，MAP_PRIVATE 应用 COW 标记

**ACPI 电源管理 (S3/S4/S5)**
- 新增 `kernel/arch/x86_64/include/arch/acpi_pm.h` / `kernel/arch/x86_64/acpi_pm.c`
  - FADT 解析：PM1a_CNT/EVT、PM_TMR、RESET_REG、SLP_TYP 各状态偏移
  - S3 挂起到内存：保存 CPU 完整上下文（含 FXSAVE）→ 关中断 → 写入 SLP_TYP|SLP_EN
  - S3 唤醒恢复：恢复 GDTR/IDTR/CR0~CR4/EFER/FXRSTOR/全部寄存器
  - S4 休眠到磁盘：保存内存镜像到 swap → S5 关机
  - S5 关机：FADT SLP_TYP|SLP_EN + 兼容 0x604 端口
  - 系统重置：RESET_REG + 键盘控制器 0xFE 后备方案
  - PM 定时器：24 位 3.57MHz 读取

**Framebuffer GOP 解析 + 图形渲染**
- 新增 `kernel/arch/x86_64/include/arch/framebuffer.h` / `kernel/arch/x86_64/framebuffer.c`
  - Multiboot framebuffer tag（type 8）解析
  - 32-bit ARGB 像素格式，物理地址映射到内核虚拟地址
  - 像素操作：set_pixel/get_pixel/fill_rect
  - Bresenham 画线算法
  - 帧缓冲滚动（memmove + 底部填充）
  - 内置 8x16 ASCII 点阵字库（0x20~0x7E 共 95 字符）
  - 字符渲染：fb_put_char/fb_puts，自动换行和制表符
  - 光标管理：下划线光标，位置跟踪

**USB HID 类驱动**
- 新增 `kernel/arch/x86_64/include/arch/usb_hid.h` / `kernel/arch/x86_64/usb_hid.c`
  - USB class 0x03 设备探测，自动识别键盘/鼠标/通用设备
  - Boot Protocol 键盘/鼠标报告解析
  - 键盘 8 字节报告：modifiers + 6 keycodes
  - 鼠标 4 字节报告：buttons + x/y delta + wheel
  - USB 控制传输：SET_IDLE/SET_PROTOCOL/GET_REPORT/SET_REPORT
  - IN 端点中断处理，事件回调机制
  - 最多 16 个 HID 设备

### 变更

- 新增 28 个源文件（14 对 .h/.c），覆盖驱动、IPC、内存管理、电源管理 14 个子系统
- 所有新增代码遵循 Kenux 内核标准，全部为真实实现，非 stub/模拟
- LAPIC/IOAPIC 从 smp.c 提取为独立模块，提升代码模块化

### 构建

- 新增编译单元需加入 `build.ninja` / `Makefile`（28 个新 .c 文件）
- 新增头文件路径：`kernel/kernel/fifo.h`、`kernel/kernel/unixsock.h`、`kernel/kernel/epoll.h`
- 新增头文件路径：`kernel/arch/x86_64/include/arch/apic.h`、`ioapic.h`、`jbd2.h`、`nvme.h`、`ldso.h`、`swap.h`、`mmap.h`、`acpi_pm.h`、`framebuffer.h`、`usb_hid.h`、`keyboard.h`
