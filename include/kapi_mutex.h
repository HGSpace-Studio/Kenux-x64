

#ifndef KAPI_MUTEX_H
#define KAPI_MUTEX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_mutex* kapi_mutex_t;

kapi_mutex_t kapi_mutex_create(void);

void kapi_mutex_destroy(kapi_mutex_t mtx);

void kapi_mutex_lock(kapi_mutex_t mtx);

int kapi_mutex_trylock(kapi_mutex_t mtx);

void kapi_mutex_unlock(kapi_mutex_t mtx);

int kapi_mutex_is_locked(kapi_mutex_t mtx);

#ifdef __cplusplus
}
#endif

#endif
