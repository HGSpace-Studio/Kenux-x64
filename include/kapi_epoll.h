#ifndef KAPI_EPOLL_H
#define KAPI_EPOLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_EPOLL_CTL_ADD 1
#define KAPI_EPOLL_CTL_MOD 2
#define KAPI_EPOLL_CTL_DEL 3

#define KAPI_EPOLLIN       0x001
#define KAPI_EPOLLOUT      0x002
#define KAPI_EPOLLRDNORM   0x040
#define KAPI_EPOLLRDBAND   0x080
#define KAPI_EPOLLPRI      0x008
#define KAPI_EPOLLWRNORM   0x100
#define KAPI_EPOLLWRBAND   0x200
#define KAPI_EPOLLERR      0x004
#define KAPI_EPOLLHUP      0x008
#define KAPI_EPOLLRDHUP    0x2000
#define KAPI_EPOLLWAKEUP   (1 << 29)
#define KAPI_EPOLLEXCLUSIVE (1 << 28)
#define KAPI_EPOLLONESHOT  (1 << 30)
#define KAPI_EPOLLET       (1 << 31)

typedef union {
    void*    ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} kapi_epoll_data_t;

typedef struct {
    uint32_t          events;
    kapi_epoll_data_t data;
} kapi_epoll_event_t;

#define KAPI_EPOLL_MAX_INSTANCES  16
#define KAPI_EPOLL_MAX_EVENTS     1024

int  kapi_epoll_create1(int flags);
int  kapi_epoll_create(int size);
int  kapi_epoll_ctl(int epfd, int op, int fd, kapi_epoll_event_t* event);
int  kapi_epoll_wait(int epfd, kapi_epoll_event_t* events, int maxevents, int timeout);
int  kapi_epoll_pwait(int epfd, kapi_epoll_event_t* events, int maxevents, int timeout, const uint64_t* sigmask);

int  kapi_epoll_init(void);

#ifdef __cplusplus
}
#endif

#endif