

#ifndef KAPI_IDR_H
#define KAPI_IDR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IDR_MAX 1024

typedef struct {
    void* ptr[KAPI_IDR_MAX];
    unsigned long bitmap[(KAPI_IDR_MAX + sizeof(unsigned long) * 8 - 1) / (sizeof(unsigned long) * 8)];
    int next_id;
} kapi_idr_t;

int kapi_idr_init(kapi_idr_t* idr);

void kapi_idr_destroy(kapi_idr_t* idr);

int kapi_idr_alloc(kapi_idr_t* idr, void* ptr);

void kapi_idr_free(kapi_idr_t* idr, int id);

void* kapi_idr_find(const kapi_idr_t* idr, int id);

int kapi_idr_alloc_fixed(kapi_idr_t* idr, int id, void* ptr);

#ifdef __cplusplus
}
#endif

#endif
