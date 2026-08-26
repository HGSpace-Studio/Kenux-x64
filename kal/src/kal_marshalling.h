/* SPDX-License-Identifier: MIT
 * kal_marshalling.h - 兼容转换层内部接口(供 dispatcher / SSI 使用,不对外壳暴露)
 */
#ifndef KAL_MARSHALLING_H
#define KAL_MARSHALLING_H

#include "kal_types.h"
#include "kal_backend.h"
#include "kal_capability.h"

#ifdef __cplusplus
extern "C" {
#endif

kal_err_t kal_marshall_check_open(const char *path, int flags);
kal_err_t kal_marshall_check_rw(kal_fd_t fd, const void *buf, kal_size_t n);
kal_err_t kal_marshall_check_mmap(const struct kal_mmap_args *a);

/* Fallback:fork 不可用时降级 */
kal_err_t kal_fallback_fork(const struct kernel_backend_ops *ops,
                            kal_pid_t *out_child);
/* Fallback:io_uring 不可用时降级到 epoll */
int kal_fallback_need_epoll(const kal_capability_bitmap_t *caps);

#ifdef __cplusplus
}
#endif
#endif /* KAL_MARSHALLING_H */
