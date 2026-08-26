/* SPDX-License-Identifier: MIT
 * kal_errno.h - 统一错误码表 + 各内核 errno 映射规则
 *
 * 对应架构文档"七、后续可选工作"第 4 项:
 *   "定义 kal_err_t 错误码表与各内核 errno 的映射规则。"
 */
#ifndef KAL_ERRNO_H
#define KAL_ERRNO_H

#include "kal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- KAL 统一错误码(语义稳定,与内核无关) ---- */
#define KAL_EPERM            (KAL_ERR_BASE - 1)    /* 操作不允许 */
#define KAL_ENOENT           (KAL_ERR_BASE - 2)    /* 不存在 */
#define KAL_EIO              (KAL_ERR_BASE - 3)    /* 底层 I/O 错误 */
#define KAL_ENOMEM           (KAL_ERR_BASE - 4)    /* 内存不足 */
#define KAL_EACCES           (KAL_ERR_BASE - 5)    /* 权限不足 */
#define KAL_EBADF            (KAL_ERR_BASE - 6)    /* 错误的 fd */
#define KAL_EAGAIN           (KAL_ERR_BASE - 7)    /* 重试 */
#define KAL_ENOMEM_FAULT     (KAL_ERR_BASE - 8)    /* 地址越界 */
#define KAL_EBUSY            (KAL_ERR_BASE - 9)    /* 资源忙(如热切换中) */
#define KAL_ENOSYS           (KAL_ERR_BASE - 10)   /* 接口未实现 */
#define KAL_ENOTSUP          (KAL_ERR_BASE - 11)   /* 能力不支持(可降级) */
#define KAL_ETIMEDOUT        (KAL_ERR_BASE - 12)   /* 超时 */
#define KAL_EUNSTABLE        (KAL_ERR_BASE - 13)   /* 内核不稳定(治理层触发) */
#define KAL_ESWITCHING       (KAL_ERR_BASE - 14)   /* 正在热切换 */
#define KAL_ECIRCUIT         (KAL_ERR_BASE - 15)   /* 熔断中 */
#define KAL_EBADMSG          (KAL_ERR_BASE - 16)   /* 参数 marshalling 失败 */

/* ---- 已知内核族 errno 标识 ----
 * 新增内核时在此追加一个标签,并在 kal_errno.c 提供映射表。
 */
typedef enum {
    KAL_ERR_SRC_LINUXLIKE = 1,   /* 类 Linux errno(ISO C 风格) */
    KAL_ERR_SRC_CUSTOM    = 2,   /* 自研内核错误码 */
    KAL_ERR_SRC_MICRO     = 3,   /* 微内核返回码 */
} kal_err_src_t;

/* 将"某内核的原始错误码"翻译为统一的 kal_err_t。
 * src 标识来源内核,native 是该内核返回的原始码。
 * 缺项归一到最接近语义(见 kal_errno.c 的映射表)。
 */
kal_err_t kal_errno_from_native(kal_err_src_t src, int native);

/* 反向:把 kal_err_t 翻译为目标内核可识别的 errno(用于参数透传)。 */
int kal_errno_to_native(kal_err_src_t src, kal_err_t unified);

/* 给出可读字符串,便于 tracing。 */
const char *kal_errno_str(kal_err_t e);

#ifdef __cplusplus
}
#endif
#endif /* KAL_ERRNO_H */
