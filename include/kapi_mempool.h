#ifndef KAPI_MEMPOOL_H
#define KAPI_MEMPOOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_mempool kapi_mempool_t;

typedef void* (*kapi_mempool_alloc_fn)(size_t size);
typedef void  (*kapi_mempool_free_fn)(void* ptr);

kapi_mempool_t* kapi_mempool_create(size_t element_size, size_t min_nr,
                                     size_t max_nr, kapi_mempool_alloc_fn alloc,
                                     kapi_mempool_free_fn free);

void kapi_mempool_destroy(kapi_mempool_t* pool);

void* kapi_mempool_alloc(kapi_mempool_t* pool);

void kapi_mempool_free(kapi_mempool_t* pool, void* element);

int kapi_mempool_resize(kapi_mempool_t* pool, size_t new_min_nr);

size_t kapi_mempool_avail(kapi_mempool_t* pool);

size_t kapi_mempool_size(kapi_mempool_t* pool);

int kapi_mempool_init(void);

#ifdef __cplusplus
}
#endif

#endif