

#include "epoll.h"
#include <arch/spinlock.h>
#include <arch/memory.h>
#include <string.h>
#include <slab.h>

extern uint64_t current_process;

extern void printk(const char* fmt, ...);

static epoll_manager_t g_epoll_mgr;

#define RQ_MASK  (EPOLL_READY_QUEUE_SIZE - 1)

static inline int rq_empty(epoll_instance_t* inst)
{
    return inst->rq_head == inst->rq_tail;
}

static inline int rq_full(epoll_instance_t* inst)
{
    return ((inst->rq_tail + 1) & RQ_MASK) == inst->rq_head;
}

static inline int rq_push(epoll_instance_t* inst, int fd,
                           uint32_t events, uint64_t data)
{
    if (rq_full(inst)) {
        return -1;
    }
    uint32_t tail = inst->rq_tail;
    inst->ready_queue[tail].fd     = fd;
    inst->ready_queue[tail].events = events;
    inst->ready_queue[tail].data   = data;
    inst->ready_queue[tail].valid  = 1;
    inst->rq_tail = (tail + 1) & RQ_MASK;
    return 0;
}

static inline int rq_pop(epoll_instance_t* inst, epoll_event_t* out)
{
    if (rq_empty(inst)) {
        return -1;
    }
    uint32_t head = inst->rq_head;
    out->events = inst->ready_queue[head].events;
    out->data   = inst->ready_queue[head].data;
    inst->ready_queue[head].valid = 0;
    inst->rq_head = (head + 1) & RQ_MASK;
    return 0;
}

void epoll_init(void)
{
    memset(&g_epoll_mgr, 0, sizeof(g_epoll_mgr));
    spin_init(&g_epoll_mgr.lock);
    g_epoll_mgr.active_count = 0;
}

static int epoll_alloc_instance(void)
{
    int idx = -1;
    spin_lock(&g_epoll_mgr.lock);

    if (g_epoll_mgr.active_count >= EPOLL_MAX_INSTANCES) {
        spin_unlock(&g_epoll_mgr.lock);
        return -1;
    }

    for (int i = 0; i < EPOLL_MAX_INSTANCES; i++) {
        if (!g_epoll_mgr.instances[i].active) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        epoll_instance_t* inst = &g_epoll_mgr.instances[idx];
        memset(inst, 0, sizeof(epoll_instance_t));
        inst->active   = 1;
        inst->fd_count = 0;
        inst->rq_head  = 0;
        inst->rq_tail  = 0;
        spin_init(&inst->lock);
        wait_queue_init(&inst->wait);
        g_epoll_mgr.active_count++;
    }

    spin_unlock(&g_epoll_mgr.lock);
    return idx;
}

static epoll_instance_t* epoll_get_instance(int epfd)
{
    if (epfd < 0 || epfd >= EPOLL_MAX_INSTANCES) {
        return NULL;
    }
    epoll_instance_t* inst = &g_epoll_mgr.instances[epfd];
    if (!inst->active) {
        return NULL;
    }
    return inst;
}

int epoll_create(int size)
{
    (void)size;

    int idx = epoll_alloc_instance();
    if (idx < 0) {
        printk("[epoll] epoll_create: 无法分配实例\n");
        return -1;
    }

    return idx;
}

int epoll_create1(int flags)
{
    int idx = epoll_alloc_instance();
    if (idx < 0) {
        printk("[epoll] epoll_create1: 无法分配实例\n");
        return -1;
    }

    (void)flags;

    return idx;
}

