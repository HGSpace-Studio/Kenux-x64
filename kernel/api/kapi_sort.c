#include "kapi_sort.h"
#include "kapi.h"

#include <string.h>

static void kapi_swap(void* a, void* b, size_t size)
{
    uint8_t tmp[256];
    size_t chunk;
    while (size > 0) {
        chunk = size > 256 ? 256 : size;
        memcpy(tmp, a, chunk);
        memcpy(a, b, chunk);
        memcpy(b, tmp, chunk);
        a = (uint8_t*)a + chunk;
        b = (uint8_t*)b + chunk;
        size -= chunk;
    }
}

static void kapi_sift_down(void* base, size_t root, size_t num, size_t size, kapi_cmp_func_t cmp)
{
    uint8_t* arr = (uint8_t*)base;
    while (1) {
        size_t child = 2 * root + 1;
        if (child >= num) {
            break;
        }
        if (child + 1 < num && cmp(arr + (child + 1) * size, arr + child * size) > 0) {
            child++;
        }
        if (cmp(arr + child * size, arr + root * size) <= 0) {
            break;
        }
        kapi_swap(arr + root * size, arr + child * size, size);
        root = child;
    }
}

void kapi_heapsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp)
{
    if (!base || num < 2 || !cmp) {
        return;
    }

    size_t i;
    for (i = num / 2; i > 0; i--) {
        kapi_sift_down(base, i - 1, num, size, cmp);
    }
    for (i = num - 1; i > 0; i--) {
        kapi_swap(base, (uint8_t*)base + i * size, size);
        kapi_sift_down(base, 0, i, size, cmp);
    }
}

static void kapi_qsort_internal(void* base, size_t low, size_t high, size_t size, kapi_cmp_func_t cmp)
{
    if (low >= high) {
        return;
    }
    uint8_t* arr = (uint8_t*)base;
    size_t pivot = low;
    size_t i = low + 1;
    size_t j = high;

    while (i <= j) {
        while (i <= high && cmp(arr + i * size, arr + pivot * size) <= 0) {
            i++;
        }
        while (j > low && cmp(arr + j * size, arr + pivot * size) > 0) {
            j--;
        }
        if (i < j) {
            kapi_swap(arr + i * size, arr + j * size, size);
        }
    }
    if (j > low) {
        kapi_swap(arr + pivot * size, arr + j * size, size);
    }
    if (j > 0) {
        kapi_qsort_internal(base, low, j - 1, size, cmp);
    }
    kapi_qsort_internal(base, j + 1, high, size, cmp);
}

void kapi_qsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp)
{
    if (!base || num < 2 || !cmp) {
        return;
    }
    kapi_qsort_internal(base, 0, num - 1, size, cmp);
}

void kapi_sort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp)
{
    kapi_qsort(base, num, size, cmp);
}

void kapi_insertsort(void* base, size_t num, size_t size, kapi_cmp_func_t cmp)
{
    if (!base || num < 2 || !cmp) {
        return;
    }
    uint8_t* arr = (uint8_t*)base;
    uint8_t tmp[256];
    size_t j;
    for (size_t i = 1; i < num; i++) {
        memcpy(tmp, arr + i * size, size);
        for (j = i; j > 0 && cmp(arr + (j - 1) * size, tmp) > 0; j--) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
        }
        memcpy(arr + j * size, tmp, size);
    }
}

void kapi_list_sort(void* priv, void* head, int (*cmp)(void* priv, void* a, void* b))
{
    if (!head || !cmp) {
        return;
    }
}