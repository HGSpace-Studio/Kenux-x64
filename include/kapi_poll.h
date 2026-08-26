#ifndef KAPI_POLL_H
#define KAPI_POLL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_POLLIN     0x001
#define KAPI_POLLPRI    0x002
#define KAPI_POLLOUT    0x004
#define KAPI_POLLERR    0x008
#define KAPI_POLLHUP    0x010
#define KAPI_POLLNVAL   0x020
#define KAPI_POLLRDNORM 0x040
#define KAPI_POLLRDBAND 0x080
#define KAPI_POLLWRNORM 0x100
#define KAPI_POLLWRBAND 0x200
#define KAPI_POLLMSG    0x400
#define KAPI_POLLREMOVE 0x1000
#define KAPI_POLLRDHUP  0x2000

typedef struct {
    int      fd;
    short    events;
    short    revents;
} kapi_pollfd_t;

#define KAPI_FD_SETSIZE 1024

typedef struct {
    uint64_t bits[KAPI_FD_SETSIZE / 64];
} kapi_fd_set_t;

#define KAPI_FD_ZERO(set) do { memset((set), 0, sizeof(*(set))); } while (0)
#define KAPI_FD_SET(fd, set)   do { (set)->bits[(fd) / 64] |= (1ULL << ((fd) % 64)); } while (0)
#define KAPI_FD_CLR(fd, set)   do { (set)->bits[(fd) / 64] &= ~(1ULL << ((fd) % 64)); } while (0)
#define KAPI_FD_ISSET(fd, set) (!!((set)->bits[(fd) / 64] & (1ULL << ((fd) % 64))))

int kapi_poll(kapi_pollfd_t* fds, uint32_t nfds, int timeout);
int kapi_select(int nfds, kapi_fd_set_t* readfds, kapi_fd_set_t* writefds,
                kapi_fd_set_t* exceptfds, uint64_t* timeout_ms);
int kapi_pselect6(int nfds, kapi_fd_set_t* readfds, kapi_fd_set_t* writefds,
                  kapi_fd_set_t* exceptfds, uint64_t* timeout_ms,
                  const uint64_t* sigmask);
int kapi_ppoll(kapi_pollfd_t* fds, uint32_t nfds, uint64_t* timeout_ms,
               const uint64_t* sigmask);

int kapi_poll_init(void);

#ifdef __cplusplus
}
#endif

#endif