static int epoll_find_fd(epoll_instance_t* inst, int fd)
{
    for (int i = 0; i < EPOLL_MAX_FDS; i++) {
        if (inst->fds[i].registered && inst->fds[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

static int epoll_find_free_slot(epoll_instance_t* inst)
{
    for (int i = 0; i < EPOLL_MAX_FDS; i++) {
        if (!inst->fds[i].registered) {
            return i;
        }
    }
    return -1;
}

int epoll_ctl(int epfd, int op, int fd, epoll_event_t* event)
{

    if (fd < 0) {
        return -1;
    }

    epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) {
        return -1;
    }

    spin_lock(&inst->lock);

    switch (op) {
    case EPOLL_CTL_ADD: {

        if (epoll_find_fd(inst, fd) >= 0) {
            spin_unlock(&inst->lock);
            return -1;
        }

        if (inst->fd_count >= EPOLL_MAX_FDS) {
            spin_unlock(&inst->lock);
            return -1;
        }
        if (!event) {
            spin_unlock(&inst->lock);
            return -1;
        }

        int slot = epoll_find_free_slot(inst);
        if (slot < 0) {
            spin_unlock(&inst->lock);
            return -1;
        }

        inst->fds[slot].fd         = fd;
        inst->fds[slot].event      = *event;
        inst->fds[slot].registered = 1;
        inst->fds[slot].ready      = 0;
        inst->fd_count++;

        spin_unlock(&inst->lock);
        return 0;
    }

    case EPOLL_CTL_MOD: {
        int slot = epoll_find_fd(inst, fd);
        if (slot < 0) {
            spin_unlock(&inst->lock);
            return -1;
        }
        if (!event) {
            spin_unlock(&inst->lock);
            return -1;
        }

        inst->fds[slot].event = *event;

        spin_unlock(&inst->lock);
        return 0;
    }

    case EPOLL_CTL_DEL: {
        int slot = epoll_find_fd(inst, fd);
        if (slot < 0) {
            spin_unlock(&inst->lock);
            return -1;
        }

        inst->fds[slot].fd         = -1;
        inst->fds[slot].registered = 0;
        inst->fds[slot].ready      = 0;
        memset(&inst->fds[slot].event, 0, sizeof(epoll_event_t));
        inst->fd_count--;

        spin_unlock(&inst->lock);
        return 0;
    }

    default:
        spin_unlock(&inst->lock);
        return -1;
    }
}

int epoll_wait(int epfd, epoll_event_t* events, int maxevents, int timeout)
{

    if (!events || maxevents <= 0 || maxevents > EPOLL_MAX_EVENTS) {
        return -1;
    }

    epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) {
        return -1;
    }

    spin_lock(&inst->lock);

    if (rq_empty(inst)) {
        if (timeout == 0) {

            spin_unlock(&inst->lock);
            return 0;
        }

        if (timeout < 0) {

            wait_queue_wait(&inst->wait, &inst->lock);
        } else {

            wait_queue_wait_timeout(&inst->wait, &inst->lock, (uint64_t)timeout);
        }

        spin_lock(&inst->lock);
    }

    int count = 0;
    while (count < maxevents && !rq_empty(inst)) {
        epoll_event_t ev;
        if (rq_pop(inst, &ev) != 0) {
            break;
        }

        events[count] = ev;
        count++;
    }

    spin_unlock(&inst->lock);
    return count;
}

void epoll_notify(int fd, uint32_t events)
{
    if (fd < 0) {
        return;
    }

    spin_lock(&g_epoll_mgr.lock);

    for (int i = 0; i < EPOLL_MAX_INSTANCES; i++) {
        epoll_instance_t* inst = &g_epoll_mgr.instances[i];
        if (!inst->active) {
            continue;
        }

        spin_lock(&inst->lock);

        for (int j = 0; j < EPOLL_MAX_FDS; j++) {
            if (!inst->fds[j].registered || inst->fds[j].fd != fd) {
                continue;
            }

            epoll_fd_t* efd = &inst->fds[j];
            uint32_t registered_events = efd->event.events & ~EPOLLET;
            uint32_t matched = events & registered_events;

            if (matched == 0) {
                continue;
            }

            if (!(efd->event.events & EPOLLET)) {

                if (!efd->ready) {
                    efd->ready = 1;
                    rq_push(inst, fd, matched, efd->event.data);
                }
            }

            else {

                if (!efd->ready) {
                    efd->ready = 1;
                    rq_push(inst, fd, matched, efd->event.data);
                }

            }

            wait_queue_wake_all(&inst->wait);

            break;
        }

        spin_unlock(&inst->lock);
    }

    spin_unlock(&g_epoll_mgr.lock);
}

void epoll_notify_clear_ready(int fd)
{
    spin_lock(&g_epoll_mgr.lock);

    for (int i = 0; i < EPOLL_MAX_INSTANCES; i++) {
        epoll_instance_t* inst = &g_epoll_mgr.instances[i];
        if (!inst->active) {
            continue;
        }

        spin_lock(&inst->lock);
        int slot = epoll_find_fd(inst, fd);
        if (slot >= 0) {
            inst->fds[slot].ready = 0;
        }
        spin_unlock(&inst->lock);
    }

    spin_unlock(&g_epoll_mgr.lock);
}

int epoll_close(int epfd)
{
    epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) {
        return -1;
    }

    spin_lock(&g_epoll_mgr.lock);

    spin_lock(&inst->lock);
    inst->active = 0;

    wait_queue_wake_all(&inst->wait);

    for (int i = 0; i < EPOLL_MAX_FDS; i++) {
        inst->fds[i].registered = 0;
        inst->fds[i].ready      = 0;
        inst->fds[i].fd         = -1;
        memset(&inst->fds[i].event, 0, sizeof(epoll_event_t));
    }
    inst->fd_count = 0;

    inst->rq_head = 0;
    inst->rq_tail = 0;
    for (int i = 0; i < EPOLL_READY_QUEUE_SIZE; i++) {
        inst->ready_queue[i].valid = 0;
    }

    spin_unlock(&inst->lock);

    g_epoll_mgr.active_count--;

    spin_unlock(&g_epoll_mgr.lock);
    return 0;
}
