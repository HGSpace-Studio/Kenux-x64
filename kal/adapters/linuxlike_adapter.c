/* SPDX-License-Identifier: MIT
 * linuxlike_adapter.c - 内核 A(默认):类 Linux 内核
 *
 * 演示一个完整能力的 backend:支持 fork、epoll、io_uring、网络栈等。
 * 真实环境下,这里会包装 host 的 Linux syscall。
 */
#include "kal_backend.h"
#include "kal_capability.h"
#include "kal_errno.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 模拟的 fd 表(演示用,真实场景下 fd 由内核分配) */
static int s_next_fd = 3;

static int ll_probe(void) { return 0; }

static int ll_init(const struct kal_cfg *cfg, kal_capability_bitmap_t *caps) {
    (void)cfg;
    kal_cap_clear_all(caps);
    /* 类 Linux 内核声明其能力 */
    kal_cap_set(caps, KAL_CAP_FORK);
    kal_cap_set(caps, KAL_CAP_EPOLL);
    kal_cap_set(caps, KAL_CAP_IO_URING);
    kal_cap_set(caps, KAL_CAP_NET_STACK);
    kal_cap_set(caps, KAL_CAP_DRM);
    kal_cap_set(caps, KAL_CAP_SUSPEND);
    kal_cap_set(caps, KAL_CAP_SANDBOX);
    kal_cap_set(caps, KAL_CAP_HOTSHP);
    /* 注意:不支持 GPU_RENDER、MIGRATE_PROCESS */
    printf("[linuxlike] init done, caps set\n");
    return 0;
}

static void ll_shutdown(void) {
    printf("[linuxlike] shutdown\n");
}

static int ll_health(void) {
    /* 演示:始终健康;真实场景下读 /proc 或调用 getrandom 探测 */
    return 0;
}

static long ll_syscall(long nr, long a0, long a1, long a2,
                       long a3, long a4, long a5) {
    (void)a3; (void)a4; (void)a5;
    /* 简化:对常见 nr 给出合理返回值,未实现的返回 -ENOSYS(Linux 风格) */
    switch (nr) {
        case KAL_NR_OPEN: {
            const char *path = (const char *)a0;
            printf("[linuxlike] syscall open('%s', flags=%ld)\n", path ? path : "(null)", a1);
            return s_next_fd++;
        }
        case KAL_NR_READ:
            printf("[linuxlike] syscall read(fd=%ld, n=%ld)\n", a0, a2);
            return a2;  /* 假装全部读完 */
        case KAL_NR_WRITE:
            printf("[linuxlike] syscall write(fd=%ld, n=%ld)\n", a0, a2);
            return a2;
        case KAL_NR_CLOSE:
            printf("[linuxlike] syscall close(fd=%ld)\n", a0);
            return 0;
        case KAL_NR_SOCKET:
            printf("[linuxlike] syscall socket(domain=%ld, type=%ld)\n", a0, a1);
            return s_next_fd++;   /* 返回一个虚拟 socket fd */
        default:
            return -38;  /* Linux ENOSYS */
    }
}

static int ll_open(const char *path, int flags, int mode) {
    (void)mode;
    printf("[linuxlike] open('%s', flags=%d)\n", path ? path : "(null)", flags);
    return s_next_fd++;
}
static int ll_close(kal_fd_t fd) {
    printf("[linuxlike] close(%d)\n", fd);
    return 0;
}
static long ll_read(kal_fd_t fd, void *buf, kal_size_t n) {
    (void)buf;
    printf("[linuxlike] read(%d, %llu)\n", fd, (unsigned long long)n);
    return (long)n;
}
static long ll_write(kal_fd_t fd, const void *buf, kal_size_t n) {
    (void)buf;
    printf("[linuxlike] write(%d, %llu)\n", fd, (unsigned long long)n);
    return (long)n;
}
static int ll_mmap(struct kal_mmap_args *args, void **out_addr) {
    (void)args;
    /* 演示:返回一个伪造地址(真实环境用 mmap(2)) */
    *out_addr = (void *)0x10000000ULL;
    printf("[linuxlike] mmap -> %p\n", *out_addr);
    return 0;
}
static int ll_munmap(void *addr, kal_size_t length) {
    (void)addr; (void)length;
    printf("[linuxlike] munmap(%p)\n", addr);
    return 0;
}
static int ll_fork(kal_pid_t *out_child) {
    /* 演示:返回一个虚拟子进程 pid */
    *out_child = 1234;
    printf("[linuxlike] fork -> child=%d\n", (int)*out_child);
    return 0;
}
static int ll_exec(const char *path, char *const argv[], char *const envp[]) {
    (void)argv; (void)envp;
    printf("[linuxlike] exec('%s')\n", path ? path : "(null)");
    return 0;
}
static int ll_wait(kal_pid_t pid, int *out_status, int options) {
    (void)options;
    if (out_status) *out_status = 0;
    printf("[linuxlike] wait(%d)\n", pid);
    return 0;
}
static int ll_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n) {
    (void)data;
    printf("[linuxlike] ipc_send(ch=%llu, n=%llu)\n",
           (unsigned long long)ch->id, (unsigned long long)n);
    return 0;
}
static int ll_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms) {
    (void)buf; (void)timeout_ms;
    printf("[linuxlike] ipc_recv(ch=%llu, n=%llu)\n",
           (unsigned long long)ch->id, (unsigned long long)n);
    return 0;
}
static int ll_hot_swap_pause(void)  { printf("[linuxlike] hot_swap_pause\n");  return 0; }
static int ll_hot_swap_resume(void) { printf("[linuxlike] hot_swap_resume\n"); return 0; }

const struct kernel_backend_ops linuxlike_backend_ops = {
    .name            = "linuxlike",
    .err_src         = KAL_ERR_SRC_LINUXLIKE,
    .probe           = ll_probe,
    .init            = ll_init,
    .shutdown        = ll_shutdown,
    .health_check    = ll_health,
    .syscall         = ll_syscall,
    .open            = ll_open,
    .close           = ll_close,
    .read            = ll_read,
    .write           = ll_write,
    .mmap            = ll_mmap,
    .munmap          = ll_munmap,
    .fork            = ll_fork,
    .exec            = ll_exec,
    .wait            = ll_wait,
    .ipc_send        = ll_ipc_send,
    .ipc_recv        = ll_ipc_recv,
    .hot_swap_pause  = ll_hot_swap_pause,
    .hot_swap_resume = ll_hot_swap_resume,
};
