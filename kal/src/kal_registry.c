/* SPDX-License-Identifier: MIT
 * kal_registry.c - backend 注册表实现
 *
 * 简单线性表:内核适配器在 main 早期注册,数量有限,无需哈希。
 */
#include "kal_registry.h"
#include <string.h>

#define KAL_MAX_BACKENDS 16

static const struct kernel_backend_ops *g_backends[KAL_MAX_BACKENDS];
static int g_count = 0;

kal_err_t kal_registry_register(const struct kernel_backend_ops *ops) {
    if (!ops || !ops->name) return KAL_EBADMSG;
    if (g_count >= KAL_MAX_BACKENDS) return KAL_EBUSY;
    /* 去重:同名覆盖 */
    for (int i = 0; i < g_count; ++i) {
        if (g_backends[i] && strcmp(g_backends[i]->name, ops->name) == 0) {
            g_backends[i] = ops;
            return KAL_OK;
        }
    }
    g_backends[g_count++] = ops;
    return KAL_OK;
}

const struct kernel_backend_ops *kal_registry_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_count; ++i) {
        if (g_backends[i] && strcmp(g_backends[i]->name, name) == 0)
            return g_backends[i];
    }
    return NULL;
}

void kal_registry_foreach(kal_registry_visitor_t visit, void *user) {
    if (!visit) return;
    for (int i = 0; i < g_count; ++i) {
        if (g_backends[i]) visit(g_backends[i], user);
    }
}

int kal_registry_count(void) { return g_count; }

/* 测试辅助:清空注册表(仅 demo / 单测使用) */
void kal_registry_reset(void) {
    g_count = 0;
    memset(g_backends, 0, sizeof(g_backends));
}
