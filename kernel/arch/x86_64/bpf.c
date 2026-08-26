#include <arch/bpf.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/hpet.h>
#include <string.h>
#include <slab.h>

static u32 bpf_map_next_id = 1;
static spinlock_t bpf_map_lock = SPINLOCK_INIT;

/* eBPF helper 函数表 —— 由 kapi_ebpf_register_helper 注册 */
typedef u64 (*bpf_helper_native_t)(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5);
static bpf_helper_native_t g_bpf_helpers[BPF_HELPER_MAX];
static spinlock_t g_bpf_helper_lock = SPINLOCK_INIT;

int bpf_register_helper(int id, bpf_helper_native_t fn)
{
    if (id < 0 || id >= BPF_HELPER_MAX) return -1;
    spin_lock(&g_bpf_helper_lock);
    g_bpf_helpers[id] = fn;
    spin_unlock(&g_bpf_helper_lock);
    return 0;
}

static bpf_helper_native_t bpf_lookup_helper(int id)
{
    if (id < 0 || id >= BPF_HELPER_MAX) return NULL;
    return g_bpf_helpers[id];
}

/* 内置 helper 实现：map_lookup_elem (r1=map*, r2=key*) -> value* */
static u64 bpf_h_map_lookup(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r3; (void)r4; (void)r5;
    bpf_map_t *map = (bpf_map_t *)(uintptr_t)r1;
    const void *key = (const void *)(uintptr_t)r2;
    return (u64)(uintptr_t)bpf_map_lookup_elem(map, key);
}

static u64 bpf_h_map_update(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r5;
    bpf_map_t *map = (bpf_map_t *)(uintptr_t)r1;
    const void *key = (const void *)(uintptr_t)r2;
    const void *value = (const void *)(uintptr_t)r3;
    return (u64)(uintptr_t)bpf_map_update_elem(map, key, value, r4);
}

static u64 bpf_h_map_delete(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r3; (void)r4; (void)r5;
    bpf_map_t *map = (bpf_map_t *)(uintptr_t)r1;
    const void *key = (const void *)(uintptr_t)r2;
    return (u64)(uintptr_t)bpf_map_delete_elem(map, key);
}

static u64 bpf_h_ktime_get_ns(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r1; (void)r2; (void)r3; (void)r4; (void)r5;
    /* 用 HPET ticks * (1e9 / freq) 近似 ns；无 HPET 时退化为 TSC */
    u64 freq = hpet_get_frequency();
    if (freq) {
        u64 ticks = hpet_get_ticks();
        return (ticks * 1000000000ULL) / freq;
    }
    return __builtin_ia32_rdtsc();
}

static u64 bpf_h_get_prandom_u32(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r1; (void)r2; (void)r3; (void)r4; (void)r5;
    /* 线性同余 PRNG，种子取 TSC */
    static u64 state = 0;
    if (!state) state = __builtin_ia32_rdtsc() ^ 0x9E3779B97F4A7C15ULL;
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (u32)(state >> 32);
}

static u64 bpf_h_get_smp_processor_id(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r1; (void)r2; (void)r3; (void)r4; (void)r5;
    return 0;
}

static u64 bpf_h_get_current_pid_tgid(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r1; (void)r2; (void)r3; (void)r4; (void)r5;
    /* 单进程内核：pid=1, tgid=1 */
    return (1ULL << 32) | 1ULL;
}

static u64 bpf_h_probe_read(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r4; (void)r5;
    /* r1=dst, r2=size, r3=src —— 安全内存读取 */
    void *dst = (void *)(uintptr_t)r1;
    u32 size = (u32)r2;
    const void *src = (const void *)(uintptr_t)r3;
    if (!dst || !src || size > 4096) return (u64)(-1);
    /* 简化: 假定内核态地址可读，直接 memcpy */
    memcpy(dst, src, size);
    return 0;
}

static u64 bpf_h_trace_printk(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
    (void)r2; (void)r3; (void)r4; (void)r5;
    /* r1=fmt 字符串，输出到 COM1 串口 */
    const char *fmt = (const char *)(uintptr_t)r1;
    if (!fmt) return 0;
    u64 n = 0;
    while (n < 128 && fmt[n]) {
        char c = fmt[n++];
        __asm__ volatile ("outb %0, %1" : : "a"(c), "d"((unsigned short)0x3F8));
    }
    return n;
}

