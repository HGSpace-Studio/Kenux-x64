

#ifndef KAPI_COMPLETION_H
#define KAPI_COMPLETION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_completion* kapi_completion_t;

kapi_completion_t kapi_completion_create(void);

void kapi_completion_destroy(kapi_completion_t c);

void kapi_completion_wait(kapi_completion_t c);

void kapi_completion_complete(kapi_completion_t c);

void kapi_completion_reset(kapi_completion_t c);

#ifdef __cplusplus
}
#endif

#endif
