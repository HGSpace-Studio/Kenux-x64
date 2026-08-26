/* SPDX-License-Identifier: MIT
 * kal_capability.h - 能力协商位图
 *
 * 启动时 KAL 与内核握手,生成 kal_capability_bitmap_t。
 * "强契约接口"必须实现;缺失即拒绝启动该内核。
 * "可选能力接口"通过位图告知外壳,外壳据此决定走原生路径还是 fallback。
 */
#ifndef KAL_CAPABILITY_H
#define KAL_CAPABILITY_H

#include "kal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAL_CAP_WORDS  4   /* 256 bit,足够覆盖 SSI 接口域 */

typedef struct {
    uint64_t bits[KAL_CAP_WORDS];
} kal_capability_bitmap_t;

/* 能力位定义(每位对应一项可选能力接口) */
#define KAL_CAP_MIGRATE_PROCESS  0   /* 支持 migrate_process */
#define KAL_CAP_GPU_RENDER       1   /* 内核提供 GPU 渲染 */
#define KAL_CAP_IO_URING         2   /* 支持 io_uring(否则降级 epoll/aio) */
#define KAL_CAP_FORK             3   /* 原生 fork(否则 clone+exec 模拟) */
#define KAL_CAP_EPOLL            4
#define KAL_CAP_NET_STACK        5   /* 内核态网络栈 */
#define KAL_CAP_DRM              6   /* 内核 DRM */
#define KAL_CAP_SUSPEND          7
#define KAL_CAP_SANDBOX          8
#define KAL_CAP_HOTSHP           9   /* 内核自身支持热切换协作 */
#define KAL_CAP_MAX              256

/* 强契约掩码:这些位必须在握手时为 1,否则 KAL 拒绝装载该 backend */
extern const kal_capability_bitmap_t KAL_STRONG_CONTRACT_MASK;

static inline void kal_cap_set(kal_capability_bitmap_t *c, unsigned bit) {
    if (bit < KAL_CAP_MAX) c->bits[bit >> 6] |= (1ULL << (bit & 63));
}
static inline int kal_cap_has(const kal_capability_bitmap_t *c, unsigned bit) {
    if (bit >= KAL_CAP_MAX) return 0;
    return (c->bits[bit >> 6] >> (bit & 63)) & 1ULL;
}
static inline void kal_cap_clear_all(kal_capability_bitmap_t *c) {
    for (int i = 0; i < KAL_CAP_WORDS; ++i) c->bits[i] = 0;
}

/* 校验强契约是否满足;满足返回 1,缺项返回 0 并把缺失位写入 missing(可空)。 */
int kal_capability_check_strong(const kal_capability_bitmap_t *caps,
                                kal_capability_bitmap_t *missing);

#ifdef __cplusplus
}
#endif
#endif /* KAL_CAPABILITY_H */