static void bpf_register_builtin_helpers(void)
{
    bpf_register_helper(BPF_FUNC_map_lookup_elem,      bpf_h_map_lookup);
    bpf_register_helper(BPF_FUNC_map_update_elem,      bpf_h_map_update);
    bpf_register_helper(BPF_FUNC_map_delete_elem,      bpf_h_map_delete);
    bpf_register_helper(BPF_FUNC_ktime_get_ns,         bpf_h_ktime_get_ns);
    bpf_register_helper(BPF_FUNC_get_prandom_u32,      bpf_h_get_prandom_u32);
    bpf_register_helper(BPF_FUNC_get_smp_processor_id, bpf_h_get_smp_processor_id);
    bpf_register_helper(BPF_FUNC_get_current_pid_tgid, bpf_h_get_current_pid_tgid);
    bpf_register_helper(BPF_FUNC_probe_read,           bpf_h_probe_read);
    bpf_register_helper(BPF_FUNC_trace_printk,         bpf_h_trace_printk);
}

typedef struct bpf_hash_entry {
    struct bpf_hash_entry *next;
    u32 hash;
    u8 data[0];
} bpf_hash_entry_t;

typedef struct {
    bpf_hash_entry_t **buckets;
    u32 n_buckets;
    u32 key_size;
    u32 value_size;
    u32 elem_count;
    u32 max_entries;
} bpf_hash_data_t;

/* hash entry 布局: [header][key][value] */
#define HASH_ENTRY_KEY(p)    ((u8*)((p) + 1))
#define HASH_ENTRY_VALUE(p, ksz)  ((u8*)((p) + 1) + (ksz))

static void *hash_map_alloc(u32 key_size, u32 value_size, u32 max_entries)
{
    u32 n_buckets = max_entries;
    if (n_buckets == 0) n_buckets = 1;
    while (n_buckets & (n_buckets - 1)) n_buckets++;
    if (n_buckets < 4) n_buckets = 4;

    bpf_hash_data_t *hdata = kzalloc(sizeof(bpf_hash_data_t));
    if (!hdata) return NULL;

    hdata->n_buckets = n_buckets;
    hdata->key_size = key_size;
    hdata->value_size = value_size;
    hdata->max_entries = max_entries;
    hdata->elem_count = 0;
    hdata->buckets = kzalloc(sizeof(bpf_hash_entry_t *) * n_buckets);
    if (!hdata->buckets) {
        kfree(hdata);
        return NULL;
    }
    return hdata;
}

static void hash_map_free(void *data)
{
    if (!data) return;
    bpf_hash_data_t *hdata = (bpf_hash_data_t *)data;
    for (u32 i = 0; i < hdata->n_buckets; i++) {
        bpf_hash_entry_t *entry = hdata->buckets[i];
        while (entry) {
            bpf_hash_entry_t *next = entry->next;
            kfree(entry);
            entry = next;
        }
    }
    kfree(hdata->buckets);
    kfree(hdata);
}

static u32 bpf_hash_fn(const void *key, u32 key_size)
{
    const u8 *p = (const u8 *)key;
    u32 h = 0x811c9dc5;
    for (u32 i = 0; i < key_size; i++) {
        h ^= p[i];
        h *= 0x01000193;
    }
    return h;
}

static int key_equal(const void *a, const void *b, u32 sz)
{
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    for (u32 i = 0; i < sz; i++) {
        if (pa[i] != pb[i]) return 0;
    }
    return 1;
}

static void *hash_map_lookup_impl(void *data, const void *key)
{
    bpf_hash_data_t *hdata = (bpf_hash_data_t *)data;
    if (!hdata || !key || !hdata->buckets) return NULL;
    u32 h = bpf_hash_fn(key, hdata->key_size);
    u32 idx = h & (hdata->n_buckets - 1);
    bpf_hash_entry_t *entry = hdata->buckets[idx];
    while (entry) {
        if (entry->hash == h &&
            key_equal(HASH_ENTRY_KEY(entry), key, hdata->key_size)) {
            return HASH_ENTRY_VALUE(entry, hdata->key_size);
        }
        entry = entry->next;
    }
    return NULL;
}

static int hash_map_update_impl(void *data, const void *key, const void *value, u64 flags)
{
    bpf_hash_data_t *hdata = (bpf_hash_data_t *)data;
    if (!hdata || !key || !value) return -1;
    (void)flags;

    u32 h = bpf_hash_fn(key, hdata->key_size);
    u32 idx = h & (hdata->n_buckets - 1);

    /* 已存在则更新 */
    bpf_hash_entry_t *entry = hdata->buckets[idx];
    while (entry) {
        if (entry->hash == h &&
            key_equal(HASH_ENTRY_KEY(entry), key, hdata->key_size)) {
            memcpy(HASH_ENTRY_VALUE(entry, hdata->key_size), value, hdata->value_size);
            return 0;
        }
        entry = entry->next;
    }

    /* 容量检查 */
    if (hdata->max_entries && hdata->elem_count >= hdata->max_entries) {
        return -1;
    }

    /* 新建条目: [header][key][value] */
    u32 entry_sz = sizeof(bpf_hash_entry_t) + hdata->key_size + hdata->value_size;
    bpf_hash_entry_t *new_entry = kzalloc(entry_sz);
    if (!new_entry) return -1;

    new_entry->hash = h;
    new_entry->next = hdata->buckets[idx];
    memcpy(HASH_ENTRY_KEY(new_entry), key, hdata->key_size);
    memcpy(HASH_ENTRY_VALUE(new_entry, hdata->key_size), value, hdata->value_size);
    hdata->buckets[idx] = new_entry;
    hdata->elem_count++;
    return 0;
}

