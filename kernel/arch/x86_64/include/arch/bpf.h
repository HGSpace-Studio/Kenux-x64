#ifndef _ARCH_BPF_H
#define _ARCH_BPF_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define BPF_REG_0       0
#define BPF_REG_1       1
#define BPF_REG_2       2
#define BPF_REG_3       3
#define BPF_REG_4       4
#define BPF_REG_5       5
#define BPF_REG_6       6
#define BPF_REG_7       7
#define BPF_REG_8       8
#define BPF_REG_9       9
#define BPF_REG_10      10
#define BPF_MAX_REGS     11

#define BPF_CLASS(code)         ((code) & 0x07)
#define BPF_LD          0x00
#define BPF_LDX         0x01
#define BPF_ST          0x02
#define BPF_STX         0x03
#define BPF_ALU         0x04
#define BPF_JMP         0x05
#define BPF_JMP32       0x06
#define BPF_ALU64       0x07

#define BPF_SIZE(code)          ((code) & 0x18)
#define BPF_W           0x00
#define BPF_H           0x08
#define BPF_B           0x10
#define BPF_DW          0x18

#define BPF_MODE(code)          ((code) & 0xE0)
#define BPF_IMM         0x00
#define BPF_ABS         0x20
#define BPF_IND         0x40
#define BPF_MEM         0x60
#define BPF_ATOMIC      0xC0

#define BPF_OP(code)            ((code) & 0xF0)
#define BPF_ADD         0x00
#define BPF_SUB         0x10
#define BPF_MUL         0x20
#define BPF_DIV         0x30
#define BPF_OR          0x40
#define BPF_AND         0x50
#define BPF_LSH         0x60
#define BPF_RSH         0x70
#define BPF_NEG         0x80
#define BPF_MOD         0x90
#define BPF_XOR         0xA0
#define BPF_MOV         0xB0
#define BPF_ARSH        0xC0
#define BPF_END         0xD0

#define BPF_JA          0x00
#define BPF_JEQ         0x10
#define BPF_JGT         0x20
#define BPF_JGE         0x30
#define BPF_JSET        0x40
#define BPF_JNE         0x50
#define BPF_JSGT        0x60
#define BPF_JSGE        0x70
#define BPF_CALL        0x80
#define BPF_EXIT        0x90
#define BPF_JLT         0xA0
#define BPF_JLE         0xB0
#define BPF_JSLT        0xC0
#define BPF_JSLE        0xD0

#define BPF_SRC(code)           ((code) & 0x08)
#define BPF_K           0x00
#define BPF_X           0x08

#define BPF_PSEUDO_MAP_FD       1
#define BPF_PSEUDO_CALL         2

#define BPF_MAP_TYPE_UNSPEC     0
#define BPF_MAP_TYPE_HASH       1
#define BPF_MAP_TYPE_ARRAY      2
#define BPF_MAP_TYPE_PROG_ARRAY 3
#define BPF_MAP_TYPE_PERF_EVENT_ARRAY 4
#define BPF_MAP_TYPE_PERCPU_HASH 5
#define BPF_MAP_TYPE_PERCPU_ARRAY 6
#define BPF_MAP_TYPE_STACK_TRACE 7
#define BPF_MAP_TYPE_CGROUP_ARRAY 8
#define BPF_MAP_TYPE_LRU_HASH    11
#define BPF_MAP_TYPE_LRU_PERCPU_HASH 12
#define BPF_MAP_TYPE_LPM_TRIE    13
#define BPF_MAP_TYPE_ARRAY_OF_MAPS 14
#define BPF_MAP_TYPE_HASH_OF_MAPS 15
#define BPF_MAP_TYPE_DEVMAP      16
#define BPF_MAP_TYPE_SOCKMAP     17
#define BPF_MAP_TYPE_CPUMAP      18

#define BPF_ANY         0
#define BPF_NOEXIST     1
#define BPF_EXIST       2

#define MAX_INSNS       4096
#define BPF_STACK_SIZE  512
#define BPF_HELPER_MAX  64
#define BPF_PROG_MAX    256
#define BPF_JIT_SIZE    (64 * 1024)

#define BPF_PROG_TYPE_UNSPEC    0
#define BPF_PROG_TYPE_SOCKET_FILTER 1
#define BPF_PROG_TYPE_KPROBE    2
#define BPF_PROG_TYPE_SCHED_CLS 3
#define BPF_PROG_TYPE_SCHED_ACT 4
#define BPF_PROG_TYPE_TRACEPOINT 5
#define BPF_PROG_TYPE_XDP       6
#define BPF_PROG_TYPE_PERF_EVENT 7
#define BPF_PROG_TYPE_CGROUP_SKB 8
#define BPF_PROG_TYPE_CGROUP_SOCK 9
#define BPF_PROG_TYPE_LWT_IN     10
#define BPF_PROG_TYPE_LWT_OUT    11
#define BPF_PROG_TYPE_LWT_XMIT   12
#define BPF_PROG_TYPE_LWT_SEG6LOCAL 13

