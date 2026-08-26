/* SPDX-License-Identifier: MIT
 * kal_errno.c - 错误码映射实现
 *
 * 设计要点:
 *  - 三个内核族各有自己的 errno 取值规则,这里维护三张表。
 *  - 缺项统一归一到最接近语义(架构文档"四、④兼容转换层")。
 *  - 反向映射用于把统一错误码透传给具体内核(参数校验场景)。
 */
#include "kal_errno.h"
#include <string.h>

/* ---- 类 Linux errno(标准 ISO/POSIX 编号) ---- */
#define LINUX_EPERM    1
#define LINUX_ENOENT   2
#define LINUX_EIO      5
#define LINUX_ENOMEM  12
#define LINUX_EACCES  13
#define LINUX_EBADF    9
#define LINUX_EAGAIN  11
#define LINUX_EFAULT  14
#define LINUX_EBUSY   16
#define LINUX_ENOSYS  38
#define LINUX_ENOTSUP 95
#define LINUX_ETIMEDOUT 110

/* ---- 自研内核错误码(假设以 0x1000 起的负区间) ---- */
#define CUSTOM_OK            0
#define CUSTOM_DENIED       (-1)
#define CUSTOM_NOT_FOUND    (-2)
#define CUSTOM_IO_FAIL      (-3)
#define CUSTOM_NO_MEM       (-4)
#define CUSTOM_BAD_FD       (-5)
#define CUSTOM_TRY_AGAIN    (-6)
#define CUSTOM_RANGE        (-7)
#define CUSTOM_BUSY         (-8)
#define CUSTOM_NOSYS        (-9)
#define CUSTOM_TIMEOUT      (-10)

/* ---- 微内核返回码(传统 L4 风格:非负小整数为错误码) ---- */
#define MICRO_OK            0
#define MICRO_INVALID      1
#define MICRO_BAD_CAP      2
#define MICRO_NO_MEM       3
#define MICRO_ABORTED      4
#define MICRO_RETRY        5
#define MICRO_TIMEOUT      6
#define MICRO_NOT_FOUND    7

/* 单条映射 */
struct err_map { int native; kal_err_t unified; };

/* 类 Linux 表 */
static const struct err_map linuxlike_tbl[] = {
    { LINUX_EPERM,    KAL_EPERM  },
    { LINUX_ENOENT,   KAL_ENOENT },
    { LINUX_EIO,      KAL_EIO    },
    { LINUX_ENOMEM,   KAL_ENOMEM },
    { LINUX_EACCES,   KAL_EACCES },
    { LINUX_EBADF,    KAL_EBADF  },
    { LINUX_EAGAIN,   KAL_EAGAIN },
    { LINUX_EFAULT,   KAL_ENOMEM_FAULT },
    { LINUX_EBUSY,    KAL_EBUSY  },
    { LINUX_ENOSYS,   KAL_ENOSYS },
    { LINUX_ENOTSUP,  KAL_ENOTSUP },
    { LINUX_ETIMEDOUT, KAL_ETIMEDOUT },
};
/* 自研内核表 */
static const struct err_map custom_tbl[] = {
    { CUSTOM_DENIED,    KAL_EPERM  },
    { CUSTOM_NOT_FOUND, KAL_ENOENT },
    { CUSTOM_IO_FAIL,   KAL_EIO    },
    { CUSTOM_NO_MEM,    KAL_ENOMEM },
    { CUSTOM_BAD_FD,    KAL_EBADF  },
    { CUSTOM_TRY_AGAIN, KAL_EAGAIN },
    { CUSTOM_RANGE,     KAL_ENOMEM_FAULT },
    { CUSTOM_BUSY,      KAL_EBUSY  },
    { CUSTOM_NOSYS,     KAL_ENOSYS },
    { CUSTOM_TIMEOUT,   KAL_ETIMEDOUT },
};
/* 微内核表 */
static const struct err_map micro_tbl[] = {
    { MICRO_INVALID,   KAL_EPERM  },
    { MICRO_BAD_CAP,   KAL_EACCES },
    { MICRO_NO_MEM,    KAL_ENOMEM },
    { MICRO_ABORTED,   KAL_EIO    },
    { MICRO_RETRY,     KAL_EAGAIN },
    { MICRO_TIMEOUT,   KAL_ETIMEDOUT },
    { MICRO_NOT_FOUND, KAL_ENOENT },
};