static int hash_map_delete(void *data, const void *key)
{
    bpf_hash_data_t *hdata = (bpf_hash_data_t *)data;
    if (!hdata || !key) return -1;
    u32 h = bpf_hash_fn(key, hdata->key_size);
    u32 idx = h & (hdata->n_buckets - 1);

    bpf_hash_entry_t **pp = &hdata->buckets[idx];
    while (*pp) {
        bpf_hash_entry_t *entry = *pp;
        if (entry->hash == h &&
            key_equal(HASH_ENTRY_KEY(entry), key, hdata->key_size)) {
            *pp = entry->next;
            kfree(entry);
            hdata->elem_count--;
            return 0;
        }
        pp = &entry->next;
    }
    return -1;
}

static int hash_map_get_next_key(void *data, const void *key, void *next_key)
{
    bpf_hash_data_t *hdata = (bpf_hash_data_t *)data;
    if (!hdata || !next_key) return -1;

    if (!key) {
        /* 返回第一个 key */
        for (u32 i = 0; i < hdata->n_buckets; i++) {
            if (hdata->buckets[i]) {
                memcpy(next_key, HASH_ENTRY_KEY(hdata->buckets[i]), hdata->key_size);
                return 0;
            }
        }
        return -1;
    }

    /* 查找当前 key 所在位置，返回下一个 */
    u32 h = bpf_hash_fn(key, hdata->key_size);
    u32 idx = h & (hdata->n_buckets - 1);
    bpf_hash_entry_t *entry = hdata->buckets[idx];
    int found = 0;
    while (entry) {
        if (found) {
            memcpy(next_key, HASH_ENTRY_KEY(entry), hdata->key_size);
            return 0;
        }
        if (entry->hash == h &&
            key_equal(HASH_ENTRY_KEY(entry), key, hdata->key_size)) {
            found = 1;
        }
        entry = entry->next;
    }

    /* 当前桶后续无元素，扫描下一个非空桶 */
    for (u32 i = idx + 1; i < hdata->n_buckets; i++) {
        if (hdata->buckets[i]) {
            memcpy(next_key, HASH_ENTRY_KEY(hdata->buckets[i]), hdata->key_size);
            return 0;
        }
    }
    return -1;
}

bpf_map_ops_t bpf_hash_map_ops = {
    .map_alloc = hash_map_alloc,
    .map_free = hash_map_free,
    .map_lookup = hash_map_lookup_impl,
    .map_update = hash_map_update_impl,
    .map_delete = hash_map_delete,
    .map_get_next_key = hash_map_get_next_key
};

typedef struct {
    u8 *data;
    u32 elem_size;
} bpf_array_data_t;

static void *array_map_alloc(u32 key_size, u32 value_size, u32 max_entries)
{
    bpf_array_data_t *adata = kzalloc(sizeof(bpf_array_data_t));
    if (!adata) return NULL;
    adata->elem_size = value_size;
    adata->data = kzalloc((u64)value_size * max_entries);
    if (!adata->data) {
        kfree(adata);
        return NULL;
    }
    return adata;
}

static void array_map_free(void *data)
{
    if (!data) return;
    bpf_array_data_t *adata = (bpf_array_data_t *)data;
    if (adata->data) kfree(adata->data);
    kfree(adata);
}

static void *array_map_lookup(void *data, const void *key)
{
    bpf_array_data_t *adata = (bpf_array_data_t *)data;
    if (!adata || !key) return NULL;
    u32 idx = *(const u32 *)key;
    return &adata->data[(u64)idx * adata->elem_size];
}

static int array_map_update(void *data, const void *key, const void *value, u64 flags)
{
    (void)flags;
    bpf_array_data_t *adata = (bpf_array_data_t *)data;
    if (!adata || !key || !value) return -1;
    u32 idx = *(const u32 *)key;
    memcpy(&adata->data[(u64)idx * adata->elem_size], value, adata->elem_size);
    return 0;
}

