#ifndef KAPI_SIGNALFD_H
#define KAPI_SIGNALFD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_SIGNALFD_SIGINFO_SIZE 128

typedef struct {
    uint32_t ssi_signo;
    int32_t  ssi_errno;
    int32_t  ssi_code;
    uint32_t ssi_uid;
    int32_t  ssi_pid;
    int32_t  ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t  ssi_status;
    int32_t  ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint8_t  ssi_pad[KAPI_SIGNALFD_SIGINFO_SIZE - 80];
} kapi_signalfd_siginfo_t;

#define KAPI_SFD_CLOEXEC  0x080000
#define KAPI_SFD_NONBLOCK 0x0800

int  kapi_signalfd(int fd, const uint64_t* mask, size_t masksize, int flags);
int  kapi_signalfd4(int fd, const uint64_t* mask, size_t masksize, int flags);
int  kapi_signalfd_init(void);

#ifdef __cplusplus
}
#endif

#endif