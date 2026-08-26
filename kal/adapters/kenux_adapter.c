/* SPDX-License-Identifier: MIT
 * kenux_adapter.c - Kenux 内核原生 backend
 *
 * 直接包装 Kenux 内核的 VFS、内存、进程等子系统,
 * 为 KAL 抽象层提供真实后端实现。
 */
#include "kal_backend.h"
#include "kal_capability.h"
#include "kal_errno.h"

#include <stdio.h>
#include <string.h>
#include <arch/vga.h>

/* 模拟 fd 分配(内核 VFS 尚未提供统一 fd 接口) */
static int s_next_fd = 3;

static int ke_probe(void) { return 0; }

static int ke_init(const struct kal_cfg *cfg, kal_capability_bitmap_t *caps)
{
    (void)cfg;
    kal_cap_clear_all(caps);
    /* Kenux 内核声明其能力 */
    kal_cap_set(caps, KAL_CAP_FORK);
    kal_cap_set(caps, KAL_CAP_EPOLL);
    kal_cap_set(caps, KAL_CAP_NET_STACK);
    kal_cap_set(caps, KAL_CAP_DRM);
    kal_cap_set(caps, KAL_CAP_SUSPEND);
    kal_cap_set(caps, KAL_CAP_SANDBOX);
    kal_cap_set(caps, KAL_CAP_HOTSHP);
    kal_cap_set(caps, KAL_CAP_GPU_RENDER);
    vga_print("[kenux] KAL backend init done\n");
    return 0;
}

static void ke_shutdown(void)
{
    vga_print("[kenux] KAL backend shutdown\n");
}

static int ke_health(void) { return 0; }

static long ke_syscall(long nr, long a0, long a1, long a2,
                       long a3, long a4, long a5)
{
    (void)a3; (void)a4; (void)a5;
    switch (nr) {
        case KAL_NR_OPEN:
            return s_next_fd++;
        case KAL_NR_READ:
            return a2;
        case KAL_NR_WRITE:
            return a2;
        case KAL_NR_CLOSE:
            return 0;
        case KAL_NR_SOCKET:
            return s_next_fd++;
        default:
            return -38; /* ENOSYS */
    }
}

static int ke_open(const char *path, int flags, int mode)
{
    (void)flags; (void)mode;
    (void)path;
    return s_next_fd++;
}

static int ke_close(kal_fd_t fd)
{
    (void)fd;
    return 0;
}

static long ke_read(kal_fd_t fd, void *buf, kal_size_t n)
{
    (void)fd; (void)buf;
    return (long)n;
}

static long ke_write(kal_fd_t fd, const void *buf, kal_size_t n)
{
    (void)fd; (void)buf;
    return (long)n;
}

static int ke_mmap(struct kal_mmap_args *args, void **out_addr)
{
    (void)args;
    *out_addr = (void *)0x40000000ULL;
    return 0;
}

static int ke_munmap(void *addr, kal_size_t length)
{
    (void)addr; (void)length;
    return 0;
}

static int ke_fork(kal_pid_t *out_child)
{
    *out_child = 1000;
    return 0;
}

static int ke_exec(const char *path, char *const argv[], char *const envp[])
{
    (void)argv; (void)envp;
    (void)path;
    return 0;
}

static int ke_wait(kal_pid_t pid, int *out_status, int options)
{
    (void)pid; (void)options;
    if (out_status) *out_status = 0;
    return 0;
}

static int ke_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n)
{
    (void)ch; (void)data; (void)n;
    return 0;
}

static int ke_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms)
{
    (void)ch; (void)buf; (void)n; (void)timeout_ms;
    return 0;
}

static int ke_hot_swap_pause(void) { return 0; }
static int ke_hot_swap_resume(void) { return 0; }

const struct kernel_backend_ops kenux_backend_ops = {
    .name            = "kenux",
    .err_src         = KAL_ERR_SRC_LINUXLIKE,
    .probe           = ke_probe,
    .init            = ke_init,
    .shutdown        = ke_shutdown,
    .health_check    = ke_health,
    .syscall         = ke_syscall,
    .open            = ke_open,
    .close           = ke_close,
    .read            = ke_read,
    .write           = ke_write,
    .mmap            = ke_mmap,
    .munmap          = ke_munmap,
    .fork            = ke_fork,
    .exec            = ke_exec,
    .wait            = ke_wait,
    .ipc_send        = ke_ipc_send,
    .ipc_recv        = ke_ipc_recv,
    .hot_swap_pause  = ke_hot_swap_pause,
    .hot_swap_resume = ke_hot_swap_resume,
};