static const struct err_map *select_table(kal_err_src_t src, size_t *out_n) {
    switch (src) {
        case KAL_ERR_SRC_LINUXLIKE:
            *out_n = sizeof(linuxlike_tbl)/sizeof(linuxlike_tbl[0]);
            return linuxlike_tbl;
        case KAL_ERR_SRC_CUSTOM:
            *out_n = sizeof(custom_tbl)/sizeof(custom_tbl[0]);
            return custom_tbl;
        case KAL_ERR_SRC_MICRO:
            *out_n = sizeof(micro_tbl)/sizeof(micro_tbl[0]);
            return micro_tbl;
        default:
            *out_n = 0;
            return NULL;
    }
}

kal_err_t kal_errno_from_native(kal_err_src_t src, int native) {
    /* 经验:大多数内核成功值都是 0,负数或非零都视为错误 */
    if (native == 0) return KAL_OK;

    size_t n = 0;
    const struct err_map *tbl = select_table(src, &n);
    if (tbl) {
        for (size_t i = 0; i < n; ++i) {
            if (tbl[i].native == native) return tbl[i].unified;
        }
    }
    /* 缺项归一:未知错误统一映射为 KAL_EIO,避免外壳看到陌生码崩溃 */
    return KAL_EIO;
}

int kal_errno_to_native(kal_err_src_t src, kal_err_t unified) {
    if (unified == KAL_OK) return 0;

    size_t n = 0;
    const struct err_map *tbl = select_table(src, &n);
    if (tbl) {
        for (size_t i = 0; i < n; ++i) {
            if (tbl[i].unified == unified) return tbl[i].native;
        }
    }
    /* 缺项归一:统一错误码无对应项时,给内核返回该族的"通用 I/O 错误" */
    switch (src) {
        case KAL_ERR_SRC_LINUXLIKE: return LINUX_EIO;
        case KAL_ERR_SRC_CUSTOM:    return CUSTOM_IO_FAIL;
        case KAL_ERR_SRC_MICRO:     return MICRO_ABORTED;
        default: return -1;
    }
}

const char *kal_errno_str(kal_err_t e) {
    switch (e) {
        case KAL_OK:             return "OK";
        case KAL_EPERM:          return "EPERM (operation not permitted)";
        case KAL_ENOENT:         return "ENOENT (no such entry)";
        case KAL_EIO:            return "EIO (i/o error)";
        case KAL_ENOMEM:         return "ENOMEM (out of memory)";
        case KAL_EACCES:         return "EACCES (permission denied)";
        case KAL_EBADF:          return "EBADF (bad fd)";
        case KAL_EAGAIN:         return "EAGAIN (try again)";
        case KAL_ENOMEM_FAULT:   return "EFAULT (bad address)";
        case KAL_EBUSY:          return "EBUSY (resource busy)";
        case KAL_ENOSYS:         return "ENOSYS (not implemented)";
        case KAL_ENOTSUP:        return "ENOTSUP (capability not supported)";
        case KAL_ETIMEDOUT:      return "ETIMEDOUT (timed out)";
        case KAL_EUNSTABLE:      return "EUNSTABLE (kernel unstable)";
        case KAL_ESWITCHING:     return "ESWITCHING (hot-swap in progress)";
        case KAL_ECIRCUIT:       return "ECIRCUIT (circuit breaker open)";
        case KAL_EBADMSG:        return "EBADMSG (marshalling failed)";
        default:                 return "<unknown>";
    }
}