static int array_map_delete(void *data, const void *key)
{
    bpf_array_data_t *adata = (bpf_array_data_t *)data;
    if (!adata || !key) return -1;
    u32 idx = *(const u32 *)key;
    memset(&adata->data[(u64)idx * adata->elem_size], 0, adata->elem_size);
    return 0;
}

static int array_map_get_next_key(void *data, const void *key, void *next_key)
{
    bpf_array_data_t *adata = (bpf_array_data_t *)data;
    if (!adata || !next_key) return -1;
    if (!key) {
        *(u32 *)next_key = 0;
        return 0;
    }
    u32 idx = *(const u32 *)key;
    idx++;
    *(u32 *)next_key = idx;
    return 0;
}

bpf_map_ops_t bpf_array_map_ops = {
    .map_alloc = array_map_alloc,
    .map_free = array_map_free,
    .map_lookup = array_map_lookup,
    .map_update = array_map_update,
    .map_delete = array_map_delete,
    .map_get_next_key = array_map_get_next_key
};

bpf_map_ops_t bpf_lru_hash_map_ops = {
    .map_alloc = hash_map_alloc,
    .map_free = hash_map_free,
    .map_lookup = hash_map_lookup_impl,
    .map_update = hash_map_update_impl,
    .map_delete = hash_map_delete,
    .map_get_next_key = hash_map_get_next_key
};

static bpf_map_ops_t *get_map_ops(u32 map_type)
{
    switch (map_type) {
    case BPF_MAP_TYPE_HASH:
    case BPF_MAP_TYPE_PERCPU_HASH:
        return &bpf_hash_map_ops;
    case BPF_MAP_TYPE_ARRAY:
    case BPF_MAP_TYPE_PERCPU_ARRAY:
    case BPF_MAP_TYPE_PROG_ARRAY:
    case BPF_MAP_TYPE_CGROUP_ARRAY:
        return &bpf_array_map_ops;
    case BPF_MAP_TYPE_LRU_HASH:
    case BPF_MAP_TYPE_LRU_PERCPU_HASH:
        return &bpf_lru_hash_map_ops;
    default:
        return NULL;
    }
}

bpf_map_t *bpf_map_create(u32 map_type, u32 key_size, u32 value_size, u32 max_entries)
{
    bpf_map_ops_t *ops = get_map_ops(map_type);
    if (!ops || !ops->map_alloc) return NULL;

    bpf_map_t *map = kzalloc(sizeof(bpf_map_t));
    if (!map) return NULL;

    map->data = ops->map_alloc(key_size, value_size, max_entries);
    if (!map->data) {
        kfree(map);
        return NULL;
    }

    map->map_type = map_type;
    map->key_size = key_size;
    map->value_size = value_size;
    map->max_entries = max_entries;
    map->map_flags = 0;
    spin_init(&map->lock);

    spin_lock(&bpf_map_lock);
    map->id = bpf_map_next_id++;
    spin_unlock(&bpf_map_lock);

    map->ops = ops;
    return map;
}

void bpf_map_destroy(bpf_map_t *map)
{
    if (!map) return;
    if (map->ops && map->ops->map_free) {
        map->ops->map_free(map->data);
    }
    kfree(map);
}

void *bpf_map_lookup_elem(bpf_map_t *map, const void *key)
{
    if (!map || !key) return NULL;
    if (!map->ops || !map->ops->map_lookup) return NULL;
    return map->ops->map_lookup(map->data, key);
}

int bpf_map_update_elem(bpf_map_t *map, const void *key, const void *value, u64 flags)
{
    if (!map || !key || !value) return -1;
    if (!map->ops || !map->ops->map_update) return -1;
    return map->ops->map_update(map->data, key, value, flags);
}

int bpf_map_delete_elem(bpf_map_t *map, const void *key)
{
    if (!map || !key) return -1;
    if (!map->ops || !map->ops->map_delete) return -1;
    return map->ops->map_delete(map->data, key);
}

int bpf_map_get_next_key(bpf_map_t *map, const void *key, void *next_key)
{
    if (!map || !next_key) return -1;
    if (!map->ops || !map->ops->map_get_next_key) return -1;
    return map->ops->map_get_next_key(map->data, key, next_key);
}

static u64 read_mem(bpf_ctx_t *ctx, int size, u64 addr)
{
    (void)ctx;
    u8 *ptr = (u8 *)(uintptr_t)addr;
    /* NULL 或显然无效地址返回 0 */
    if (!ptr) return 0;
    switch (size) {
    case 1: return (u64)*(volatile u8 *)ptr;
    case 2: return (u64)*(volatile u16 *)ptr;
    case 4: return (u64)*(volatile u32 *)ptr;
    case 8: return (u64)*(volatile u64 *)ptr;
    default: return 0;
    }
}

