#pragma once

#include "sys/types.h"

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED 31

#define SIGRTMIN  32
#define SIGRTMAX  64

#define NSIG      65

#define SIG_ERR   ((void (*)(int))-1)
#define SIG_DFL   ((void (*)(int))0)
#define SIG_IGN   ((void (*)(int))1)

#define SA_NOCLDSTOP  1
#define SA_NOCLDWAIT  2
#define SA_SIGINFO    4
#define SA_ONSTACK    0x08000000
#define SA_RESTART    0x10000000
#define SA_NODEFER    0x40000000
#define SA_RESETHAND  0x80000000

#define SS_ONSTACK 1
#define SS_DISABLE 2

#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

typedef int sig_atomic_t;

typedef void (*sighandler_t)(int);

union sigval {
    int   sival_int;
    void *sival_ptr;
};

typedef struct {
    int      si_signo;
    int      si_errno;
    int      si_code;
    pid_t    si_pid;
    uid_t    si_uid;
    void    *si_addr;
    int      si_status;
    union sigval si_value;
} siginfo_t;

struct sigaction {
    void     (*sa_handler)(int);
    void     (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t  sa_mask;
    int       sa_flags;
    void    (*sa_restorer)(void);
};

typedef struct {
    void     *ss_sp;
    size_t    ss_size;
    int       ss_flags;
} stack_t;

#ifdef __cplusplus
extern "C" {
#endif

sighandler_t signal(int signum, sighandler_t handler);
int          raise(int signum);
int          kill(pid_t pid, int signum);
int          killpg(int pgrp, int signum);
int          tgkill(pid_t tgid, pid_t tid, int signum);
int          pthread_kill(pid_t thread, int signum);

int          sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int          sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int          sigpending(sigset_t *set);
int          sigsuspend(const sigset_t *mask);
int          sigwait(const sigset_t *set, int *signum);

int          sigemptyset(sigset_t *set);
int          sigfillset(sigset_t *set);
int          sigaddset(sigset_t *set, int signum);
int          sigdelset(sigset_t *set, int signum);
int          sigismember(const sigset_t *set, int signum);

int          sigaltstack(const stack_t *ss, stack_t *old_ss);

unsigned int alarm(unsigned int seconds);
int          pause(void);

#ifdef __cplusplus
}
#endif