

#ifndef KAPI_KTHREAD_H
#define KAPI_KTHREAD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_kthread* kapi_kthread_t;

kapi_kthread_t kapi_kthread_run(const char* name, int (*func)(void*), void* arg);

void kapi_kthread_stop(kapi_kthread_t kt);

int kapi_kthread_join(kapi_kthread_t kt, uint64_t* ret);

int kapi_kthread_should_stop(void);

void kapi_kthread_wake(kapi_kthread_t kt);

#ifdef __cplusplus
}
#endif

#endif
