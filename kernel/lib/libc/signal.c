#include "signal.h"
#include <stddef.h>


#define SYS_kill         62
#define SYS_getpid       39
#define SYS_rt_sigaction 13


#define SIGSET_SIZE      8

typedef unsigned long sigset_t;


struct sigaction {
    void (*sa_handler)(int);
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
};

static inline long syscall0(long n)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx","r11","memory");
    return ret;
}

static inline long syscall2(long n, long a1, long a2)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx","r11","memory");
    return ret;
}

static inline long syscall4(long n, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                      : "rcx","r11","memory");
    return ret;
}


static pid_t getpid_internal(void)
{
    return (pid_t)syscall0(SYS_getpid);
}


int raise(int sig)
{
    return (int)syscall2(SYS_kill, getpid_internal(), sig);
}


sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction act, oldact;
    act.sa_handler = handler;
    act.sa_flags = 0;
    act.sa_restorer = NULL;
    act.sa_mask = 0;

    long ret = syscall4(SYS_rt_sigaction, signum, (long)&act, (long)&oldact, SIGSET_SIZE);
    if (ret < 0) {
        return SIG_ERR;
    }
    return oldact.sa_handler;
}


int kill(pid_t pid, int sig)
{
    return (int)syscall2(SYS_kill, pid, sig);
}
