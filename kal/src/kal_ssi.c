/* SPDX-License-Identifier: MIT
 * kal_ssi.c - SSI(System Service Interface)对外壳暴露的 C ABI 入口实现
 *
 * 每个入口做三件事:
 *   1. 治理层守卫(热切换/熔断期间拒绝进入内核);
 *   2. 经 marshalling 校验参数;
 *   3. 经 dispatcher 按接口域路由到目标 backend,并把返回码翻译为统一 kal_err_t。
 */
#include "kal.h"
#include "kal_dispatcher.h"
#include "kal_governance.h"
#include "kal_marshalling.h"
#include "kal_errno.h"

#include <string.h>
#include <stdio.h>

#if defined(KAL_KERNEL)
#include <arch/hpet.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__MACH__)
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

/* 按接口域选 backend:走 dispatcher 路由表,无绑定则回退活跃 backend。
 * 这样外壳调 kal_ipc_send 时,若 IPC 域绑定到 micro,会自动路由到 micro,
 * 而 kal_open 等仍走活跃 backend —— 多内核并存对外壳透明。
 */
static const struct kernel_backend_ops *ops_for(uint32_t domain) {
    return kal_dispatcher_pick_ops(domain);
}

/* 治理层守卫:热切换窗口/熔断期间,拒绝非 BYPASS 调用进入内核。
 * SSI 入口对外壳是同步阻塞语义,这里直接返回错误码,避免把不稳定内核打挂。
 */
static kal_err_t ssi_guard(void) {
    if (kal_governance_is_switching())    return KAL_ESWITCHING;
    if (kal_governance_is_circuit_open()) return KAL_ECIRCUIT;
    return KAL_OK;
}

/* ---- 文件系统 ---- */
kal_fd_t kal_open(const char *path, int flags, int mode) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return (kal_fd_t)g;
    kal_err_t e = kal_marshall_check_open(path, flags);
    if (e != KAL_OK) return (kal_fd_t)e;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_FS);
    if (!b || !b->open) return (kal_fd_t)KAL_ENOSYS;
    int r = b->open(path, flags, mode);
    if (r < 0) {
        /* r 是该 backend 的原始 errno,经 marshalling 翻译 */
        return (kal_fd_t)kal_errno_from_native(b->err_src, r);
    }
    return (kal_fd_t)r;
}

int kal_close(kal_fd_t fd) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (fd < 0) return KAL_EBADF;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_FS);
    if (!b || !b->close) return KAL_ENOSYS;
    int r = b->close(fd);
    return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
}

long kal_read(kal_fd_t fd, void *buf, kal_size_t n) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    kal_err_t e = kal_marshall_check_rw(fd, buf, n);
    if (e != KAL_OK) return e;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_FS);
    if (!b) return KAL_ENOSYS;
    if (!b->read) {
        /* 退化路径:用通用 syscall 转发 */
        return b->syscall ? b->syscall(KAL_NR_READ, fd, (long)buf, (long)n, 0,0,0)
                          : KAL_ENOSYS;
    }
    return b->read(fd, buf, n);
}

long kal_write(kal_fd_t fd, const void *buf, kal_size_t n) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    kal_err_t e = kal_marshall_check_rw(fd, buf, n);
    if (e != KAL_OK) return e;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_FS);
    if (!b) return KAL_ENOSYS;
    if (!b->write) {
        return b->syscall ? b->syscall(KAL_NR_WRITE, fd, (long)buf, (long)n, 0,0,0)
                          : KAL_ENOSYS;
    }
    return b->write(fd, buf, n);
}

int kal_stat(const char *path, struct kal_stat *out) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!path || !out) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_FS);
    if (!b) return KAL_ENOSYS;
    /* 简化:演示版直接清零并填一个虚拟值(真实实现经 marshalling 转换内核 stat) */
    memset(out, 0, sizeof(*out));
    out->st_mode = 0644;
    out->st_size = 0;
    return KAL_OK;
}

/* ---- 内存 ---- */
void *kal_mmap(struct kal_mmap_args *args) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return NULL;
    kal_err_t e = kal_marshall_check_mmap(args);
    if (e != KAL_OK) return NULL;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_MEMORY);
    if (!b || !b->mmap) return NULL;
    void *addr = NULL;
    int r = b->mmap(args, &addr);
    if (r != 0) return NULL;
    return addr;
}

int kal_munmap(void *addr, kal_size_t length) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!addr || length == 0) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_MEMORY);
    if (!b || !b->munmap) return KAL_ENOSYS;
    int r = b->munmap(addr, length);
    return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
}

/* ---- 进程 / 线程 ---- */
kal_pid_t kal_fork(void) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return (kal_pid_t)g;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_PROCESS);
    if (!b) return (kal_pid_t)KAL_ENOSYS;
    /* 能力探测:若活跃 backend 的 caps 没有 KAL_CAP_FORK,走 fallback(clone+exec 模拟) */
    const kal_backend_state_t *s = kal_governance_active_state();
    if (s && !kal_cap_has(&s->caps, KAL_CAP_FORK)) {
        kal_pid_t child = -1;
        kal_err_t e = kal_fallback_fork(b, &child);
        return e == KAL_OK ? child : (kal_pid_t)e;
    }
    if (!b->fork) return (kal_pid_t)KAL_ENOSYS;
    kal_pid_t child = -1;
    int r = b->fork(&child);
    if (r != 0) return (kal_pid_t)kal_errno_from_native(b->err_src, r);
    return child;
}

