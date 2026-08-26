/* SPDX-License-Identifier: MIT
 * kal.h - KAL 总入口(umbrella header)
 *
 * 外壳层包含本文件即可使用全部稳定接口。
 * 内核适配层只需包含 kal_backend.h + kal_capability.h。
 */
#ifndef KAL_H
#define KAL_H

#include "kal_types.h"
#include "kal_errno.h"
#include "kal_capability.h"
#include "kal_backend.h"
#include "kal_registry.h"
#include "kal_dispatcher.h"
#include "kal_governance.h"
#include "kal_ssi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 顶层初始化:读 cfg,注册内置 backend,启动治理层。
 * 成功后即可调用 kal_open/kal_read 等 SSI 入口。
 */
kal_err_t kal_init(const struct kal_cfg *cfg);

/* 顶层终止:逆序关闭。 */
void kal_exit(void);

#ifdef __cplusplus
}
#endif
#endif /* KAL_H */
