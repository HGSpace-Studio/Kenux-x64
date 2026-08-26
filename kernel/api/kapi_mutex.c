

#include "kapi_mutex.h"
#include "kapi.h"

#include <sync.h>
#include <arch/memory.h>
#include <string.h>

struct kapi_mutex {
    mutex_t mtx;
};

kapi_mutex_t kapi_mutex_create(void)
{
    kapi_mutex_t mtx = (kapi_mutex_t)memory_alloc(sizeof(struct kapi_mutex));
    if (!mtx) {
        return NULL;
    }
    mutex_init(&mtx->mtx);
    return mtx;
}

void kapi_mutex_destroy(kapi_mutex_t mtx)
{
    if (!mtx) {
        return;
    }
    memory_free(mtx);
}

void kapi_mutex_lock(kapi_mutex_t mtx)
{
    if (!mtx) {
        return;
    }
    mutex_lock(&mtx->mtx);
}

int kapi_mutex_trylock(kapi_mutex_t mtx)
{
    if (!mtx) {
        return 0;
    }
    return mutex_trylock(&mtx->mtx);
}

void kapi_mutex_unlock(kapi_mutex_t mtx)
{
    if (!mtx) {
        return;
    }
    mutex_unlock(&mtx->mtx);
}

int kapi_mutex_is_locked(kapi_mutex_t mtx)
{
    if (!mtx) {
        return 0;
    }
    return mutex_is_locked(&mtx->mtx);
}