int kal_exec(const char *path, char *const argv[], char *const envp[]) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!path) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_PROCESS);
    if (!b || !b->exec) return KAL_ENOSYS;
    int r = b->exec(path, argv, envp);
    return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
}

int kal_wait(kal_pid_t pid, int *out_status, int options) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (pid < 0) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_PROCESS);
    if (!b || !b->wait) return KAL_ENOSYS;
    int r = b->wait(pid, out_status, options);
    return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
}

int kal_sched_setaffinity(kal_pid_t pid, size_t cpusetsize,
                          const unsigned char *mask) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!mask) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_PROCESS);
    if (!b || !b->syscall) return KAL_ENOSYS;
    long r = b->syscall(KAL_NR_SCHED_SETAFF, pid, (long)cpusetsize, (long)mask, 0,0,0);
    return r == 0 ? KAL_OK : (int)r;
}

/* ---- 设备 I/O ---- */
int kal_ioctl(kal_fd_t fd, unsigned long request, void *arg) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (fd < 0) return KAL_EBADF;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_DEVIO);
    if (!b || !b->syscall) return KAL_ENOSYS;
    long r = b->syscall(KAL_NR_IOCTL, fd, (long)request, (long)arg, 0,0,0);
    return r == 0 ? KAL_OK : (int)r;
}

/* ---- 网络(可选能力) ---- */
static int caps_supports_net(void) {
    const kal_backend_state_t *s = kal_governance_active_state();
    return s && kal_cap_has(&s->caps, KAL_CAP_NET_STACK);
}

int kal_socket(int domain, int type, int protocol) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!caps_supports_net()) return KAL_ENOTSUP;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_NET);
    if (!b || !b->syscall) return KAL_ENOSYS;
    long r = b->syscall(KAL_NR_SOCKET, domain, type, protocol, 0,0,0);
    if (r < 0) return kal_errno_from_native(b->err_src, (int)r);
    return (int)r;
}
int kal_bind(int fd, const void *addr, size_t addrlen) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!addr) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_NET);
    if (!b) return KAL_ENOSYS;
    (void)fd; (void)addrlen;
    return KAL_OK;
}
int kal_connect(int fd, const void *addr, size_t addrlen) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_NET);
    (void)b; (void)fd; (void)addr; (void)addrlen;
    return KAL_ENOTSUP;
}
long kal_sendmsg(int fd, const void *msg, int flags) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (fd < 0 || !msg) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_NET);
    if (!b || !b->syscall) return KAL_ENOSYS;
    return b->syscall(KAL_NR_SENDMSG, fd, (long)msg, flags, 0,0,0);
}
long kal_recvmsg(int fd, void *msg, int flags) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (fd < 0 || !msg) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_NET);
    if (!b || !b->syscall) return KAL_ENOSYS;
    return b->syscall(KAL_NR_RECVMSG, fd, (long)msg, flags, 0,0,0);
}

/* ---- IPC 通道 ----
 * 演示多内核并存:把 KAL_DOMAIN_IPC 绑定到 micro 后,kal_ipc_send 会路由到 micro,
 * 而其它 SSI 仍走活跃 backend。
 */
kal_err_t kal_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!ch || (!data && n)) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_IPC);
    if (!b) return KAL_ENOSYS;
    if (b->ipc_send) {
        int r = b->ipc_send(ch, data, n);
        return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
    }
    if (b->syscall) {
        long r = b->syscall(KAL_NR_IPC_SEND, (long)ch, (long)data, (long)n, 0,0,0);
        return r == 0 ? KAL_OK : (kal_err_t)r;
    }
    return KAL_ENOSYS;
}

kal_err_t kal_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms) {
    kal_err_t g = ssi_guard(); if (g != KAL_OK) return g;
    if (!ch || (!buf && n)) return KAL_EBADMSG;
    const struct kernel_backend_ops *b = ops_for(KAL_DOMAIN_IPC);
    if (!b) return KAL_ENOSYS;
    if (b->ipc_recv) {
        int r = b->ipc_recv(ch, buf, n, timeout_ms);
        return r == 0 ? KAL_OK : kal_errno_from_native(b->err_src, r);
    }
    return KAL_ENOSYS;
}

/* ---- 时间(KAL 自身提供,不强求内核) ---- */
uint64_t kal_clock_gettime_ns(void) {
#if defined(KAL_KERNEL)
    uint64_t ticks = hpet_get_ticks();
    uint64_t freq = hpet_get_frequency();
    if (freq == 0) return 0;
    return ticks * 1000000000ULL / freq;
#elif defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER cnt;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    if (freq.QuadPart == 0) return 0;
    return (uint64_t)((double)cnt.QuadPart / (double)freq.QuadPart * 1e9);
#elif defined(__MACH__)
    static mach_timebase_info_data_t tb = {0};
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    return t * tb.numer / tb.denom;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

const char *kal_strerror(kal_err_t e) {
    return kal_errno_str(e);
}

/* ---- 顶层入口 ----
 * kal_init 在 main 早期调用:注册内置 backend,启动治理层。
 * 内置 backend 列表通过 kal_register_builtins() 注入(由 adapters 模块提供)。
 */
extern void kal_register_builtins(void);  /* defined in kal_builtins.c */

kal_err_t kal_init(const struct kal_cfg *cfg) {
    if (!cfg) return KAL_EBADMSG;
    kal_register_builtins();
    printf("[kal] registered %d backend(s)\n", kal_registry_count());
    return kal_governance_boot(cfg);
}

void kal_exit(void) {
    kal_governance_shutdown();
    printf("[kal] exited\n");
}
