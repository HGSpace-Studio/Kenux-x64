

#ifndef KAPI_SMP_H
#define KAPI_SMP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int kapi_smp_processor_id(void);

int kapi_on_each_cpu(void (*func)(void*), void* arg);

#ifdef __cplusplus
}
#endif

#endif
