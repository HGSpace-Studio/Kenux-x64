#include "kapi_hash.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

struct kapi_hash_table {
    kapi_hlist_head_t* buckets;
    size_t             bits;
    size_t             size;
    kapi_hash_fn       hash_fn;
    kapi_hash_cmp_fn   cmp_fn;
    int                valid;
};

#define KAPI_HASH_MAX_TABLES 64

static kapi_hash_table_t kapi_hash_tables[KAPI_HASH_MAX_TABLES];
static int kapi_hash_initialized = 0;

int kapi_hash_init(void)
{
    if (kapi_hash_initialized) {
        return KAPI_OK;
    }

    memset(kapi_hash_tables, 0, sizeof(kapi_hash_tables));
    kapi_hash_initialized = 1;
    return KAPI_OK;
}

kapi_hash_table_t* kapi_hash_create(size_t bits, kapi_hash_fn hash_fn,
                                     kapi_hash_cmp_fn cmp_fn)
{
    if (bits < KAPI_HASH_BITS_MIN) {
        bits = KAPI_HASH_BITS_MIN;
    }
    if (bits > KAPI_HASH_BITS_MAX) {
        bits = KAPI_HASH_BITS_MAX;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_HASH_MAX_TABLES; i++) {
        if (!kapi_hash_tables[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    kapi_hash_table_t* ht = &kapi_hash_tables[slot];
    memset(ht, 0, sizeof(*ht));

    size_t bucket_count = (size_t)1 << bits;
    ht->buckets = (kapi_hlist_head_t*)memory_alloc(
        bucket_count * sizeof(kapi_hlist_head_t));
    if (!ht->buckets) {
        return NULL;
    }

    for (size_t i = 0; i < bucket_count; i++) {
        kapi_hlist_init(&ht->buckets[i]);
    }

    ht->bits = bits;
    ht->size = 0;
    ht->hash_fn = hash_fn;
    ht->cmp_fn = cmp_fn;
    ht->valid = 1;
    return ht;
}

void kapi_hash_destroy(kapi_hash_table_t* ht)
{
    if (!ht || !ht->valid) {
        return;
    }

    if (ht->buckets) {
        memory_free(ht->buckets);
        ht->buckets = NULL;
    }

    ht->valid = 0;
}

int kapi_hash_add(kapi_hash_table_t* ht, const void* key, size_t key_len,
                  kapi_hlist_node_t* node)
{
    if (!ht || !ht->valid || !node) {
        return KAPI_EINVAL;
    }

    if (!kapi_hlist_unhashed(node)) {
        return KAPI_EBUSY;
    }

    size_t bucket_count = (size_t)1 << ht->bits;
    uint64_t hash = 0;
    if (ht->hash_fn) {
        hash = ht->hash_fn(key, key_len);
    }
    size_t bucket = (size_t)(hash & (bucket_count - 1));

    kapi_hlist_add_head(node, &ht->buckets[bucket]);
    ht->size++;
    return KAPI_OK;
}

int kapi_hash_del(kapi_hash_table_t* ht, kapi_hlist_node_t* node)
{
    if (!ht || !ht->valid || !node) {
        return KAPI_EINVAL;
    }

    if (kapi_hlist_unhashed(node)) {
        return KAPI_ENOENT;
    }

    kapi_hlist_del(node);
    ht->size--;
    return KAPI_OK;
}

kapi_hlist_node_t* kapi_hash_find(kapi_hash_table_t* ht, const void* key,
                                   size_t key_len)
{
    if (!ht || !ht->valid) {
        return NULL;
    }

    size_t bucket_count = (size_t)1 << ht->bits;
    uint64_t hash = 0;
    if (ht->hash_fn) {
        hash = ht->hash_fn(key, key_len);
    }
    size_t bucket = (size_t)(hash & (bucket_count - 1));

    kapi_hlist_node_t* pos;
    kapi_hlist_for_each(pos, &ht->buckets[bucket]) {
        if (ht->cmp_fn) {
            if (ht->cmp_fn(pos, key, key_len) == 0) {
                return pos;
            }
        }
    }
    return NULL;
}

int kapi_hash_resize(kapi_hash_table_t* ht, size_t new_bits)
{
    if (!ht || !ht->valid) {
        return KAPI_EINVAL;
    }

    if (new_bits < KAPI_HASH_BITS_MIN) {
        new_bits = KAPI_HASH_BITS_MIN;
    }
    if (new_bits > KAPI_HASH_BITS_MAX) {
        new_bits = KAPI_HASH_BITS_MAX;
    }

    if (new_bits == ht->bits) {
        return KAPI_OK;
    }

    size_t new_count = (size_t)1 << new_bits;
    kapi_hlist_head_t* new_buckets = (kapi_hlist_head_t*)memory_alloc(
        new_count * sizeof(kapi_hlist_head_t));
    if (!new_buckets) {
        return KAPI_ENOMEM;
    }

    for (size_t i = 0; i < new_count; i++) {
        kapi_hlist_init(&new_buckets[i]);
    }

    size_t old_count = (size_t)1 << ht->bits;
    for (size_t i = 0; i < old_count; i++) {
        kapi_hlist_node_t* pos;
        kapi_hlist_node_t* n;
        kapi_hlist_for_each_safe(pos, n, &ht->buckets[i]) {
            kapi_hlist_del(pos);
            uint64_t hash = ht->hash_fn ? ht->hash_fn(NULL, 0) : (uint64_t)i;
            size_t new_bucket = (size_t)(hash & (new_count - 1));
            kapi_hlist_add_head(pos, &new_buckets[new_bucket]);
        }
    }

    memory_free(ht->buckets);
    ht->buckets = new_buckets;
    ht->bits = new_bits;
    return KAPI_OK;
}

size_t kapi_hash_size(kapi_hash_table_t* ht)
{
    if (!ht || !ht->valid) {
        return 0;
    }
    return ht->size;
}

int kapi_hash_empty(kapi_hash_table_t* ht)
{
    if (!ht || !ht->valid) {
        return 1;
    }
    return ht->size == 0;
}

uint64_t kapi_hash_string(const void* key, size_t len)
{
    const uint8_t* p = (const uint8_t*)key;
    uint64_t hash = 5381;

    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + p[i];
    }

    return hash;
}

uint64_t kapi_hash_uint64(const void* key, size_t len)
{
    if (len < sizeof(uint64_t)) {
        return 0;
    }

    uint64_t val = *(const uint64_t*)key;
    val = (~val) + (val << 21);
    val = val ^ (val >> 24);
    val = (val + (val << 3)) + (val << 8);
    val = val ^ (val >> 14);
    val = (val + (val << 2)) + (val << 4);
    val = val ^ (val >> 28);
    val = val + (val << 31);
    return val;
}

uint64_t kapi_hash_ptr(const void* key, size_t len)
{
    uint64_t val = (uint64_t)(uintptr_t)key;
    val = (~val) + (val << 21);
    val = val ^ (val >> 24);
    val = (val + (val << 3)) + (val << 8);
    val = val ^ (val >> 14);
    val = (val + (val << 2)) + (val << 4);
    val = val ^ (val >> 28);
    val = val + (val << 31);
    (void)len;
    return val;
}