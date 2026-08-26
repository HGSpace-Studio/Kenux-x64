#ifndef KAPI_EBPF_H
#define KAPI_EBPF_H

#include <stdint.h>
#include <stddef.h>

#include <arch/bpf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* kapi 层 eBPF 接口 —— 封装底层 arch/bpf.c 实现 */

int kapi_ebpf_init(void);

/* 辅助函数注册（id < BPF_HELPER_MAX），返回 0 成功 */
typedef u64 (*kapi_bpf_helper_fn_t)(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5);
int kapi_ebpf_register_helper(int id, kapi_bpf_helper_fn_t fn);

/* Map 管理（封装 bpf_map_create/destroy/lookup/update/delete） */
bpf_map_t* kapi_ebpf_map_create(u32 type, const char* name,
                                 u32 key_size, u32 value_size,
                                 u32 max_entries);
int kapi_ebpf_map_destroy(bpf_map_t* map);

/* 程序加载/卸载（封装 bpf_prog_alloc/free） */
bpf_program_t* kapi_ebpf_prog_load(const char* name, int type,
                                    const bpf_insn_t* insns, u32 insn_cnt);
int kapi_ebpf_prog_unload(bpf_program_t* prog);

/* 校验 */
int kapi_ebpf_prog_verify(const bpf_insn_t* insns, u32 insn_cnt);

/* 程序执行：返回 r0 */
u64 kapi_ebpf_prog_run(bpf_program_t* prog, void* ctx);

/* 调试：dump 程序指令到串口 */
void kapi_ebpf_prog_dump(const bpf_program_t* prog);

#ifdef __cplusplus
}
#endif

#endif
