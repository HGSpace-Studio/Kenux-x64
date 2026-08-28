#ifndef KAPI_TIMERFD_H
#define KAPI_TIMERFD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_TFD_CLOEXEC  0x080000
#define KAPI_TFD_NONBLOCK 0x0800

#define KAPI_TFD_TIMER_ABSTIME   (1 << 0)
#define KFD_TIMER_CANCEL_ON_SET (1 << 1)

typedef struct {
    uint64_t tv_sec;
    uint64_t tv_nsec;
} kapi_itimerspec_t;

int  kapi_timerfd_create(int clockid, int flags);
int  kapi_timerfd_settime(int fd, int flags, const kapi_itimerspec_t* new_value,
                          kapi_itimerspec_t* old_value);
int  kapi_timerfd_gettime(int fd, kapi_itimerspec_t* curr_value);
int  kapi_timerfd_init(void);

#ifdef __cplusplus
}
#endif

#endif