static void write_mem(bpf_ctx_t *ctx, int size, u64 addr, u64 val)
{
    (void)ctx;
    u8 *ptr = (u8 *)(uintptr_t)addr;
    if (!ptr) return;
    switch (size) {
    case 1: *(volatile u8 *)ptr = (u8)val; break;
    case 2: *(volatile u16 *)ptr = (u16)val; break;
    case 4: *(volatile u32 *)ptr = (u32)val; break;
    case 8: *(volatile u64 *)ptr = (u64)val; break;
    default: break;
    }
}

static inline u64 get_reg(bpf_ctx_t *ctx, int reg)
{
    if (reg >= BPF_MAX_REGS) return 0;
    return ctx->regs[reg];
}

static inline void set_reg(bpf_ctx_t *ctx, int reg, u64 val)
{
    if (reg >= BPF_MAX_REGS) return;
    if (reg == BPF_REG_10) return;
    ctx->regs[reg] = val;
}

u64 bpf_prog_run(bpf_program_t *prog, void *ctx_in)
{
    if (!prog || !prog->insns || !prog->loaded) return 0;

    bpf_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (ctx_in) {
        memcpy(ctx.regs + 1, ctx_in, 5 * sizeof(u64));
    }
    /* r10 指向栈顶（栈向下生长，r10 + 负偏移 = 栈内地址） */
    ctx.regs[BPF_REG_10] = (u64)(uintptr_t)&ctx.stack[BPF_STACK_SIZE];

    u32 pc = 0;
    while (pc < prog->num_insns) {
        bpf_insn_t *insn = &prog->insns[pc];
        u8 code = insn->code;
        u8 cls = BPF_CLASS(code);
        u8 op = BPF_OP(code);
        u8 src = BPF_SRC(code);
        u8 size = BPF_SIZE(code);
        int dst = insn->dst_reg;
        int sr = insn->src_reg;

        switch (cls) {
        case BPF_ALU64:
        case BPF_ALU: {
            u64 dv = get_reg(&ctx, dst);
            u64 sv = get_reg(&ctx, sr);
            s64 sdv = (s64)dv;
            s64 ssv = (s64)sv;

            switch (op) {
            case BPF_ADD: dv = dv + sv; break;
            case BPF_SUB: dv = dv - sv; break;
            case BPF_MUL: dv = dv * sv; break;
            case BPF_DIV:
                if (sv == 0) goto exit_abort;
                dv = dv / sv;
                break;
            case BPF_MOD:
                if (sv == 0) goto exit_abort;
                dv = dv % sv;
                break;
            case BPF_AND: dv = dv & sv; break;
            case BPF_OR:  dv = dv | sv; break;
            case BPF_XOR: dv = dv ^ sv; break;
            case BPF_LSH: dv = dv << (sv & 63); break;
            case BPF_RSH: dv = dv >> (sv & 63); break;
            case BPF_ARSH: dv = (u64)(sdv >> (sv & 63)); break;
            case BPF_NEG: dv = (u64)(-sdv); break;
            case BPF_MOV:
                if (src == BPF_K) dv = (u64)(s64)(s32)insn->imm;
                else dv = sv;
                break;
            case BPF_END: {
                if (src == BPF_K) {
                    switch (insn->imm) {
                    case 16: dv = (u16)dv; break;
                    case 32: dv = (u32)dv; break;
                    case 64: break;
                    }
                } else {
                    switch (insn->imm) {
                    case 16: dv = __builtin_bswap16((u16)dv); break;
                    case 32: dv = __builtin_bswap32((u32)dv); break;
                    case 64: dv = __builtin_bswap64(dv); break;
                    }
                }
                break;
            }
            default: goto exit_abort;
            }

            if (cls == BPF_ALU && op != BPF_END && op != BPF_NEG) {
                dv = (u32)(dv & 0xFFFFFFFF);
            }
            set_reg(&ctx, dst, dv);
            break;
        }
        case BPF_JMP:
        case BPF_JMP32: {
            u64 dv = get_reg(&ctx, dst);
            u64 sv = get_reg(&ctx, sr);
            s64 sdv = (s64)dv;
            s64 ssv = (s64)sv;
            u32 dv32 = (u32)dv;
            u32 sv32 = (u32)sv;
            s32 sdv32 = (s32)dv;
            s32 ssv32 = (s32)sv;
            int cond = 0;

            switch (op) {
            case BPF_JA: pc += insn->off + 1; continue;
            case BPF_JEQ:  cond = (cls == BPF_JMP32) ? (dv32 == sv32) : (dv == sv); break;
            case BPF_JNE:  cond = (cls == BPF_JMP32) ? (dv32 != sv32) : (dv != sv); break;
            case BPF_JGT:  cond = (cls == BPF_JMP32) ? (dv32 > sv32) : (dv > sv); break;
            case BPF_JGE:  cond = (cls == BPF_JMP32) ? (dv32 >= sv32) : (dv >= sv); break;
            case BPF_JLT:  cond = (cls == BPF_JMP32) ? (dv32 < sv32) : (dv < sv); break;
            case BPF_JLE:  cond = (cls == BPF_JMP32) ? (dv32 <= sv32) : (dv <= sv); break;
            case BPF_JSGT: cond = (cls == BPF_JMP32) ? (sdv32 > ssv32) : (sdv > ssv); break;
            case BPF_JSGE: cond = (cls == BPF_JMP32) ? (sdv32 >= ssv32) : (sdv >= ssv); break;
            case BPF_JSLT: cond = (cls == BPF_JMP32) ? (sdv32 < ssv32) : (sdv < ssv); break;
            case BPF_JSLE: cond = (cls == BPF_JMP32) ? (sdv32 <= ssv32) : (sdv <= ssv); break;
            case BPF_JSET: cond = (cls == BPF_JMP32) ? ((dv32 & sv32) != 0) : ((dv & sv) != 0); break;
            case BPF_CALL: {
                int helper_id = insn->imm;
                u64 r1 = get_reg(&ctx, BPF_REG_1);
                u64 r2 = get_reg(&ctx, BPF_REG_2);
                u64 r3 = get_reg(&ctx, BPF_REG_3);
                u64 r4 = get_reg(&ctx, BPF_REG_4);
                u64 r5 = get_reg(&ctx, BPF_REG_5);
                u64 ret = 0;
                bpf_helper_native_t fn = bpf_lookup_helper(helper_id);
                if (fn) {
                    ret = fn(r1, r2, r3, r4, r5);
                } else {
                    /* 回退到原始内置逻辑 */
                    switch (helper_id) {
                    case BPF_FUNC_get_prandom_u32:
                        ret = (u64)(__builtin_ia32_rdtsc() & 0xFFFFFFFF);
                        break;
                    case BPF_FUNC_get_current_pid_tgid:
                        ret = 1;
                        break;
                    case BPF_FUNC_get_smp_processor_id:
                        ret = 0;
                        break;
                    default:
                        ret = 0;
                        break;
                    }
                }
                set_reg(&ctx, BPF_REG_0, ret);
                pc++;
                continue;
            }
            case BPF_EXIT:
                goto exit_ok;
            default: goto exit_abort;
            }

            if (op != BPF_JA && op != BPF_CALL && op != BPF_EXIT) {
                if (src == BPF_K) {
                    u64 imm = (u64)(s64)(s32)insn->imm;
                    s64 simm = (s64)(s32)insn->imm;
                    u32 imm32 = (u32)insn->imm;
                    s32 simm32 = (s32)insn->imm;
                    switch (op) {
                    case BPF_JEQ:  cond = (cls == BPF_JMP32) ? (dv32 == imm32) : (dv == imm); break;
                    case BPF_JNE:  cond = (cls == BPF_JMP32) ? (dv32 != imm32) : (dv != imm); break;
                    case BPF_JGT:  cond = (cls == BPF_JMP32) ? (dv32 > imm32) : (dv > imm); break;
                    case BPF_JGE:  cond = (cls == BPF_JMP32) ? (dv32 >= imm32) : (dv >= imm); break;
                    case BPF_JLT:  cond = (cls == BPF_JMP32) ? (dv32 < imm32) : (dv < imm); break;
                    case BPF_JLE:  cond = (cls == BPF_JMP32) ? (dv32 <= imm32) : (dv <= imm); break;
                    case BPF_JSGT: cond = (cls == BPF_JMP32) ? (sdv32 > simm32) : (sdv > simm); break;
                    case BPF_JSGE: cond = (cls == BPF_JMP32) ? (sdv32 >= simm32) : (sdv >= simm); break;
                    case BPF_JSLT: cond = (cls == BPF_JMP32) ? (sdv32 < simm32) : (sdv < simm); break;
                    case BPF_JSLE: cond = (cls == BPF_JMP32) ? (sdv32 <= simm32) : (sdv <= simm); break;
                    case BPF_JSET: cond = (cls == BPF_JMP32) ? ((dv32 & imm32) != 0) : ((dv & imm) != 0); break;
                    }
                }
            }

            if (cond) {
                pc += insn->off + 1;
            } else {
                pc++;
            }
            break;
        }
        case BPF_LDX: {
            u64 dv = get_reg(&ctx, dst);
            u64 sv = get_reg(&ctx, sr);
            u64 addr = sv + (s64)insn->off;
            int sz = 4;
            switch (size) {
            case BPF_B: sz = 1; break;
            case BPF_H: sz = 2; break;
            case BPF_W: sz = 4; break;
            case BPF_DW: sz = 8; break;
            }
            u64 val = read_mem(&ctx, sz, addr);
            if (cls == BPF_ALU) val = (u32)val;
            set_reg(&ctx, dst, val);
            break;
        }
        case BPF_ST:
        case BPF_STX: {
            u64 val;
            u64 addr;
            if (cls == BPF_ST) {
                val = (u64)(s64)(s32)insn->imm;
                addr = get_reg(&ctx, dst) + (s64)insn->off;
            } else {
                val = get_reg(&ctx, sr);
                addr = get_reg(&ctx, dst) + (s64)insn->off;
            }
            int sz = 4;
            switch (size) {
            case BPF_B: sz = 1; break;
            case BPF_H: sz = 2; break;
            case BPF_W: sz = 4; break;
            case BPF_DW: sz = 8; break;
            }
            write_mem(&ctx, sz, addr, val);
            break;
        }
        case BPF_LD: {
            u8 mode = BPF_MODE(code);
            if (mode == BPF_IMM) {
                /* LD_IMM64: 两条指令序列，第一条载低32位，第二条载高32位 */
                u64 imm = (u64)(s64)(s32)insn->imm;
                if (pc + 1 < prog->num_insns) {
                    bpf_insn_t *next = &prog->insns[pc + 1];
                    if (next->code == 0) {
                        u64 hi = (u64)(u32)next->imm;
                        imm = (hi << 32) | (u32)insn->imm;
                        pc++;  /* 跳过第二条 */
                    }
                }
                set_reg(&ctx, dst, imm);
            } else if (mode == BPF_ABS) {
                /* LD_ABS: addr = imm，从 ctx 偏移读取 */
                int sz = (size == BPF_B) ? 1 : (size == BPF_H) ? 2 :
                         (size == BPF_W) ? 4 : 8;
                u64 addr = (u64)(uintptr_t)ctx_in + (u64)(s64)(s32)insn->imm;
                set_reg(&ctx, dst, read_mem(&ctx, sz, addr));
            } else if (mode == BPF_IND) {
                /* LD_IND: addr = src_reg + imm */
                u64 base = get_reg(&ctx, sr);
                int sz = (size == BPF_B) ? 1 : (size == BPF_H) ? 2 :
                         (size == BPF_W) ? 4 : 8;
                u64 addr = (u64)(uintptr_t)ctx_in + base + (u64)(s64)(s32)insn->imm;
                set_reg(&ctx, dst, read_mem(&ctx, sz, addr));
            } else {
                u64 imm = (u64)(s64)(s32)insn->imm;
                set_reg(&ctx, dst, imm);
            }
            break;
        }
        default:
            goto exit_abort;
        }

        pc++;
    }

exit_ok:
    return get_reg(&ctx, BPF_REG_0);

exit_abort:
    set_reg(&ctx, BPF_REG_0, (u64)(-1));
    return get_reg(&ctx, BPF_REG_0);
}

