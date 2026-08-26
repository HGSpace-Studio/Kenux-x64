#include "kapi_signalfd.h"
#include "kapi.h"
#include <string.h>

#define KAPI_SIGNALFD_MAX 16

typedef struct {
    int      valid;
    int      fd;
    uint64_t mask;
    int      cloexec;
    int      nonblock;
} kapi_signalfd_inst_t;

static kapi_signalfd_inst_t signalfd_instances[KAPI_SIGNALFD_MAX];
static int signalfd_next_fd = 200;

int kapi_signalfd(int fd, const uint64_t* mask, size_t masksize, int flags)
{
    (void)flags;
    if (!mask) return KAPI_EINVAL;

    if (fd >= 0) {
        for (int i = 0; i < KAPI_SIGNALFD_MAX; i++) {
            if (signalfd_instances[i].valid && signalfd_instances[i].fd == fd) {
                if (masksize >= sizeof(uint64_t)) signalfd_instances[i].mask = *mask;
                return fd;
            }
        }
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SIGNALFD_MAX; i++) {
        if (!signalfd_instances[i].valid) {
            signalfd_instances[i].valid = 1;
            signalfd_instances[i].fd = signalfd_next_fd++;
            if (masksize >= sizeof(uint64_t)) signalfd_instances[i].mask = *mask;
            signalfd_instances[i].cloexec = 0;
            signalfd_instances[i].nonblock = 0;
            return signalfd_instances[i].fd;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_signalfd4(int fd, const uint64_t* mask, size_t masksize, int flags)
{
    int ret = kapi_signalfd(fd, mask, masksize, flags);
    if (ret >= 0) {
        for (int i = 0; i < KAPI_SIGNALFD_MAX; i++) {
            if (signalfd_instances[i].valid && signalfd_instances[i].fd == ret) {
                if (flags & KAPI_SFD_CLOEXEC) signalfd_instances[i].cloexec = 1;
                if (flags & KAPI_SFD_NONBLOCK) signalfd_instances[i].nonblock = 1;
                break;
            }
        }
    }
    return ret;
}

int kapi_signalfd_init(void)
{
    memset(signalfd_instances, 0, sizeof(signalfd_instances));
    return KAPI_OK;
}