#define BPF_FUNC_unspec                  0
#define BPF_FUNC_map_lookup_elem          1
#define BPF_FUNC_map_update_elem          2
#define BPF_FUNC_map_delete_elem          3
#define BPF_FUNC_probe_read               4
#define BPF_FUNC_ktime_get_ns             5
#define BPF_FUNC_trace_printk             6
#define BPF_FUNC_get_prandom_u32          7
#define BPF_FUNC_get_smp_processor_id     8
#define BPF_FUNC_skb_store_bytes          9
#define BPF_FUNC_l3_csum_replace          10
#define BPF_FUNC_l4_csum_replace          11
#define BPF_FUNC_tail_call                12
#define BPF_FUNC_clone_redirect           13
#define BPF_FUNC_get_current_pid_tgid     14
#define BPF_FUNC_get_current_uid_gid      15
#define BPF_FUNC_get_current_comm         16
#define BPF_FUNC_get_cgroup_classid       17
#define BPF_FUNC_skb_vlan_push            18
#define BPF_FUNC_skb_vlan_pop             19
#define BPF_FUNC_skb_get_tunnel_key       20
#define BPF_FUNC_skb_set_tunnel_key       21
#define BPF_FUNC_perf_event_read          22
#define BPF_FUNC_redirect                 23
#define BPF_FUNC_get_route_realm          24
#define BPF_FUNC_perf_event_output        25
#define BPF_FUNC_skb_load_bytes           26
#define BPF_FUNC_get_stackid              27
#define BPF_FUNC_csum_diff                28
#define BPF_FUNC_skb_get_tunnel_opt       29
#define BPF_FUNC_skb_set_tunnel_opt       30

#define XDP_ABORTED     0
#define XDP_DROP        1
#define XDP_PASS        2
#define XDP_TX          3
#define XDP_REDIRECT    4

typedef struct {
    u8 code;
    u8 dst_reg:4;
    u8 src_reg:4;
    s16 off;
    s32 imm;
} bpf_insn_t;

typedef struct bpf_map bpf_map_t;

typedef void *(*bpf_map_alloc_fn)(u32 key_size, u32 value_size, u32 max_entries);
typedef void (*bpf_map_free_fn)(void *data);
typedef void *(*bpf_map_lookup_fn)(void *data, const void *key);
typedef int (*bpf_map_update_fn)(void *data, const void *key, const void *value, u64 flags);
typedef int (*bpf_map_delete_fn)(void *data, const void *key);
typedef int (*bpf_map_get_next_key_fn)(void *data, const void *key, void *next_key);

typedef struct bpf_map_ops {
    bpf_map_alloc_fn map_alloc;
    bpf_map_free_fn map_free;
    bpf_map_lookup_fn map_lookup;
    bpf_map_update_fn map_update;
    bpf_map_delete_fn map_delete;
    bpf_map_get_next_key_fn map_get_next_key;
} bpf_map_ops_t;

struct bpf_map {
    u32 map_type;
    u32 key_size;
    u32 value_size;
    u32 max_entries;
    u32 map_flags;
    u32 id;
    spinlock_t lock;
    bpf_map_ops_t *ops;
    void *data;
};

typedef struct {
    bpf_insn_t *insns;
    u32 num_insns;
    int loaded;
    int type;
    u8 *jited_image;
    u32 jited_len;
    spinlock_t lock;
} bpf_program_t;

typedef struct {
    u64 regs[BPF_MAX_REGS];
    u8 stack[BPF_STACK_SIZE];
    u64 stack_size;
} bpf_ctx_t;

void bpf_init(void);

bpf_program_t *bpf_prog_alloc(bpf_insn_t *insns, u32 num_insns, int type);
void bpf_prog_free(bpf_program_t *prog);
u64 bpf_prog_run(bpf_program_t *prog, void *ctx);
int bpf_verifier(bpf_program_t *prog);
int bpf_jit_compile(bpf_program_t *prog);
void bpf_jit_free(bpf_program_t *prog);

bpf_map_t *bpf_map_create(u32 map_type, u32 key_size, u32 value_size, u32 max_entries);
void bpf_map_destroy(bpf_map_t *map);
void *bpf_map_lookup_elem(bpf_map_t *map, const void *key);
int bpf_map_update_elem(bpf_map_t *map, const void *key, const void *value, u64 flags);
int bpf_map_delete_elem(bpf_map_t *map, const void *key);
int bpf_map_get_next_key(bpf_map_t *map, const void *key, void *next_key);

u64 bpf_helper_func_1(int id, u64 arg1);
u64 bpf_helper_func_2(int id, u64 arg1, u64 arg2);
u64 bpf_helper_func_3(int id, u64 arg1, u64 arg2, u64 arg3);
u64 bpf_helper_func_4(int id, u64 arg1, u64 arg2, u64 arg3, u64 arg4);
u64 bpf_helper_func_5(int id, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

extern bpf_map_ops_t bpf_hash_map_ops;
extern bpf_map_ops_t bpf_array_map_ops;
extern bpf_map_ops_t bpf_lru_hash_map_ops;

#endif