int bpf_verifier(bpf_program_t *prog)
{
    if (!prog || !prog->insns) return -1;
    if (prog->num_insns == 0 || prog->num_insns > MAX_INSNS) return -1;

    int *visited = kzalloc(sizeof(int) * prog->num_insns);
    if (!visited) return -1;

    for (u32 pc = 0; pc < prog->num_insns; pc++) {
        bpf_insn_t *insn = &prog->insns[pc];
        u8 cls = BPF_CLASS(insn->code);
        u8 dst = insn->dst_reg;
        u8 sr = insn->src_reg;

        if (dst >= BPF_MAX_REGS || sr >= BPF_MAX_REGS) {
            kfree(visited);
            return -1;
        }

        if (cls == BPF_JMP && BPF_OP(insn->code) == BPF_CALL) {
            if (insn->imm >= BPF_HELPER_MAX || insn->imm < 0) {
                kfree(visited);
                return -1;
            }
        }

        if (cls == BPF_JMP || cls == BPF_JMP32) {
            u8 op = BPF_OP(insn->code);
            if (op == BPF_JA) {
                s32 target = pc + insn->off + 1;
                if (target < 0 || (u32)target >= prog->num_insns) {
                    kfree(visited);
                    return -1;
                }
            }
        }
    }

    kfree(visited);
    return 0;
}

