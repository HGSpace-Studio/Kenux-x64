

#ifndef KAPI_WAIT_H
#define KAPI_WAIT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_waitqueue* kapi_waitqueue_t;

kapi_waitqueue_t kapi_waitqueue_create(void);

void kapi_waitqueue_destroy(kapi_waitqueue_t wq);

void kapi_waitqueue_wake_up(kapi_waitqueue_t wq);

void kapi_waitqueue_wake_up_all(kapi_waitqueue_t wq);

#ifdef __cplusplus
}
#endif

#endif
