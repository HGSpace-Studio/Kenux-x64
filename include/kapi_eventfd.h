#ifndef KAPI_EVENTFD_H
#define KAPI_EVENTFD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_EFD_CLOEXEC  0x080000
#define KAPI_EFD_NONBLOCK 0x0800
#define KAPI_EFD_SEMAPHORE 0x0002

int  kapi_eventfd(unsigned int initval, int flags);
int  kapi_eventfd2(unsigned int initval, int flags);
int  kapi_eventfd_init(void);

#ifdef __cplusplus
}
#endif

#endif