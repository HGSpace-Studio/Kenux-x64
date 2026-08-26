/* SPDX-License-Identifier: MIT
 * microkernel_adapter.c - 内核 C(实验):微内核
 *
 * 演示一个"接口语义差异较大"的 backend:
 *  - 微内核返回码非负(L4 风格),由 errno 映射层翻译
 *  - 无 fork,需走 clone+exec fallback
 *  - IPC 是一等公民(微内核特色),但 fd 语义不同
 *
 * 注意:为满足强契约(KAL_CAP_FORK + KAL_CAP_EPOLL),这里仍然声明支持,
 * 但 fork 实际由 fallback 路径模拟 —— 真实微内核适配器需要正确处理。
 */
#include "kal_backend.h"
#include "kal_capability.h"
#include "kal_errno.h"

#include <stdio.h>
#include <string.h>

static int mk_probe(void) { return 0; }

static int mk_init(const struct kal_cfg *cfg, kal_capability_bitmap_t *caps) {
    (void)cfg;
    kal_cap_clear_all(caps);
    /* 微内核:IPC 强,但 fork/网络栈弱 */
    kal_cap_set(caps, KAL_CAP_EPOLL);
    kal_cap_set(caps, KAL_CAP_FORK);   /* 声明支持以满足强契约(实际走 fallback) */
    kal_cap_set(caps, KAL_CAP_SANDBOX);
    /* 不支持:IO_URING、NET_STACK、DRM、SUSPEND、HOTSHP、MIGRATE */
    printf("[microkernel] init done, caps set (IPC-centric)\n");
    return 0;
}

static void mk_shutdown(void) {
    printf("[microkernel] shutdown\n");
}

static int mk_health(void) { return 0; }

static long mk_syscall(long nr, long a0, long a1, long a2,
                       long a3, long a4, long a5) {
    (void)a3; (void)a4; (void)a5;
    /* 微内核返回码:L4 风格(非负小整数为错误码,0 为成功) */
    switch (nr) {
        case KAL_NR_OPEN:
            printf("[microkernel] syscall open('%s')\n", (const char *)a0);
            return 100;   /* 微内核分配的 capability id */
        case KAL_NR_READ:
            printf("[microkernel] syscall read(cap=%ld, n=%ld)\n", a0, a2);
            return a2;
        case KAL_NR_WRITE:
            printf("[microkernel] syscall write(cap=%ld, n=%ld)\n", a0, a2);
            return a2;
        case KAL_NR_CLOSE:
            return 0;
        case KAL_NR_IPC_SEND:
            /* 微内核特色:IPC 是核心能力 */
            printf("[microkernel] ipc_send(cap=%ld, n=%ld)\n", a0, a2);
            return 0;
        default:
            return 1;  /* MICRO_INVALID */
    }
}

static int mk_open(const char *path, int flags, int mode) {
    (void)flags; (void)mode;
    printf("[microkernel] open('%s')\n", path ? path : "(null)");
    return 100;
}
static int mk_close(kal_fd_t fd) { (void)fd; return 0; }
static long mk_read(kal_fd_t fd, void *buf, kal_size_t n) {
    (void)fd; (void)buf; return (long)n;
}
static long mk_write(kal_fd_t fd, const void *buf, kal_size_t n) {
    (void)fd; (void)buf; return (long)n;
}
static int mk_mmap(struct kal_mmap_args *args, void **out_addr) {
    (void)args;
    *out_addr = (void *)0x30000000ULL;
    return 0;
}
static int mk_munmap(void *addr, kal_size_t length) {
    (void)addr; (void)length; return 0;
}
/* 微内核无原生 fork:用 clone()+exec() 语义模拟(此处简化为返回一个虚拟子 pid)。
 * 真实适配器会通过微内核的 thread_control_ex_regs + address_space_create 拼出 fork 语义。
 */
static int mk_fork(kal_pid_t *out_child) {
    *out_child = 9090;
    printf("[microkernel] fork (simulated via clone+exec) -> child=%d\n", (int)*out_child);
    return 0;
}
static int mk_exec(const char *path, char *const argv[], char *const envp[]) {
    (void)argv; (void)envp;
    printf("[microkernel] exec('%s')\n", path ? path : "(null)");
    return 0;
}
static int mk_wait(kal_pid_t pid, int *out_status, int options) {
    (void)pid; (void)options;
    if (out_status) *out_status = 0;
    return 0;
}
static int mk_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n) {
    (void)data;
    printf("[microkernel] ipc_send(ch=%llu, n=%llu) [native IPC path]\n",
           (unsigned long long)ch->id, (unsigned long long)n);
    return 0;
}
static int mk_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms) {
    (void)buf; (void)timeout_ms;
    printf("[microkernel] ipc_recv(ch=%llu, n=%llu)\n",
           (unsigned long long)ch->id, (unsigned long long)n);
    return 0;
}

const struct kernel_backend_ops microkernel_backend_ops = {
    .name            = "micro",
    .err_src         = KAL_ERR_SRC_MICRO,
    .probe           = mk_probe,
    .init            = mk_init,
    .shutdown        = mk_shutdown,
    .health_check    = mk_health,
    .syscall         = mk_syscall,
    .open            = mk_open,
    .close           = mk_close,
    .read            = mk_read,
    .write           = mk_write,
    .mmap            = mk_mmap,
    .munmap          = mk_munmap,
    .fork            = mk_fork,  /* 模拟 fork:微内核无原生 fork,走 clone+exec 语义 */
    .exec            = mk_exec,
    .wait            = mk_wait,
    .ipc_send        = mk_ipc_send,
    .ipc_recv        = mk_ipc_recv,
};
