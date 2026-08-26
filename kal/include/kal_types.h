/* SPDX-License-Identifier: MIT
 * kal_types.h - KAL 公共基础类型
 *
 * 这些类型对"外壳层"和"内核适配层"都可见,且必须保持 ABI 稳定。
 * 任何修改都意味着破坏 SSI 契约,需要走版本协商流程。
 */
#ifndef KAL_TYPES_H
#define KAL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 基本别名 ---- */
typedef int32_t            kal_fd_t;        /* 统一文件描述符 */
typedef int64_t            kal_off_t;       /* 统一偏移量 */
typedef int32_t            kal_pid_t;       /* 统一进程 ID */
typedef uint64_t           kal_size_t;
typedef uint32_t           kal_flags_t;

/* ---- 错误码 ----
 * KAL 自有错误码均为负数;>=0 表示成功(数值含义由调用方约定,如 fd/pid/字节数)。
 * 与具体内核 errno 的映射规则见 kal_errno.h。
 */
typedef int32_t            kal_err_t;

#define KAL_OK             (0)
#define KAL_ERR_BASE       (-1000)

/* ---- IPC 通道 ---- */
typedef struct kal_chan {
    uint64_t id;          /* 通道标识,由内核分配 */
    uint32_t backend_tag; /* 标记属于哪个内核实例,跨内核不可复用 */
    uint32_t reserved;
} kal_chan_t;

/* ---- 统一 stat 结构(Marshalling 目标) ---- */
struct kal_stat {
    kal_fd_t      st_fd;            /* 由内核填回的真实句柄 */
    kal_off_t     st_size;
    uint32_t      st_mode;
    uint32_t      st_blksize;
    uint64_t      st_blocks;
    uint64_t      st_atime_ns;
    uint64_t      st_mtime_ns;
    uint64_t      st_ctime_ns;
};

/* ---- 内存映射参数 ---- */
struct kal_mmap_args {
    void       *addr;       /* 建议地址,可为 NULL */
    kal_size_t  length;
    int         prot;       /* KAL_PROT_* */
    int         flags;      /* KAL_MAP_* */
    kal_fd_t    fd;
    kal_off_t   offset;
};

#define KAL_PROT_READ   0x1
#define KAL_PROT_WRITE  0x2
#define KAL_PROT_EXEC   0x4

#define KAL_MAP_SHARED  0x01
#define KAL_MAP_PRIVATE 0x02
#define KAL_MAP_ANON    0x04
#define KAL_MAP_FIXED   0x08

/* ---- 请求编号(对应 SSI 接口域) ----
 * 用于 syscall 通用转发,与具体内核解耦。
 */
enum kal_syscall_nr {
    KAL_NR_OPEN       = 1,
    KAL_NR_CLOSE      = 2,
    KAL_NR_READ       = 3,
    KAL_NR_WRITE      = 4,
    KAL_NR_MMAP       = 5,
    KAL_NR_MUNMAP     = 6,
    KAL_NR_FORK       = 7,
    KAL_NR_EXEC       = 8,
    KAL_NR_WAIT       = 9,
    KAL_NR_IOCTL      = 10,
    KAL_NR_SOCKET     = 11,
    KAL_NR_SENDMSG    = 12,
    KAL_NR_RECVMSG    = 13,
    KAL_NR_IPC_SEND   = 14,
    KAL_NR_IPC_RECV   = 15,
    KAL_NR_SCHED_SETAFF = 16,
    KAL_NR_MAX
};

/* ---- 调用上下文:供 dispatcher / 治理层做路由与降级决策 ---- */
typedef struct kal_call_ctx {
    uint32_t            domain;       /* KAL_DOMAIN_* */
    uint32_t            flags;        /* KAL_CALLF_* */
    uint64_t            deadline_ns;  /* 0 表示无超时 */
    const char         *caller;       /* 调用方标识(便于 tracing) */
} kal_call_ctx_t;

#define KAL_DOMAIN_PROCESS   0x01
#define KAL_DOMAIN_MEMORY    0x02
#define KAL_DOMAIN_FS        0x04
#define KAL_DOMAIN_DEVIO     0x08
#define KAL_DOMAIN_NET       0x10
#define KAL_DOMAIN_IPC       0x20
#define KAL_DOMAIN_TIME      0x40
#define KAL_DOMAIN_GUI       0x80
#define KAL_DOMAIN_SECURITY  0x100
#define KAL_DOMAIN_POWER     0x200

#define KAL_CALLF_SYNC       0x00   /* 默认同步直通 */
#define KAL_CALLF_ASYNC      0x01   /* 异步派发,future/promise */
#define KAL_CALLF_BYPASS     0x02   /* 热切换窗口内允许的"必行"调用 */

#ifdef __cplusplus
}
#endif
#endif /* KAL_TYPES_H */
