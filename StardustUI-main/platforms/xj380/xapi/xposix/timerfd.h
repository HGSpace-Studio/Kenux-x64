#pragma once

#include "sys/types.h"

#define TFD_TIMER_ABSTIME 1
#define TFD_TIMER_CANCEL_ON_SET 2

#define TFD_CLOEXEC  02000000
#define TFD_NONBLOCK 00004000

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

#ifdef __cplusplus
extern "C" {
#endif

int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value,
                    struct itimerspec *old_value);
int timerfd_gettime(int fd, struct itimerspec *curr_value);

int eventfd(unsigned int initval, int flags);
int signalfd(int fd, const sigset_t *mask, int flags);

#ifdef __cplusplus
}
#endif