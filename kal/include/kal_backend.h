/* SPDX-License-Identifier: MIT
 * kal_backend.h - 内核适配层接口(struct kernel_backend_ops)
 *
 * 每个具体内核实现一份该结构并通过 kal_registry_register 注册。
 * 这是"新增内核只需写一个 Adapter,无需改外壳"的关键。
 */
#ifndef KAL_BACKEND_H
#define KAL_BACKEND_H

#include "kal_types.h"
#include "kal_capability.h"
#include "kal_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
struct kernel_backend_ops;
struct kal_cfg;

/* 配置参数:由治理层在 init 时传入 */
struct kal_cfg {
    const char *backend_name;        /* 期望装载的 backend 名 */
    uint32_t    heartbeat_ms;        /* 健康检查周期 */
    uint32_t    failover_threshold;  /* 连续失败多少次触发故障转移 */
    uint32_t    drain_timeout_ms;    /* 热切换 drain 超时 */
    int         allow_degrade;       /* 1=允许降级 fallback */
};

/* 单个 backend 的运行时状态 —— 由治理层维护,backend 可读但不应改 */
typedef struct {
    const struct kernel_backend_ops *ops;
    kal_capability_bitmap_t  caps;
    int                       loaded;        /* 是否已 init 成功 */
    int                       healthy;       /* 最近一次健康检查结果 */
    uint32_t                  fail_count;    /* 连续失败计数 */
    uint64_t                  last_heartbeat_ns;
} kal_backend_state_t;

/* ① 接口契约层暴露给内核侧的统一 ops。
 * 实现者把"内核私有语义"翻译到这里。
 */
struct kernel_backend_ops {
    const char *name;                              /* 如 "linuxlike"、"custom"、"micro" */
    kal_err_src_t err_src;                         /* 该 backend 的 errno 来源族 */

    /* 生命周期 */
    int  (*probe)(void);                           /* 探测能否在此环境运行,返回 0/非 0 */
    int  (*init)(const struct kal_cfg *cfg,
                 kal_capability_bitmap_t *out_caps); /* 初始化并回填能力位图 */
    void (*shutdown)(void);

    /* 健康检查:返回 0 视为健康 */
    int  (*health_check)(void);

    /* 通用 syscall 转发(用于不在专用入口的接口) */
    long (*syscall)(long nr, long a0, long a1, long a2,
                    long a3, long a4, long a5);

    /* 高频/专用接口(可空,缺失则由兼容层降级) */
    int  (*open)(const char *path, int flags, int mode);
    int  (*close)(kal_fd_t fd);
    long (*read)(kal_fd_t fd, void *buf, kal_size_t n);
    long (*write)(kal_fd_t fd, const void *buf, kal_size_t n);
    int  (*mmap)(struct kal_mmap_args *args, void **out_addr);
    int  (*munmap)(void *addr, kal_size_t length);
    int  (*fork)(kal_pid_t *out_child);
    int  (*exec)(const char *path, char *const argv[], char *const envp[]);
    int  (*wait)(kal_pid_t pid, int *out_status, int options);

    int  (*ipc_send)(kal_chan_t *ch, const void *data, kal_size_t n);
    int  (*ipc_recv)(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms);

    /* 热切换协作钩子(可空:KAL_CAP_HOTSHP=0 时治理层走强制路径) */
    int  (*hot_swap_pause)(void);                  /* 冻结新请求、刷脏 */
    int  (*hot_swap_resume)(void);                 /* 恢复,可能在新内核上 */
};

/* 内核侧辅助:声明一个 backend 并给出符号 */
#define KAL_REGISTER_BACKEND(var_name, ops_ptr) \
    const struct kernel_backend_ops *kal_backend_entry_##var_name = (ops_ptr)

#ifdef __cplusplus
}
#endif
#endif /* KAL_BACKEND_H */
