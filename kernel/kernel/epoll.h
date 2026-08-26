

#ifndef KERNEL_EPOLL_H
#define KERNEL_EPOLL_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "wait.h"

#define EPOLL_MAX_INSTANCES    128
#define EPOLL_MAX_FDS          1024
#define EPOLL_MAX_EVENTS       64
#define EPOLL_READY_QUEUE_SIZE 256

#define EPOLL_CTL_ADD          1
#define EPOLL_CTL_MOD          2
#define EPOLL_CTL_DEL          3

#define EPOLLIN                0x001
#define EPOLLOUT               0x004
#define EPOLLERR               0x008
#define EPOLLHUP               0x010
#define EPOLLET                0x80000000

#define EPOLL_CLOEXEC          0x00080000

typedef struct {
    uint32_t events;
    uint64_t data;
} epoll_event_t;

typedef struct {
    int            fd;
    epoll_event_t  event;
    int            registered;
    int            ready;
} epoll_fd_t;

typedef struct {
    int            fd;
    uint32_t       events;
    uint64_t       data;
    uint32_t       valid;
} epoll_ready_entry_t;

typedef struct {
    int                 active;
    int                 fd_count;

    epoll_fd_t          fds[EPOLL_MAX_FDS];

    epoll_ready_entry_t ready_queue[EPOLL_READY_QUEUE_SIZE];
    uint32_t            rq_head;
    uint32_t            rq_tail;

    spinlock_t          lock;
    wait_queue_t        wait;
} epoll_instance_t;

typedef struct {
    epoll_instance_t instances[EPOLL_MAX_INSTANCES];
    int              active_count;
    spinlock_t       lock;
} epoll_manager_t;

int epoll_create(int size);

int epoll_create1(int flags);

int epoll_ctl(int epfd, int op, int fd, epoll_event_t* event);

int epoll_wait(int epfd, epoll_event_t* events, int maxevents, int timeout);

void epoll_notify(int fd, uint32_t events);

int epoll_close(int epfd);

void epoll_init(void);

#endif
