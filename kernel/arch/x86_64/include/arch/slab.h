

#ifndef ARCH_SLAB_H
#define ARCH_SLAB_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SLAB_NAME_MAX       32
#define SLAB_MAX_CACHES     64

#define SLAB_FULL           0
#define SLAB_PARTIAL        1
#define SLAB_EMPTY          2

typedef struct slab {
    struct slab* next;
    struct slab* prev;
    void*        mem;
    uint32_t     obj_size;
    uint32_t     obj_count;
    uint32_t     free_count;
    uint32_t     inuse;
    void*        free_list;
    uint32_t     state;
} slab_t;

typedef struct kmem_cache {
    char         name[SLAB_NAME_MAX];
    uint32_t     obj_size;
    uint32_t     align;
    uint32_t     objs_per_slab;
    uint32_t     slab_order;

    slab_t*      full_slabs;
    slab_t*      partial_slabs;
    slab_t*      empty_slabs;

    uint64_t     total_objs;
    uint64_t     free_objs;
    spinlock_t   lock;
} kmem_cache_t;

void slab_init(void);

kmem_cache_t* kmem_cache_create(const char* name, uint32_t size, uint32_t align);
void kmem_cache_destroy(kmem_cache_t* cache);

void* kmem_cache_alloc(kmem_cache_t* cache);
void kmem_cache_free(kmem_cache_t* cache, void* obj);

void* kmalloc(uint64_t size);
void* kzalloc(uint64_t size);
void  kfree(void* ptr);
uint64_t ksize(void* ptr);

#define KMALLOC_MIN_SIZE    8
#define KMALLOC_SHIFT_HIGH  11
#define KMALLOC_CACHES      (KMALLOC_SHIFT_HIGH + 1)

#endif
