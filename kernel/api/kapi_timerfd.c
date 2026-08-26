#include "kapi_timerfd.h"
#include "kapi.h"
#include <string.h>

#define KAPI_TIMERFD_MAX 16

typedef struct {
    int              valid;
    int              fd;
    int              clockid;
    int              cloexec;
    int              nonblock;
    uint64_t         expire_sec;
    uint64_t         expire_nsec;
    uint64_t         interval_sec;
    uint64_t         interval_nsec;
    int              armed;
} kapi_timerfd_inst_t;

static kapi_timerfd_inst_t timerfd_instances[KAPI_TIMERFD_MAX];
static int timerfd_next_fd = 300;

int kapi_timerfd_create(int clockid, int flags)
{
    if (clockid < 0 || clockid > 2) return KAPI_EINVAL;

    for (int i = 0; i < KAPI_TIMERFD_MAX; i++) {
        if (!timerfd_instances[i].valid) {
            timerfd_instances[i].valid = 1;
            timerfd_instances[i].fd = timerfd_next_fd++;
            timerfd_instances[i].clockid = clockid;
            timerfd_instances[i].cloexec = (flags & KAPI_TFD_CLOEXEC) ? 1 : 0;
            timerfd_instances[i].nonblock = (flags & KAPI_TFD_NONBLOCK) ? 1 : 0;
            timerfd_instances[i].expire_sec = 0;
            timerfd_instances[i].expire_nsec = 0;
            timerfd_instances[i].interval_sec = 0;
            timerfd_instances[i].interval_nsec = 0;
            timerfd_instances[i].armed = 0;
            return timerfd_instances[i].fd;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_timerfd_settime(int fd, int flags, const kapi_itimerspec_t* new_value,
                          kapi_itimerspec_t* old_value)
{
    (void)flags;
    for (int i = 0; i < KAPI_TIMERFD_MAX; i++) {
        if (timerfd_instances[i].valid && timerfd_instances[i].fd == fd) {
            if (old_value) {
                old_value[0].tv_sec = timerfd_instances[i].expire_sec;
                old_value[0].tv_nsec = timerfd_instances[i].expire_nsec;
                old_value[1].tv_sec = timerfd_instances[i].interval_sec;
                old_value[1].tv_nsec = timerfd_instances[i].interval_nsec;
            }
            if (new_value) {
                timerfd_instances[i].expire_sec = new_value[0].tv_sec;
                timerfd_instances[i].expire_nsec = new_value[0].tv_nsec;
                timerfd_instances[i].interval_sec = new_value[1].tv_sec;
                timerfd_instances[i].interval_nsec = new_value[1].tv_nsec;
                timerfd_instances[i].armed = (new_value[0].tv_sec != 0 || new_value[0].tv_nsec != 0) ? 1 : 0;
            }
            return KAPI_OK;
        }
    }
    return KAPI_EINVAL;
}

int kapi_timerfd_gettime(int fd, kapi_itimerspec_t* curr_value)
{
    for (int i = 0; i < KAPI_TIMERFD_MAX; i++) {
        if (timerfd_instances[i].valid && timerfd_instances[i].fd == fd) {
            if (curr_value) {
                curr_value[0].tv_sec = timerfd_instances[i].expire_sec;
                curr_value[0].tv_nsec = timerfd_instances[i].expire_nsec;
                curr_value[1].tv_sec = timerfd_instances[i].interval_sec;
                curr_value[1].tv_nsec = timerfd_instances[i].interval_nsec;
            }
            return KAPI_OK;
        }
    }
    return KAPI_EINVAL;
}

int kapi_timerfd_init(void)
{
    memset(timerfd_instances, 0, sizeof(timerfd_instances));
    return KAPI_OK;
}