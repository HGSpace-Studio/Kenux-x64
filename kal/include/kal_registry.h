/* SPDX-License-Identifier: MIT
 * kal_registry.h - backend 注册表
 *
 * 启动时按配置注册一个或多个 backend,运行时可切换。
 */
#ifndef KAL_REGISTRY_H
#define KAL_REGISTRY_H

#include "kal_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 注册一个 backend(可在 main 早期调用) */
kal_err_t kal_registry_register(const struct kernel_backend_ops *ops);

/* 按名查找已注册的 ops(未找到返回 NULL) */
const struct kernel_backend_ops *kal_registry_find(const char *name);

/* 枚举已注册的 backend 名(用于 governance 启动时打印) */
typedef void (*kal_registry_visitor_t)(const struct kernel_backend_ops *ops,
                                       void *user);
void kal_registry_foreach(kal_registry_visitor_t visit, void *user);

/* 取已注册数量 */
int kal_registry_count(void);

#ifdef __cplusplus
}
#endif
#endif /* KAL_REGISTRY_H */
