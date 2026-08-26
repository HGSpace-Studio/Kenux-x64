#include "kapi_mempool.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

#define SERIAL_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)

struct kapi_mempool {
    void**                free_list;
    size_t                element_size;
    size_t                min_nr;
    size_t                max_nr;
    size_t                curr_nr;
    size_t                free_count;
    kapi_mempool_alloc_fn alloc_fn;
    kapi_mempool_free_fn  free_fn;
    int                   valid;
};

#define KAPI_MEMPOOL_MAX_POOLS 64

static kapi_mempool_t kapi_mempool_table[KAPI_MEMPOOL_MAX_POOLS];
static int kapi_mempool_initialized = 0;

int kapi_mempool_init(void)
{
    SERIAL_DBG('j');
    if (kapi_mempool_initialized) {
        return KAPI_OK;
    }

    SERIAL_DBG('k');
    memset(kapi_mempool_table, 0, sizeof(kapi_mempool_table));
    kapi_mempool_initialized = 1;
    return KAPI_OK;
}

kapi_mempool_t* kapi_mempool_create(size_t element_size, size_t min_nr,
                                     size_t max_nr, kapi_mempool_alloc_fn alloc,
                                     kapi_mempool_free_fn free)
{
    if (element_size == 0 || min_nr == 0 || max_nr < min_nr) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MEMPOOL_MAX_POOLS; i++) {
        if (!kapi_mempool_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    kapi_mempool_t* pool = &kapi_mempool_table[slot];
    memset(pool, 0, sizeof(*pool));

    pool->free_list = (void**)memory_alloc(max_nr * sizeof(void*));
    if (!pool->free_list) {
        return NULL;
    }

    pool->element_size = element_size;
    pool->min_nr = min_nr;
    pool->max_nr = max_nr;
    pool->alloc_fn = alloc ? alloc : memory_alloc;
    pool->free_fn = free ? free : memory_free;

    for (size_t i = 0; i < min_nr; i++) {
        void* elem = pool->alloc_fn(element_size);
        if (!elem) {
            for (size_t j = 0; j < i; j++) {
                pool->free_fn(pool->free_list[j]);
            }
            memory_free(pool->free_list);
            pool->free_list = NULL;
            return NULL;
        }
        pool->free_list[i] = elem;
    }

    pool->curr_nr = min_nr;
    pool->free_count = min_nr;
    pool->valid = 1;
    return pool;
}

void kapi_mempool_destroy(kapi_mempool_t* pool)
{
    if (!pool || !pool->valid) {
        return;
    }

    for (size_t i = 0; i < pool->curr_nr; i++) {
        if (pool->free_list[i]) {
            pool->free_fn(pool->free_list[i]);
        }
    }

    memory_free(pool->free_list);
    pool->free_list = NULL;
    pool->valid = 0;
}

void* kapi_mempool_alloc(kapi_mempool_t* pool)
{
    if (!pool || !pool->valid) {
        return NULL;
    }

    if (pool->free_count > 0) {
        pool->free_count--;
        return pool->free_list[pool->free_count];
    }

    if (pool->curr_nr >= pool->max_nr) {
        return NULL;
    }

    void* elem = pool->alloc_fn(pool->element_size);
    if (!elem) {
        return NULL;
    }

    pool->curr_nr++;
    return elem;
}

void kapi_mempool_free(kapi_mempool_t* pool, void* element)
{
    if (!pool || !pool->valid || !element) {
        return;
    }

    if (pool->free_count < pool->max_nr) {
        pool->free_list[pool->free_count] = element;
        pool->free_count++;
        return;
    }

    if (pool->curr_nr > pool->min_nr) {
        pool->free_fn(element);
        pool->curr_nr--;
    } else {
        pool->free_list[pool->free_count - 1] = element;
    }
}

int kapi_mempool_resize(kapi_mempool_t* pool, size_t new_min_nr)
{
    if (!pool || !pool->valid) {
        return KAPI_EINVAL;
    }

    if (new_min_nr > pool->max_nr) {
        return KAPI_EINVAL;
    }

    if (new_min_nr > pool->curr_nr) {
        for (size_t i = pool->curr_nr; i < new_min_nr; i++) {
            void* elem = pool->alloc_fn(pool->element_size);
            if (!elem) {
                return KAPI_ENOMEM;
            }
            pool->free_list[i] = elem;
            pool->free_count++;
        }
        pool->curr_nr = new_min_nr;
    } else if (new_min_nr < pool->curr_nr) {
        while (pool->curr_nr > new_min_nr && pool->free_count > 0) {
            pool->free_count--;
            pool->free_fn(pool->free_list[pool->free_count]);
            pool->curr_nr--;
        }
    }

    pool->min_nr = new_min_nr;
    return KAPI_OK;
}

size_t kapi_mempool_avail(kapi_mempool_t* pool)
{
    if (!pool || !pool->valid) {
        return 0;
    }
    return pool->free_count;
}

size_t kapi_mempool_size(kapi_mempool_t* pool)
{
    if (!pool || !pool->valid) {
        return 0;
    }
    return pool->curr_nr;
}