static u8 jit_buffer[BPF_JIT_SIZE];
static u32 jit_pos = 0;

static void emit_byte(u8 b)
{
    if (jit_pos < BPF_JIT_SIZE) {
        jit_buffer[jit_pos++] = b;
    }
}

static void emit_dword(u32 val)
{
    emit_byte(val & 0xFF);
    emit_byte((val >> 8) & 0xFF);
    emit_byte((val >> 16) & 0xFF);
    emit_byte((val >> 24) & 0xFF);
}

static void emit_qword(u64 val)
{
    emit_dword((u32)(val & 0xFFFFFFFF));
    emit_dword((u32)(val >> 32));
}

static void emit_mov_reg(u8 dst, u8 src)
{
    if (dst == src) return;
    emit_byte(0x48);
    emit_byte(0x89);
    emit_byte(0xC0 | (src << 3) | dst);
}

static void emit_add_reg(u8 dst, u8 src)
{
    emit_byte(0x48);
    emit_byte(0x01);
    emit_byte(0xC0 | (src << 3) | dst);
}

static void emit_ret(void)
{
    emit_byte(0xC3);
}

int bpf_jit_compile(bpf_program_t *prog)
{
    if (!prog || !prog->insns) return -1;
    jit_pos = 0;
    memset(jit_buffer, 0, sizeof(jit_buffer));

    for (u32 pc = 0; pc < prog->num_insns; pc++) {
        bpf_insn_t *insn = &prog->insns[pc];
        u8 cls = BPF_CLASS(insn->code);
        u8 op = BPF_OP(insn->code);
        u8 src = BPF_SRC(insn->code);
        u8 dst = insn->dst_reg;
        u8 sr = insn->src_reg;

        switch (cls) {
        case BPF_ALU64:
            switch (op) {
            case BPF_MOV:
                if (src == BPF_K) {
                    emit_byte(0x48);
                    emit_byte(0xC7);
                    emit_byte(0xC0 | dst);
                    emit_dword((u32)insn->imm);
                } else {
                    emit_mov_reg(dst, sr);
                }
                break;
            case BPF_ADD:
                if (src == BPF_K) {
                    emit_byte(0x48);
                    emit_byte(0x83);
                    emit_byte(0xC0 | dst);
                    emit_byte((u8)insn->imm);
                } else {
                    emit_add_reg(dst, sr);
                }
                break;
            default:
                break;
            }
            break;
        case BPF_JMP:
            if (op == BPF_EXIT) {
                emit_mov_reg(0, 7);
                emit_ret();
            }
            break;
        default:
            break;
        }
    }

    prog->jited_len = jit_pos;
    prog->jited_image = jit_buffer;
    prog->loaded = 1;
    return 0;
}

