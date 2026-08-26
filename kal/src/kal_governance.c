/* SPDX-License-Identifier: MIT
 * kal_governance.c - 运行时治理层
 *
 * 核心:热切换(Hot-Swap)状态机,对应架构文档"三、热切换时序":
 *   1. freeze   暂停接收新请求,标记 switching
 *   2. drain    等待在途请求完成(带超时)
 *   3. checkpoint 序列化外壳可见状态(fd/映射/信号表)
 *   4. switch   卸载旧 Adapter
 *   5. load     加载新 Adapter
 *   6. restore  在新内核上重建状态
 *   7. resume   放行请求,标记降级窗口
 *
 * 状态边界原则:只 checkpoint "外壳可见状态",内核私有状态(页表、调度队列)
 * 不迁移,切换后由新内核重建。
 */
#include "kal_governance.h"
#include "kal_registry.h"
#include "kal_dispatcher.h"
#include "kal_capability.h"
#include "kal_errno.h"
#include "kal_marshalling.h"

#include <string.h>
#include <stdio.h>

#if defined(KAL_KERNEL)
#include <arch/hpet.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

/* ---- 内部状态 ---- */
typedef struct {
    kal_backend_state_t  active;
    kal_backend_state_t  standby;        /* 热切换时新装载的 backend */
    kal_svc_state_t      state;
    kal_shell_state_t    shadow;         /* checkpoint 的外壳可见状态 */
    struct kal_cfg       cfg;
    int                  booted;
    int                  circuit_open;
    int                  degrade_window; /* resume 后置位一段时间 */
    uint64_t             switch_started_ns;
} governance_t;

static governance_t G;

