#pragma once

#include "sys/types.h"

#define GRND_NONBLOCK 1
#define GRND_RANDOM   2
#define GRND_INSECURE 4

#ifdef __cplusplus
extern "C" {
#endif

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);

#ifdef __cplusplus
}
#endif