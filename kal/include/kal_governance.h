/* SPDX-License-Identifier: MIT
 * kal_governance.h - 运行时治理层
 *
 * 职责:
 *   - 能力协商(启动握手)
 *   - 健康检查 / 心跳(达到阈值 -> 故障转移)
 *   - 热切换(Hot-Swap)状态机
 *   - 版本协商(接口版本号 + 兼容性矩阵)
 */
#ifndef KAL_GOVERNANCE_H
#define KAL_GOVERNANCE_H

#include "kal_types.h"
#include "kal_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 热切换状态机 */
typedef enum {
    KAL_SVC_RUN       = 0,   /* 正常服务 */
    KAL_SVC_FREEZE    = 1,   /* 暂停接收新请求,标记 switching */
    KAL_SVC_DRAIN     = 2,   /* 等待在途请求完成(带超时) */
    KAL_SVC_CHECKPOINT= 3,   /* 序列化外壳可见状态(fd/映射/信号表) */
    KAL_SVC_SWITCH    = 4,   /* 卸载旧 Adapter */
    KAL_SVC_LOAD      = 5,   /* 加载新 Adapter */
    KAL_SVC_RESTORE   = 6,   /* 在新内核上重建状态 */
    KAL_SVC_RESUME    = 7,   /* 放行请求,标记降级窗口 */
} kal_svc_state_t;

/* 外壳可见状态(热切换时需要 checkpoint/restore 的最小集合) */
typedef struct {
    /* 文件描述符表:统一 fd -> 内核私有句柄 */
    int    fd_table[64];
    int    fd_table_len;
    /* 内存映射登记簿(简化:只记元数据,真实数据由内核重建) */
    struct {
        void       *addr;
        kal_size_t  length;
        int         prot;
        int         flags;
        kal_fd_t    fd;
        kal_off_t   offset;
    } mmap_records[16];
    int    mmap_records_len;
    /* 信号处理表占位(此处只记录个数,真实表项由治理层在实现里维护) */
    int    sig_handlers_count;
} kal_shell_state_t;

/* ---- 公共 API ---- */

/* 启动治理层:首次装载 default backend,握手并校验强契约 */
kal_err_t kal_governance_boot(const struct kal_cfg *cfg);

/* 停止所有 backend,释放资源 */
void kal_governance_shutdown(void);

/* 主动发起热切换:从当前活跃 backend 切到 new_backend */
kal_err_t kal_governance_hot_swap(const char *new_backend);

/* 健康检查单次触发(治理层内部线程周期调用,这里暴露给测试) */
int kal_governance_health_tick(void);

/* 取当前状态机阶段 */
kal_svc_state_t kal_governance_state(void);

/* 取当前活跃 backend 状态(包含 caps/healthy/fail_count) */
const kal_backend_state_t *kal_governance_active_state(void);

/* 供 dispatcher / SSI 查询:当前是否在热切换窗口 */
int kal_governance_is_switching(void);

/* 供 dispatcher / SSI 查询:是否处于熔断 */
int kal_governance_is_circuit_open(void);

/* 版本协商:返回当前 SSI 版本号 + 兼容矩阵 */
typedef struct {
    uint16_t ssi_major;
    uint16_t ssi_minor;
    uint16_t backend_major;
    uint16_t backend_minor;
    int      compatible;
} kal_version_info_t;
kal_version_info_t kal_governance_version(void);

#ifdef __cplusplus
}
#endif
#endif /* KAL_GOVERNANCE_H */
