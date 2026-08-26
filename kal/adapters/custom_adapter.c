/* SPDX-License-Identifier: MIT
 * custom_adapter.c - 内核 B(备用):自研内核
 *
 * 演示一个"能力受限但稳定"的 backend:
 *  - 不支持 io_uring(走 epoll fallback)
 *  - 不支持原生 fork(走 clone+exec fallback,但这里仍提供 fork 入口以演示)
 *  - 不支持网络栈(外壳网络面板需要降级)
 *  - 自有错误码族(CUSTOM_*)
 */
#include "kal_backend.h"
#include "kal_capability.h"
#include "kal_errno.h"

#include <stdio.h>
#include <string.h>

static int s_next_fd = 3;

static int cu_probe(void) { return 0; }

static int cu_init(const struct kal_cfg *cfg, kal_capability_bitmap_t *caps) {
    (void)cfg;
    kal_cap_clear_all(caps);
    /* 自研内核声明其能力:比 Linux 少 */
    kal_cap_set(caps, KAL_CAP_FORK);
    kal_cap_set(caps, KAL_CAP_EPOLL);
    kal_cap_set(caps, KAL_CAP_SUSPEND);
    /* 不支持:IO_URING、NET_STACK、DRM、HOTSHP */
    printf("[custom] init done, caps set (limited)\n");
    return 0;
}

static void cu_shutdown(void) {
    printf("[custom] shutdown\n");
}

static int cu_health(void) { return 0; }

static long cu_syscall(long nr, long a0, long a1, long a2,
                       long a3, long a4, long a5) {
    (void)a3; (void)a4; (void)a5;
    switch (nr) {
        case KAL_NR_OPEN:
            printf("[custom] syscall open('%s')\n", (const char *)a0);
            return s_next_fd++;
        case KAL_NR_READ:
            printf("[custom] syscall read(fd=%ld, n=%ld)\n", a0, a2);
            return a2;
        case KAL_NR_WRITE:
            printf("[custom] syscall write(fd=%ld, n=%ld)\n", a0, a2);
            return a2;
        case KAL_NR_CLOSE:
            return 0;
        case KAL_NR_SOCKET:
            /* 不支持网络栈:返回自研内核的 NOSYS */
            return -9;  /* CUSTOM_NOSYS */
        default:
            return -9;
    }
}

static int cu_open(const char *path, int flags, int mode) {
    (void)flags; (void)mode;
    printf("[custom] open('%s')\n", path ? path : "(null)");
    return s_next_fd++;
}
static int cu_close(kal_fd_t fd) { (void)fd; return 0; }
static long cu_read(kal_fd_t fd, void *buf, kal_size_t n) {
    (void)fd; (void)buf; return (long)n;
}
static long cu_write(kal_fd_t fd, const void *buf, kal_size_t n) {
    (void)fd; (void)buf; return (long)n;
}
static int cu_mmap(struct kal_mmap_args *args, void **out_addr) {
    (void)args;
    *out_addr = (void *)0x20000000ULL;
    return 0;
}
static int cu_munmap(void *addr, kal_size_t length) {
    (void)addr; (void)length; return 0;
}
static int cu_fork(kal_pid_t *out_child) {
    /* 自研内核原生 fork(虽然真实情况可能需要 clone+exec) */
    *out_child = 5678;
    printf("[custom] fork -> child=%d\n", (int)*out_child);
    return 0;
}
static int cu_exec(const char *path, char *const argv[], char *const envp[]) {
    (void)argv; (void)envp;
    printf("[custom] exec('%s')\n", path ? path : "(null)");
    return 0;
}
static int cu_wait(kal_pid_t pid, int *out_status, int options) {
    (void)pid; (void)options;
    if (out_status) *out_status = 0;
    return 0;
}
static int cu_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n) {
    (void)ch; (void)data; (void)n; return 0;
}
static int cu_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms) {
    (void)ch; (void)buf; (void)n; (void)timeout_ms; return 0;
}
/* 自研内核不支持 HOTSHP:hot_swap_pause/resume 为空,治理层走强制路径 */

const struct kernel_backend_ops custom_backend_ops = {
    .name            = "custom",
    .err_src         = KAL_ERR_SRC_CUSTOM,
    .probe           = cu_probe,
    .init            = cu_init,
    .shutdown        = cu_shutdown,
    .health_check    = cu_health,
    .syscall         = cu_syscall,
    .open            = cu_open,
    .close           = cu_close,
    .read            = cu_read,
    .write           = cu_write,
    .mmap            = cu_mmap,
    .munmap          = cu_munmap,
    .fork            = cu_fork,
    .exec            = cu_exec,
    .wait            = cu_wait,
    .ipc_send        = cu_ipc_send,
    .ipc_recv        = cu_ipc_recv,
    /* hot_swap_pause / hot_swap_resume 故意留空,演示"内核无协作"场景 */
};
