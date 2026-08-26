/* SPDX-License-Identifier: MIT
 * kal_marshalling.c - 兼容转换层
 *
 * 处理不同内核之间的"语义鸿沟":
 *  - 数据结构 Marshalling:统一 kal_stat ↔ 内核私有 stat
 *  - 语义降级(Fallback):io_uring → epoll/aio;fork → clone+exec
 *  - 参数校验:进入内核前做边界检查
 *
 * 这一层不直接被外壳调用,而是由 dispatcher / SSI 入口内部使用。
 */
#include "kal_backend.h"
#include "kal_errno.h"
#include "kal_capability.h"
#include <string.h>

/* ---- 参数边界检查 ----
 * 在进入内核前做最小校验,防止不稳定内核被错误输入打挂。
 */
kal_err_t kal_marshall_check_open(const char *path, int flags) {
    if (!path) return KAL_EBADMSG;
    if (path[0] == '\0') return KAL_ENOENT;
    (void)flags;
    return KAL_OK;
}

kal_err_t kal_marshall_check_rw(kal_fd_t fd, const void *buf, kal_size_t n) {
    if (fd < 0) return KAL_EBADF;
    if (n > 0 && !buf) return KAL_EBADMSG;
    return KAL_OK;
}

kal_err_t kal_marshall_check_mmap(const struct kal_mmap_args *a) {
    if (!a) return KAL_EBADMSG;
    if (a->length == 0) return KAL_EBADMSG;
    if ((a->flags & (KAL_MAP_SHARED | KAL_MAP_PRIVATE)) == 0)
        return KAL_EBADMSG;
    if (!(a->flags & KAL_MAP_ANON) && a->fd < 0) return KAL_EBADF;
    return KAL_OK;
}

/* ---- Fallback:fork 不可用时,用 clone()+exec() 模拟 ----
 * 这里只做"降级路径"的占位:真实场景下会用 backend->syscall(KAL_NR_CLONE,...)
 * 配合 backend->exec 拼出等价语义。当前演示版直接返回 ENOSYS。
 */
kal_err_t kal_fallback_fork(const struct kernel_backend_ops *ops,
                            kal_pid_t *out_child) {
    if (!ops || !out_child) return KAL_EBADMSG;
    /* 若 backend 自己实现了 fork,直接走原生;否则告警 */
    if (ops->fork) {
        int r = ops->fork(out_child);
        return r == 0 ? KAL_OK : KAL_EIO;
    }
    return KAL_ENOSYS;
}

/* ---- Fallback:io_uring 不可用时降级到 epoll/aio ----
 * 演示版只给出"决策":若 caps 不含 IO_URING,返回 1 表示应走 epoll 路径。
 */
int kal_fallback_need_epoll(const kal_capability_bitmap_t *caps) {
    return !kal_cap_has(caps, KAL_CAP_IO_URING);
}
