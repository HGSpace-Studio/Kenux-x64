

#include "kapi_idr.h"
#include "kapi.h"

#include <string.h>

int kapi_idr_init(kapi_idr_t* idr)
{
    if (!idr) {
        return KAPI_EINVAL;
    }

    memset(idr->ptr, 0, sizeof(idr->ptr));
    memset(idr->bitmap, 0, sizeof(idr->bitmap));
    idr->next_id = 0;
    return KAPI_OK;
}

void kapi_idr_destroy(kapi_idr_t* idr)
{
    if (!idr) {
        return;
    }

    memset(idr->ptr, 0, sizeof(idr->ptr));
    memset(idr->bitmap, 0, sizeof(idr->bitmap));
    idr->next_id = 0;
}

int kapi_idr_alloc(kapi_idr_t* idr, void* ptr)
{
    if (!idr || !ptr) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_IDR_MAX; i++) {
        int id = (idr->next_id + i) % KAPI_IDR_MAX;
        if (idr->ptr[id] == NULL) {
            idr->ptr[id] = ptr;
            idr->bitmap[id / (sizeof(unsigned long) * 8)] |= (1UL << (id % (sizeof(unsigned long) * 8)));
            idr->next_id = (id + 1) % KAPI_IDR_MAX;
            return id;
        }
    }

    return KAPI_ERROR;
}

void kapi_idr_free(kapi_idr_t* idr, int id)
{
    if (!idr || id < 0 || id >= KAPI_IDR_MAX) {
        return;
    }

    idr->ptr[id] = NULL;
    idr->bitmap[id / (sizeof(unsigned long) * 8)] &= ~(1UL << (id % (sizeof(unsigned long) * 8)));
}

void* kapi_idr_find(const kapi_idr_t* idr, int id)
{
    if (!idr || id < 0 || id >= KAPI_IDR_MAX) {
        return NULL;
    }

    return idr->ptr[id];
}

int kapi_idr_alloc_fixed(kapi_idr_t* idr, int id, void* ptr)
{
    if (!idr || !ptr || id < 0 || id >= KAPI_IDR_MAX) {
        return KAPI_EINVAL;
    }

    if (idr->ptr[id] != NULL) {
        return KAPI_EBUSY;
    }

    idr->ptr[id] = ptr;
    idr->bitmap[id / (sizeof(unsigned long) * 8)] |= (1UL << (id % (sizeof(unsigned long) * 8)));
    return id;
}
