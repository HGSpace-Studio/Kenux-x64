#ifndef KAPI_HASH_H
#define KAPI_HASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_hlist_node   kapi_hlist_node_t;
typedef struct kapi_hlist_head   kapi_hlist_head_t;
typedef struct kapi_hash_table   kapi_hash_table_t;

struct kapi_hlist_node {
    kapi_hlist_node_t* next;
    kapi_hlist_node_t** pprev;
};

struct kapi_hlist_head {
    kapi_hlist_node_t* first;
};

#define KAPI_HLIST_HEAD_INIT { .first = NULL }
#define KAPI_HLIST_HEAD(name) kapi_hlist_head_t name = { .first = NULL }

#define KAPI_HASH_BITS_DEFAULT 8
#define KAPI_HASH_BITS_MIN     4
#define KAPI_HASH_BITS_MAX     20

typedef uint64_t (*kapi_hash_fn)(const void* key, size_t len);
typedef int (*kapi_hash_cmp_fn)(const void* a, const void* b, size_t len);

static inline void kapi_hlist_init(kapi_hlist_head_t* head)
{
    head->first = NULL;
}

static inline void kapi_hlist_node_init(kapi_hlist_node_t* node)
{
    node->next = NULL;
    node->pprev = NULL;
}

static inline int kapi_hlist_empty(const kapi_hlist_head_t* head)
{
    return head->first == NULL;
}

static inline int kapi_hlist_unhashed(const kapi_hlist_node_t* node)
{
    return node->pprev == NULL;
}

static inline void kapi_hlist_del(kapi_hlist_node_t* node)
{
    if (node->pprev) {
        kapi_hlist_node_t* next = node->next;
        kapi_hlist_node_t** pprev = node->pprev;
        *pprev = next;
        if (next) {
            next->pprev = pprev;
        }
        node->next = NULL;
        node->pprev = NULL;
    }
}

static inline void kapi_hlist_add_head(kapi_hlist_node_t* node,
                                        kapi_hlist_head_t* head)
{
    kapi_hlist_node_t* first = head->first;
    node->next = first;
    if (first) {
        first->pprev = &node->next;
    }
    head->first = node;
    node->pprev = &head->first;
}

static inline void kapi_hlist_add_before(kapi_hlist_node_t* node,
                                          kapi_hlist_node_t* next)
{
    node->pprev = next->pprev;
    node->next = next;
    next->pprev = &node->next;
    if (node->pprev) {
        *(node->pprev) = node;
    }
}

static inline void kapi_hlist_add_behind(kapi_hlist_node_t* node,
                                          kapi_hlist_node_t* prev)
{
    node->next = prev->next;
    prev->next = node;
    node->pprev = &prev->next;
    if (node->next) {
        node->next->pprev = &node->next;
    }
}

#define kapi_hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

#define kapi_hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); pos = n)

#define kapi_hlist_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - (uint64_t)(&((type*)0)->member)))

#define kapi_hlist_for_each_entry(pos, head, member) \
    for (pos = kapi_hlist_entry((head)->first, typeof(*pos), member); \
         &pos->member != NULL; \
         pos = kapi_hlist_entry(pos->member.next, typeof(*pos), member))

#define kapi_hlist_for_each_entry_safe(pos, n, head, member) \
    for (pos = kapi_hlist_entry((head)->first, typeof(*pos), member); \
         &pos->member != NULL && ({ n = kapi_hlist_entry(pos->member.next, \
             typeof(*pos), member); 1; }); \
         pos = n)

kapi_hash_table_t* kapi_hash_create(size_t bits, kapi_hash_fn hash_fn,
                                     kapi_hash_cmp_fn cmp_fn);

void kapi_hash_destroy(kapi_hash_table_t* ht);

int kapi_hash_add(kapi_hash_table_t* ht, const void* key, size_t key_len,
                  kapi_hlist_node_t* node);

int kapi_hash_del(kapi_hash_table_t* ht, kapi_hlist_node_t* node);

kapi_hlist_node_t* kapi_hash_find(kapi_hash_table_t* ht, const void* key,
                                   size_t key_len);

int kapi_hash_resize(kapi_hash_table_t* ht, size_t new_bits);

size_t kapi_hash_size(kapi_hash_table_t* ht);

int kapi_hash_empty(kapi_hash_table_t* ht);

uint64_t kapi_hash_string(const void* key, size_t len);

uint64_t kapi_hash_uint64(const void* key, size_t len);

uint64_t kapi_hash_ptr(const void* key, size_t len);

int kapi_hash_init(void);

#ifdef __cplusplus
}
#endif

#endif