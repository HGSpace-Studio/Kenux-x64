#pragma once

#include "sys/types.h"

#define F_ULOCK 0
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

#ifdef __cplusplus
extern "C" {
#endif

int   flock(int fd, int operation);
int   lockf(int fd, int cmd, off_t len);

#ifdef __cplusplus
}
#endif