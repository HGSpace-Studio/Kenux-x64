#include "kapi_poll.h"
#include "kapi.h"
#include <string.h>

int kapi_poll(kapi_pollfd_t* fds, uint32_t nfds, int timeout)
{
    if (!fds && nfds > 0) return KAPI_EINVAL;

    int count = 0;
    for (uint32_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        if (fds[i].events & KAPI_POLLIN) {
            fds[i].revents |= KAPI_POLLIN;
        }
        if (fds[i].events & KAPI_POLLOUT) {
            fds[i].revents |= KAPI_POLLOUT;
        }
        if (fds[i].events & KAPI_POLLERR) {
            fds[i].revents |= KAPI_POLLERR;
        }
        if (fds[i].events & KAPI_POLLHUP) {
            fds[i].revents |= KAPI_POLLHUP;
        }
        if (fds[i].revents) count++;
    }

    if (count == 0 && timeout > 0) {
        kapi_proc_msleep(timeout > 100 ? 100 : timeout);
        for (uint32_t i = 0; i < nfds; i++) {
            if (fds[i].fd < 0) continue;
            if (fds[i].events & KAPI_POLLIN) fds[i].revents |= KAPI_POLLIN;
            if (fds[i].events & KAPI_POLLOUT) fds[i].revents |= KAPI_POLLOUT;
            if (fds[i].revents) count++;
        }
    }

    return count;
}

int kapi_select(int nfds, kapi_fd_set_t* readfds, kapi_fd_set_t* writefds,
                kapi_fd_set_t* exceptfds, uint64_t* timeout_ms)
{
    kapi_pollfd_t* pfds = (kapi_pollfd_t*)kapi_kmalloc(sizeof(kapi_pollfd_t) * nfds);
    if (!pfds) return KAPI_ENOMEM;

    int count = 0;
    for (int i = 0; i < nfds; i++) {
        short events = 0;
        if (readfds && KAPI_FD_ISSET(i, readfds)) events |= KAPI_POLLIN;
        if (writefds && KAPI_FD_ISSET(i, writefds)) events |= KAPI_POLLOUT;
        if (exceptfds && KAPI_FD_ISSET(i, exceptfds)) events |= KAPI_POLLERR;
        if (events) {
            pfds[count].fd = i;
            pfds[count].events = events;
            pfds[count].revents = 0;
            count++;
        }
    }

    int timeout = -1;
    if (timeout_ms) timeout = (int)(*timeout_ms / 1000000ULL);

    int ret = kapi_poll(pfds, count, timeout);

    if (readfds) KAPI_FD_ZERO(readfds);
    if (writefds) KAPI_FD_ZERO(writefds);
    if (exceptfds) KAPI_FD_ZERO(exceptfds);

    for (int i = 0; i < count; i++) {
        if (pfds[i].revents & KAPI_POLLIN) {
            if (readfds) KAPI_FD_SET(pfds[i].fd, readfds);
        }
        if (pfds[i].revents & KAPI_POLLOUT) {
            if (writefds) KAPI_FD_SET(pfds[i].fd, writefds);
        }
        if (pfds[i].revents & KAPI_POLLERR) {
            if (exceptfds) KAPI_FD_SET(pfds[i].fd, exceptfds);
        }
    }

    kapi_kfree(pfds);
    return ret;
}

int kapi_pselect6(int nfds, kapi_fd_set_t* readfds, kapi_fd_set_t* writefds,
                  kapi_fd_set_t* exceptfds, uint64_t* timeout_ms,
                  const uint64_t* sigmask)
{
    (void)sigmask;
    return kapi_select(nfds, readfds, writefds, exceptfds, timeout_ms);
}

int kapi_ppoll(kapi_pollfd_t* fds, uint32_t nfds, uint64_t* timeout_ms,
               const uint64_t* sigmask)
{
    (void)sigmask;
    int timeout = -1;
    if (timeout_ms) timeout = (int)(*timeout_ms / 1000000ULL);
    return kapi_poll(fds, nfds, timeout);
}

int kapi_poll_init(void)
{
    return KAPI_OK;
}