/* 平台无关的纳秒时钟(用于心跳/超时) */
static uint64_t now_ns(void) {
#if defined(KAL_KERNEL)
    uint64_t ticks = hpet_get_ticks();
    uint64_t freq = hpet_get_frequency();
    if (freq == 0) return 0;
    return ticks * 1000000000ULL / freq;
#elif defined(_WIN32)
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    if (freq.QuadPart == 0) return 0;
    return (uint64_t)((double)cnt.QuadPart / (double)freq.QuadPart * 1e9);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* 把 backend 装载为给定 state:probe->init->能力校验 */
static kal_err_t load_backend(const struct kernel_backend_ops *ops,
                              const struct kal_cfg *cfg,
                              kal_backend_state_t *out) {
    if (!ops) return KAL_ENOENT;
    if (ops->probe && ops->probe() != 0) {
        printf("[kal] backend '%s' probe failed\n", ops->name);
        return KAL_EIO;
    }
    kal_capability_bitmap_t caps;
    kal_cap_clear_all(&caps);
    if (ops->init) {
        int r = ops->init(cfg, &caps);
        if (r != 0) {
            printf("[kal] backend '%s' init failed: %d\n", ops->name, r);
            return KAL_EIO;
        }
    }
    /* 强契约校验:缺项则拒绝装载 */
    kal_capability_bitmap_t missing;
    if (!kal_capability_check_strong(&caps, &missing)) {
        printf("[kal] backend '%s' violates strong contract (missing bits=0x%llx)\n",
                ops->name, (unsigned long long)missing.bits[0]);
        return KAL_ENOSYS;
    }
    memset(out, 0, sizeof(*out));
    out->ops      = ops;
    out->caps     = caps;
    out->loaded   = 1;
    out->healthy  = 1;
    out->fail_count = 0;
    out->last_heartbeat_ns = now_ns();
    return KAL_OK;
}

kal_err_t kal_governance_boot(const struct kal_cfg *cfg) {
    if (!cfg || !cfg->backend_name) return KAL_EBADMSG;
    memset(&G, 0, sizeof(G));
    G.cfg = *cfg;
    G.cfg.heartbeat_ms      = cfg->heartbeat_ms      ? cfg->heartbeat_ms      : 1000;
    G.cfg.failover_threshold= cfg->failover_threshold? cfg->failover_threshold: 3;
    G.cfg.drain_timeout_ms  = cfg->drain_timeout_ms  ? cfg->drain_timeout_ms  : 500;

    const struct kernel_backend_ops *ops = kal_registry_find(cfg->backend_name);
    if (!ops) {
        printf("[kal] backend '%s' not registered\n", cfg->backend_name);
        return KAL_ENOENT;
    }
    kal_err_t e = load_backend(ops, &G.cfg, &G.active);
    if (e != KAL_OK) return e;

    kal_dispatcher_init(ops->name);
    G.state  = KAL_SVC_RUN;
    G.booted = 1;
    printf("[kal] governance booted, active='%s'\n", ops->name);
    return KAL_OK;
}

void kal_governance_shutdown(void) {
    if (!G.booted) return;
    if (G.active.ops && G.active.ops->shutdown) {
        G.active.ops->shutdown();
    }
    if (G.standby.ops && G.standby.loaded && G.standby.ops->shutdown) {
        G.standby.ops->shutdown();
    }
    memset(&G, 0, sizeof(G));
}

int kal_governance_health_tick(void) {
    if (!G.booted || !G.active.ops) return 0;
    int healthy = G.active.ops->health_check
                  ? (G.active.ops->health_check() == 0)
                  : 1;
    G.active.last_heartbeat_ns = now_ns();
    if (healthy) {
        G.active.healthy = 1;
        G.active.fail_count = 0;
    } else {
        G.active.healthy = 0;
        G.active.fail_count++;
        printf("[kal] health check failed (count=%u threshold=%u)\n",
               G.active.fail_count, G.cfg.failover_threshold);
        if (G.active.fail_count >= G.cfg.failover_threshold) {
            /* 触发故障转移:先熔断,再尝试切到任一其它已注册 backend */
            kal_dispatcher_circuit_open();
            G.circuit_open = 1;
            const char *cur = G.active.ops->name;
            const char *next = NULL;
            /* 简单策略:在注册表里找第一个非当前 backend */
            for (int i = 0; i < kal_registry_count() && !next; ++i) {
                /* 遍历靠 visitor 拿不到索引,这里直接按名扫常见候选 */
                (void)i;
            }
            (void)cur; (void)next;
            /* 演示版只打印,真实切换由 kal_governance_hot_swap 显式触发 */
        }
    }
    return healthy ? 0 : -1;
}

kal_svc_state_t kal_governance_state(void) { return G.state; }
const kal_backend_state_t *kal_governance_active_state(void) {
    return G.booted ? &G.active : NULL;
}
int kal_governance_is_switching(void) {
    return G.state != KAL_SVC_RUN;
}
int kal_governance_is_circuit_open(void) {
    return G.circuit_open;
}

/* ---- 外壳可见状态的 checkpoint / restore ----
 * 只迁移外壳可见状态(fd 表、内存映射元数据、信号处理表计数)。
 * 内核私有状态(页表/调度队列/缓存)不迁移,切换后由新内核重建。
 */
static void checkpoint_shell(kal_shell_state_t *out) {
    memset(out, 0, sizeof(*out));
    /* 真实实现会从 dispatcher 的 fd 表与 mmap 登记簿读取。
     * 这里用占位值演示数据流。 */
    out->fd_table_len = 0;
    out->mmap_records_len = 0;
    out->sig_handlers_count = 0;
}

static kal_err_t restore_shell(const kal_shell_state_t *s,
                               const struct kernel_backend_ops *new_ops) {
    if (!s || !new_ops) return KAL_EBADMSG;
    /* 在新内核上重建 fd / mmap。
     * 真实实现:对每个 fd 调用 new_ops->open(path,...) 重新打开,
     * 对每条 mmap 调用 new_ops->mmap(args,&addr) 重建映射。
     * 这里只演示遍历。 */
    for (int i = 0; i < s->mmap_records_len; ++i) {
        struct kal_mmap_args a = {
            .addr = s->mmap_records[i].addr,
            .length = s->mmap_records[i].length,
            .prot   = s->mmap_records[i].prot,
            .flags  = s->mmap_records[i].flags,
            .fd     = s->mmap_records[i].fd,
            .offset = s->mmap_records[i].offset,
        };
        void *out_addr = NULL;
        if (new_ops->mmap) new_ops->mmap(&a, &out_addr);
    }
    return KAL_OK;
}

/* 7 步热切换 */
kal_err_t kal_governance_hot_swap(const char *new_backend) {
    if (!G.booted) return KAL_EUNSTABLE;
    if (G.state != KAL_SVC_RUN) return KAL_ESWITCHING;
    const struct kernel_backend_ops *new_ops = kal_registry_find(new_backend);
    if (!new_ops) return KAL_ENOENT;
    if (new_ops == G.active.ops) return KAL_OK;  /* 同一个,无需切换 */

    printf("[kal] hot-swap: '%s' -> '%s'\n",
           G.active.ops->name, new_ops->name);
    G.switch_started_ns = now_ns();

    /* 1. FREEZE:暂停接收新请求,标记 switching */
    G.state = KAL_SVC_FREEZE;
    kal_dispatcher_circuit_open();
    G.circuit_open = 1;

    /* 2. DRAIN:等待在途请求完成(简化版:无并发,直接进入下一步) */
    G.state = KAL_SVC_DRAIN;
    /* 真实实现:等待 in_flight == 0,带 drain_timeout_ms 超时强制放弃 */

    /* 3. CHECKPOINT:序列化外壳可见状态 */
    G.state = KAL_SVC_CHECKPOINT;
    checkpoint_shell(&G.shadow);

    /* 4. SWITCH:卸载旧 Adapter */
    G.state = KAL_SVC_SWITCH;
    if (G.active.ops->hot_swap_pause) {
        G.active.ops->hot_swap_pause();
    }
    if (G.active.ops->shutdown) {
        G.active.ops->shutdown();
    }
    G.active.loaded = 0;

    /* 5. LOAD:加载新 Adapter */
    G.state = KAL_SVC_LOAD;
    kal_err_t e = load_backend(new_ops, &G.cfg, &G.standby);
    if (e != KAL_OK) {
        /* 装载失败:尝试回退到原 backend(简化:这里只报错并清状态) */
        printf("[kal] hot-swap load failed: %s, attempting rollback\n",
                kal_errno_str(e));
        G.state = KAL_SVC_RUN;
        kal_dispatcher_circuit_close();
        G.circuit_open = 0;
        return e;
    }

    /* 6. RESTORE:在新内核上重建外壳可见状态 */
    G.state = KAL_SVC_RESTORE;
    restore_shell(&G.shadow, new_ops);

    /* 7. RESUME:切换 active,放行请求,标记降级窗口 */
    G.state = KAL_SVC_RESUME;
    G.active = G.standby;
    memset(&G.standby, 0, sizeof(G.standby));
    kal_dispatcher_notify_switched(new_ops->name);
    kal_dispatcher_circuit_close();
    G.circuit_open = 0;
    G.degrade_window = 1;

    G.state = KAL_SVC_RUN;
    uint64_t elapsed_ms = (now_ns() - G.switch_started_ns) / 1000000ULL;
    printf("[kal] hot-swap complete, active='%s' (%llu ms, degrade window)\n",
           new_ops->name, (unsigned long long)elapsed_ms);
    return KAL_OK;
}

kal_version_info_t kal_governance_version(void) {
    kal_version_info_t v;
    v.ssi_major = 1; v.ssi_minor = 0;
    v.backend_major = G.active.ops ? 1 : 0;
    v.backend_minor = 0;
    /* 简化:同主版本即兼容 */
    v.compatible = (v.ssi_major == v.backend_major) ? 1 : 0;
    return v;
}
