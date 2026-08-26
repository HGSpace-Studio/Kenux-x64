#ifndef BPF_H
#define BPF_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define BPF_MAXINSNS            4096
#define BPF_STACK_SIZE          512
#define BPF_MAX_MAPS            64
#define BPF_REG_COUNT           11
#define BPF_HELPER_MAX          64
#define BPF_PROG_MAX            256
#define BPF_JIT_SIZE            65536

#define BPF_CLASS(code)         ((code) & 0x07)
#define BPF_ALU                 0x04
#define BPF_JMP                 0x05
#define BPF_RET                 0x06
#define BPF_LD                  0x00
#define BPF_LDX                 0x01
#define BPF_ST                  0x02
#define BPF_STX                 0x03
#define BPF_ALU64               0x07

#define BPF_SIZE(code)          ((code) & 0x18)
#define BPF_W                   0x00
#define BPF_H                   0x08
#define BPF_B                   0x10
#define BPF_DW                  0x18

#define BPF_MODE(code)          ((code) & 0xE0)
#define BPF_IMM                 0x00
#define BPF_ABS                 0x20
#define BPF_IND                 0x40
#define BPF_MEM                 0x60
#define BPF_ATOMIC              0xC0

#define BPF_OP(code)            ((code) & 0xF0)
#define BPF_ADD                 0x00
#define BPF_SUB                 0x10
#define BPF_MUL                 0x20
#define BPF_DIV                 0x30
#define BPF_OR                  0x40
#define BPF_AND                 0x50
#define BPF_LSH                 0x60
#define BPF_RSH                 0x70
#define BPF_NEG                 0x80
#define BPF_MOD                 0x90
#define BPF_XOR                 0xA0
#define BPF_MOV                 0xB0
#define BPF_ARSH                0xC0
#define BPF_END                 0xD0

#define BPF_JA                  0x00
#define BPF_JEQ                 0x10
#define BPF_JGT                 0x20
#define BPF_JGE                 0x30
#define BPF_JSET                0x40
#define BPF_JNE                 0x50
#define BPF_JSGT                0x60
#define BPF_JSGE                0x70
#define BPF_CALL                0x80
#define BPF_EXIT                0x90
#define BPF_JLT                 0xA0
#define BPF_JLE                 0xB0
#define BPF_JSLT                0xC0
#define BPF_JSLE                0xD0

#define BPF_SRC(code)           ((code) & 0x08)
#define BPF_K                   0x00
#define BPF_X                   0x08

#define BPF_PSEUDO_MAP_FD       1
#define BPF_PSEUDO_CALL         1

#define BPF_MAP_TYPE_HASH       1
#define BPF_MAP_TYPE_ARRAY      2
#define BPF_MAP_TYPE_PROG_ARRAY 3

#define BPF_ANY                 0
#define BPF_NOEXIST             1
#define BPF_EXIST               2
#define BPF_F_LOCK              4

struct bpf_insn {
    uint8_t  code;
    uint8_t  dst_reg:4;
    uint8_t  src_reg:4;
    int16_t  off;
    int32_t  imm;
};

struct bpf_prog {
    uint32_t len;
    struct bpf_insn* insns;
    uint8_t* image;
    uint32_t jited_len;
    int type;
    int aux_id;
    spinlock_t lock;
};

struct bpf_map_ops {
    void* (*map_lookup_elem)(struct bpf_map* map, const void* key);
    int   (*map_update_elem)(struct bpf_map* map, const void* key, const void* value, uint64_t flags);
    int   (*map_delete_elem)(struct bpf_map* map, const void* key);
    void* (*map_lookup_elem_percpu)(struct bpf_map* map, const void* key);
};

struct bpf_map {
    uint32_t map_type;
    uint32_t key_size;
    uint32_t value_size;
    uint32_t max_entries;
    uint32_t map_flags;
    uint32_t id;
    spinlock_t lock;
    struct bpf_map_ops* ops;
    void* data;
};

struct bpf_array {
    struct bpf_map map;
    uint32_t elem_size;
    void* value;
};

struct bpf_htab_elem {
    struct bpf_htab_elem* next;
    uint32_t hash;
    char key[0];
};

struct bpf_htab {
    struct bpf_map map;
    struct bpf_htab_elem** buckets;
    uint32_t n_buckets;
};

union bpf_attr {
    struct {
        uint32_t map_type;
        uint32_t key_size;
        uint32_t value_size;
        uint32_t max_entries;
        uint32_t map_flags;
    };
    struct {
        uint32_t map_fd;
        uint64_t key;
        uint64_t value;
        uint64_t flags;
    };
    struct {
        uint32_t prog_type;
        uint32_t insn_cnt;
        uint64_t insns;
        uint64_t license;
        uint32_t log_level;
        uint32_t log_size;
        uint64_t log_buf;
        uint32_t kern_version;
    };
};

struct bpf_verifier_env {
    struct bpf_prog* prog;
    uint32_t insn_idx;
    int allow_ptr_leaks;
    int explored;
    int stack_depth;
};

void bpf_init(void);

struct bpf_map* bpf_map_create(const union bpf_attr* attr);
void bpf_map_destroy(struct bpf_map* map);
void* bpf_map_lookup_elem(struct bpf_map* map, const void* key);
int bpf_map_update_elem(struct bpf_map* map, const void* key, const void* value, uint64_t flags);
int bpf_map_delete_elem(struct bpf_map* map, const void* key);

struct bpf_prog* bpf_prog_load(const struct bpf_insn* insns, uint32_t insn_cnt, int type);
void bpf_prog_destroy(struct bpf_prog* prog);
uint64_t bpf_prog_run(struct bpf_prog* prog, void* ctx);

int bpf_jit_compile(struct bpf_prog* prog);
void bpf_jit_free(struct bpf_prog* prog);

int bpf_verifier_validate(struct bpf_prog* prog);
int bpf_verify_insn(struct bpf_verifier_env* env, struct bpf_insn* insn, uint32_t idx);

static inline uint32_t bpf_jit_hash(const void* key, uint32_t key_size)
{
    const uint8_t* p = key;
    uint32_t h = 0x811c9dc5;
    for (uint32_t i = 0; i < key_size; i++) {
        h ^= p[i];
        h *= 0x01000193;
    }
    return h;
}

#endif
