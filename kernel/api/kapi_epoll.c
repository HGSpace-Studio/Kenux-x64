#include "kapi_epoll.h"
#include "kapi.h"
#include <string.h>

typedef struct {
    int              fd;
    uint32_t         events;
    uint32_t         revents;
    kapi_epoll_data_t data;
} kapi_epoll_entry_t;

typedef struct {
    int                  valid;
    int                  cloexec;
    kapi_epoll_entry_t*  entries;
    int                  entry_count;
    int                  entry_capacity;
} kapi_epoll_instance_t;

static kapi_epoll_instance_t epoll_instances[KAPI_EPOLL_MAX_INSTANCES];
static int epoll_fd_base = 100;

static int epoll_alloc_instance(void)
{
    for (int i = 0; i < KAPI_EPOLL_MAX_INSTANCES; i++) {
        if (!epoll_instances[i].valid) {
            epoll_instances[i].valid = 1;
            epoll_instances[i].cloexec = 0;
            epoll_instances[i].entries = (kapi_epoll_entry_t*)kapi_kmalloc(sizeof(kapi_epoll_entry_t) * 16);
            if (!epoll_instances[i].entries) {
                epoll_instances[i].valid = 0;
                return -1;
            }
            epoll_instances[i].entry_count = 0;
            epoll_instances[i].entry_capacity = 16;
            return i;
        }
    }
    return -1;
}

static kapi_epoll_instance_t* epoll_get_instance(int epfd)
{
    int idx = epfd - epoll_fd_base;
    if (idx < 0 || idx >= KAPI_EPOLL_MAX_INSTANCES) return NULL;
    if (!epoll_instances[idx].valid) return NULL;
    return &epoll_instances[idx];
}

static void epoll_check_events(kapi_epoll_instance_t* inst)
{
    for (int i = 0; i < inst->entry_count; i++) {
        kapi_epoll_entry_t* e = &inst->entries[i];
        e->revents = 0;
        if (e->events & KAPI_EPOLLIN) {
            e->revents |= KAPI_EPOLLIN;
        }
        if (e->events & KAPI_EPOLLOUT) {
            e->revents |= KAPI_EPOLLOUT;
        }
        if (e->events & KAPI_EPOLLERR) {
            e->revents |= KAPI_EPOLLERR;
        }
        if (e->events & KAPI_EPOLLHUP) {
            e->revents |= KAPI_EPOLLHUP;
        }
    }
}

int kapi_epoll_create1(int flags)
{
    int idx = epoll_alloc_instance();
    if (idx < 0) return KAPI_ERROR;
    if (flags & KAPI_SFD_CLOEXEC) {
        epoll_instances[idx].cloexec = 1;
    }
    return idx + epoll_fd_base;
}

int kapi_epoll_create(int size)
{
    (void)size;
    return kapi_epoll_create1(0);
}

int kapi_epoll_ctl(int epfd, int op, int fd, kapi_epoll_event_t* event)
{
    kapi_epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) return KAPI_ERROR;

    switch (op) {
    case KAPI_EPOLL_CTL_ADD: {
        for (int i = 0; i < inst->entry_count; i++) {
            if (inst->entries[i].fd == fd) return KAPI_EINVAL;
        }
        if (inst->entry_count >= inst->entry_capacity) {
            int new_cap = inst->entry_capacity * 2;
            kapi_epoll_entry_t* new_entries = (kapi_epoll_entry_t*)kapi_kmalloc(sizeof(kapi_epoll_entry_t) * new_cap);
            if (!new_entries) return KAPI_ENOMEM;
            memcpy(new_entries, inst->entries, sizeof(kapi_epoll_entry_t) * inst->entry_count);
            kapi_kfree(inst->entries);
            inst->entries = new_entries;
            inst->entry_capacity = new_cap;
        }
        kapi_epoll_entry_t* e = &inst->entries[inst->entry_count++];
        e->fd = fd;
        e->events = event->events;
        e->revents = 0;
        e->data = event->data;
        return KAPI_OK;
    }
    case KAPI_EPOLL_CTL_MOD: {
        for (int i = 0; i < inst->entry_count; i++) {
            if (inst->entries[i].fd == fd) {
                inst->entries[i].events = event->events;
                inst->entries[i].data = event->data;
                return KAPI_OK;
            }
        }
        return KAPI_ENOENT;
    }
    case KAPI_EPOLL_CTL_DEL: {
        for (int i = 0; i < inst->entry_count; i++) {
            if (inst->entries[i].fd == fd) {
                inst->entries[i] = inst->entries[inst->entry_count - 1];
                inst->entry_count--;
                return KAPI_OK;
            }
        }
        return KAPI_ENOENT;
    }
    default:
        return KAPI_EINVAL;
    }
}

int kapi_epoll_wait(int epfd, kapi_epoll_event_t* events, int maxevents, int timeout)
{
    kapi_epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) return KAPI_ERROR;
    if (maxevents <= 0) return KAPI_EINVAL;

    epoll_check_events(inst);

    int count = 0;
    for (int i = 0; i < inst->entry_count && count < maxevents; i++) {
        kapi_epoll_entry_t* e = &inst->entries[i];
        if (e->revents) {
            events[count].events = e->revents;
            events[count].data = e->data;
            count++;
            if (e->events & KAPI_EPOLLET) {
                e->revents = 0;
            }
        }
    }

    if (count == 0 && timeout > 0) {
        kapi_proc_msleep(timeout > 100 ? 100 : timeout);
        epoll_check_events(inst);
        for (int i = 0; i < inst->entry_count && count < maxevents; i++) {
            kapi_epoll_entry_t* e = &inst->entries[i];
            if (e->revents) {
                events[count].events = e->revents;
                events[count].data = e->data;
                count++;
                if (e->events & KAPI_EPOLLET) {
                    e->revents = 0;
                }
            }
        }
    }

    return count;
}

int kapi_epoll_pwait(int epfd, kapi_epoll_event_t* events, int maxevents, int timeout, const uint64_t* sigmask)
{
    (void)sigmask;
    return kapi_epoll_wait(epfd, events, maxevents, timeout);
}

int kapi_epoll_init(void)
{
    memset(epoll_instances, 0, sizeof(epoll_instances));
    return KAPI_OK;
}