/* SPDX-License-Identifier: MIT
 * kal_builtins.c - 把内置 adapter 注册进 registry
 *
 * 这一层把"内置 backend 集合"与 KAL 核心解耦:删一个 adapter 不需要改 kal_ssi.c。
 * 内核构建时只注册 kenux 原生 backend;用户态 demo 构建注册全部演示 backend。
 */
#include "kal_backend.h"
#include "kal_registry.h"

#ifdef KAL_KERNEL
/* 内核构建:仅注册 Kenux 原生 backend */
extern const struct kernel_backend_ops kenux_backend_ops;

void kal_register_builtins(void) {
    kal_registry_register(&kenux_backend_ops);
}
#else
/* 用户态 demo 构建:注册全部演示 backend */
extern const struct kernel_backend_ops linuxlike_backend_ops;
extern const struct kernel_backend_ops custom_backend_ops;
extern const struct kernel_backend_ops microkernel_backend_ops;

void kal_register_builtins(void) {
    kal_registry_register(&linuxlike_backend_ops);
    kal_registry_register(&custom_backend_ops);
    kal_registry_register(&microkernel_backend_ops);
}
#endif
