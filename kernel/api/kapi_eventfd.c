#include "kapi_eventfd.h"
#include "kapi.h"
#include <string.h>

#define KAPI_EVENTFD_MAX 32

typedef struct {
    int          valid;
    int          fd;
    uint64_t     counter;
    int          cloexec;
    int          nonblock;
    int          semaphore;
} kapi_eventfd_inst_t;

static kapi_eventfd_inst_t eventfd_instances[KAPI_EVENTFD_MAX];
static int eventfd_next_fd = 400;

int kapi_eventfd(unsigned int initval, int flags)
{
    for (int i = 0; i < KAPI_EVENTFD_MAX; i++) {
        if (!eventfd_instances[i].valid) {
            eventfd_instances[i].valid = 1;
            eventfd_instances[i].fd = eventfd_next_fd++;
            eventfd_instances[i].counter = (uint64_t)initval;
            eventfd_instances[i].cloexec = (flags & KAPI_EFD_CLOEXEC) ? 1 : 0;
            eventfd_instances[i].nonblock = (flags & KAPI_EFD_NONBLOCK) ? 1 : 0;
            eventfd_instances[i].semaphore = (flags & KAPI_EFD_SEMAPHORE) ? 1 : 0;
            return eventfd_instances[i].fd;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_eventfd2(unsigned int initval, int flags)
{
    return kapi_eventfd(initval, flags);
}

int kapi_eventfd_init(void)
{
    memset(eventfd_instances, 0, sizeof(eventfd_instances));
    return KAPI_OK;
}