/* SPDX-License-Identifier: MIT
 * kal_dispatcher.h - 请求路由 / 调度层
 *
 * 职责:
 *   - 同步直通(无歧义调用直接给当前活跃 backend)
 *   - 异步派发(GUI 高频事件、批量 I/O)
 *   - 多内核并存路由(接口域 -> backend 实例映射)
 *   - 流量整形(限流、熔断,内核不稳定时降级)
 */
#ifndef KAL_DISPATCHER_H
#define KAL_DISPATCHER_H

#include "kal_types.h"
#include "kal_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 路由表:domain -> backend 名 */
typedef struct {
    uint32_t    domain;
    const char *backend_name;
} kal_route_entry_t;

/* future/promise 句柄(简化版,演示异步派发用) */
typedef struct kal_future {
    kal_err_t   result;
    long        value;
    int         ready;
} kal_future_t;

/* ---- 公共 API ---- */

/* 初始化 dispatcher,指定默认 backend */
kal_err_t kal_dispatcher_init(const char *default_backend);

/* 注册一条路由:把某 domain 绑定到指定 backend(多内核并存) */
kal_err_t kal_dispatcher_bind_domain(uint32_t domain, const char *backend_name);

/* 取当前活跃 backend 的状态(只读视图) */
const kal_backend_state_t *kal_dispatcher_active(void);

/* 按接口域选择 backend:命中路由表则返回绑定的 backend,否则回退到活跃 backend。
 * SSI 入口统一通过本函数选 backend,使"多内核并存路由"对外壳透明。
 */
const struct kernel_backend_ops *kal_dispatcher_pick_ops(uint32_t domain);

/* 通用转发入口(同步) */
long kal_dispatcher_syscall(long nr, long a0, long a1, long a2,
                            long a3, long a4, long a5,
                            const kal_call_ctx_t *ctx);

/* 异步派发:立即返回 future,结果通过 kal_future_wait 取 */
kal_err_t kal_dispatcher_dispatch_async(long nr, long a0, long a1,
                                        const kal_call_ctx_t *ctx,
                                        kal_future_t *out_future);

/* 阻塞等待异步结果 */
kal_err_t kal_future_wait(kal_future_t *f, uint32_t timeout_ms);

/* 通知 dispatcher 活跃 backend 已切换(由治理层调用) */
void kal_dispatcher_notify_switched(const char *new_backend);

/* 熔断/恢复控制 */
void kal_dispatcher_circuit_open(void);
void kal_dispatcher_circuit_close(void);

#ifdef __cplusplus
}
#endif
#endif /* KAL_DISPATCHER_H */
