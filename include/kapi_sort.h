#ifndef KAPI_SORT_H
#define KAPI_SORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*kapi_cmp_func_t)(const void* a, const void* b);

void kapi_list_sort(void* priv, void* head, int (*cmp)(void* priv, void* a, void* b));

void kapi_heapsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp);

void kapi_qsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp);

void kapi_sort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp);

void kapi_insertsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp);

#ifdef __cplusplus
}
#endif

#endif