void bpf_jit_free(bpf_program_t *prog)
{
    if (!prog) return;
    prog->jited_image = NULL;
    prog->jited_len = 0;
}

bpf_program_t *bpf_prog_alloc(bpf_insn_t *insns, u32 num_insns, int type)
{
    if (!insns || num_insns == 0 || num_insns > MAX_INSNS) return NULL;

    bpf_program_t *prog = kzalloc(sizeof(bpf_program_t));
    if (!prog) return NULL;

    prog->insns = kzalloc(sizeof(bpf_insn_t) * num_insns);
    if (!prog->insns) {
        kfree(prog);
        return NULL;
    }

    memcpy(prog->insns, insns, sizeof(bpf_insn_t) * num_insns);
    prog->num_insns = num_insns;
    prog->type = type;
    prog->loaded = 0;
    prog->jited_image = NULL;
    prog->jited_len = 0;
    spin_init(&prog->lock);

    if (bpf_verifier(prog) != 0) {
        kfree(prog->insns);
        kfree(prog);
        return NULL;
    }

    prog->loaded = 1;
    return prog;
}

void bpf_prog_free(bpf_program_t *prog)
{
    if (!prog) return;
    if (prog->insns) kfree(prog->insns);
    bpf_jit_free(prog);
    kfree(prog);
}

void bpf_init(void)
{
    bpf_map_next_id = 1;
    spin_init(&bpf_map_lock);
    spin_init(&g_bpf_helper_lock);
    for (int i = 0; i < BPF_HELPER_MAX; i++) {
        g_bpf_helpers[i] = NULL;
    }
    bpf_register_builtin_helpers();
}

u64 bpf_helper_func_1(int id, u64 arg1)
{
    (void)id;
    (void)arg1;
    return 0;
}

u64 bpf_helper_func_2(int id, u64 arg1, u64 arg2)
{
    (void)id;
    (void)arg1;
    (void)arg2;
    return 0;
}

u64 bpf_helper_func_3(int id, u64 arg1, u64 arg2, u64 arg3)
{
    (void)id;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 0;
}

u64 bpf_helper_func_4(int id, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    (void)id;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    return 0;
}

u64 bpf_helper_func_5(int id, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    (void)id;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    return 0;
}
