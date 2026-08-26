/* SPDX-License-Identifier: MIT
 * kal_dispatcher.c - 请求路由 / 调度层
 *
 * 三个核心策略:
 *   1. 同步直通:无歧义调用直接给当前活跃 backend。
 *   2. 异步派发:KAL_CALLF_ASYNC 标记,立即返回 future,内部线程执行。
 *   3. 多内核并存路由:domain -> backend_name 表,命中则走指定 backend。
 *
 * 流量整形:热切换窗口/熔断时,非 BYPASS 调用直接返回 KAL_EBUSY / KAL_ECIRCUIT。
 */
#include "kal_dispatcher.h"
#include "kal_registry.h"
#include "kal_governance.h"
#include "kal_marshalling.h"
#include "kal_errno.h"

#include <string.h>
#include <stdio.h>

#define KAL_MAX_ROUTES 16

typedef struct {
    kal_route_entry_t entries[KAL_MAX_ROUTES];
    int               n;
    const struct kernel_backend_ops *active;   /* 当前活跃 backend */
    const char       *active_name;
    int               circuit_open;
} dispatcher_t;

static dispatcher_t D;

/* 内部:按 domain 找绑定的 backend,否则回退到活跃 backend */
static const struct kernel_backend_ops *pick_backend(uint32_t domain) {
    for (int i = 0; i < D.n; ++i) {
        if (D.entries[i].domain & domain) {
            const struct kernel_backend_ops *b = kal_registry_find(D.entries[i].backend_name);
            if (b) return b;
        }
    }
    return D.active;
}

kal_err_t kal_dispatcher_init(const char *default_backend) {
    memset(&D, 0, sizeof(D));
    if (default_backend) {
        D.active = kal_registry_find(default_backend);
        D.active_name = default_backend;
        if (!D.active) return KAL_ENOENT;
    }
    return KAL_OK;
}

kal_err_t kal_dispatcher_bind_domain(uint32_t domain, const char *backend_name) {
    if (!backend_name) return KAL_EBADMSG;
    if (!kal_registry_find(backend_name)) return KAL_ENOENT;
    if (D.n >= KAL_MAX_ROUTES) return KAL_EBUSY;
    /* 去重:同 domain 已绑定则覆盖 */
    for (int i = 0; i < D.n; ++i) {
        if (D.entries[i].domain == domain) {
            D.entries[i].backend_name = backend_name;
            return KAL_OK;
        }
    }
    D.entries[D.n].domain = domain;
    D.entries[D.n].backend_name = backend_name;
    D.n++;
    return KAL_OK;
}

const kal_backend_state_t *kal_dispatcher_active(void) {
    return kal_governance_active_state();
}

const struct kernel_backend_ops *kal_dispatcher_pick_ops(uint32_t domain) {
    return pick_backend(domain);
}

long kal_dispatcher_syscall(long nr, long a0, long a1, long a2,
                            long a3, long a4, long a5,
                            const kal_call_ctx_t *ctx) {
    /* 流量整形:热切换中且非 BYPASS 直接拒 */
    if (kal_governance_is_switching()) {
        if (!ctx || !(ctx->flags & KAL_CALLF_BYPASS))
            return KAL_ESWITCHING;
    }
    if (D.circuit_open || kal_governance_is_circuit_open()) {
        if (!ctx || !(ctx->flags & KAL_CALLF_BYPASS))
            return KAL_ECIRCUIT;
    }

    uint32_t domain = ctx ? ctx->domain : 0;
    const struct kernel_backend_ops *b = pick_backend(domain);
    if (!b || !b->syscall) return KAL_ENOSYS;

    /* 进入内核前做边界检查(由 marshalling 层提供) */
    if (nr == KAL_NR_OPEN) {
        kal_err_t e = kal_marshall_check_open((const char *)a0, (int)a1);
        if (e != KAL_OK) return e;
    } else if (nr == KAL_NR_READ || nr == KAL_NR_WRITE) {
        kal_err_t e = kal_marshall_check_rw((kal_fd_t)a0,
                                            nr == KAL_NR_READ ? (void*)a1 : (void*)a1,
                                            (kal_size_t)a2);
        if (e != KAL_OK) return e;
    }

    return b->syscall(nr, a0, a1, a2, a3, a4, a5);
}

/* 异步派发:简化实现,内联执行后立即填 future。
 * 真实实现会投递到工作线程池;此处保留语义接口,便于外壳迁移到异步模型。
 */
kal_err_t kal_dispatcher_dispatch_async(long nr, long a0, long a1,
                                        const kal_call_ctx_t *ctx,
                                        kal_future_t *out_future) {
    if (!out_future) return KAL_EBADMSG;
    if (!ctx || !(ctx->flags & KAL_CALLF_ASYNC)) {
        /* 即便调用方没标记异步,也按异步语义执行 */
    }
    long v = kal_dispatcher_syscall(nr, a0, a1, 0, 0, 0, 0, ctx);
    out_future->result = (v < 0) ? (kal_err_t)v : KAL_OK;
    out_future->value  = v;
    out_future->ready  = 1;
    return KAL_OK;
}

kal_err_t kal_future_wait(kal_future_t *f, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!f) return KAL_EBADMSG;
    /* 简化:dispatch_async 已同步填好,这里只校验 ready */
    return f->ready ? f->result : KAL_ETIMEDOUT;
}

void kal_dispatcher_notify_switched(const char *new_backend) {
    D.active = kal_registry_find(new_backend);
    D.active_name = new_backend;
}

void kal_dispatcher_circuit_open(void)  { D.circuit_open = 1; }
void kal_dispatcher_circuit_close(void) { D.circuit_open = 0; }
