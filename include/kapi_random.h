

#ifndef KAPI_RANDOM_H
#define KAPI_RANDOM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void kapi_get_random_bytes(void* buf, size_t len);

uint32_t kapi_get_random_u32(void);

uint64_t kapi_get_random_u64(void);

#ifdef __cplusplus
}
#endif

#endif
