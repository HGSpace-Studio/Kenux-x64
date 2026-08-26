/* SPDX-License-Identifier: MIT
 * kal_capability.c - 能力位图与强契约校验
 *
 * 强契约的定义:open/close/read/write/mmap/munmap 这些"最小可运行集"
 * 必须存在(由 backend 在 init 时回填 KAL_CAP_FORK 等位)。
 * 这里把强契约掩码限定为"内核必须声明的最小能力"。
 */
#include "kal_capability.h"
#include <string.h>

/* 强契约:目前要求 fork 与 epoll(否则内核连最基本的多进程/事件循环都跑不起来) */
const kal_capability_bitmap_t KAL_STRONG_CONTRACT_MASK = {
    { (1ULL << KAL_CAP_FORK) | (1ULL << KAL_CAP_EPOLL), 0, 0, 0 }
};

int kal_capability_check_strong(const kal_capability_bitmap_t *caps,
                                kal_capability_bitmap_t *missing) {
    if (!caps) return 0;
    if (missing) kal_cap_clear_all(missing);
    int ok = 1;
    for (int i = 0; i < KAL_CAP_WORDS; ++i) {
        uint64_t want = KAL_STRONG_CONTRACT_MASK.bits[i];
        uint64_t have = caps->bits[i] & want;
        if (have != want) {
            ok = 0;
            if (missing) missing->bits[i] = want & ~caps->bits[i];
        }
    }
    return ok;
}
