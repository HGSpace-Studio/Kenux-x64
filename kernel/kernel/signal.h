

#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <arch/types.h>
#include <arch/process.h>

#define SIG_MAX        64
#define SIG_WORDS      (SIG_MAX / 64)

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
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

#define SIGRTMIN  32
#define SIGRTMAX  63

#define SA_NOCLDSTOP  0x00000001
#define SA_NOCLDWAIT  0x00000002
#define SA_SIGINFO    0x00000004
#define SA_RESTART    0x00000010
#define SA_ONSTACK    0x00000020

typedef struct {
    uint64_t bits[SIG_WORDS];
} k_sigset_t;

static inline void sigaddset(k_sigset_t* set, int sig)
{
    if (sig > 0 && sig <= SIG_MAX) set->bits[(sig - 1) / 64] |= (1ULL << ((sig - 1) % 64));
}
static inline void sigdelset(k_sigset_t* set, int sig)
{
    if (sig > 0 && sig <= SIG_MAX) set->bits[(sig - 1) / 64] &= ~(1ULL << ((sig - 1) % 64));
}
static inline int sigismember(const k_sigset_t* set, int sig)
{
    if (sig > 0 && sig <= SIG_MAX) return (set->bits[(sig - 1) / 64] >> ((sig - 1) % 64)) & 1;
    return 0;
}

typedef struct {
    int    si_signo;
    int    si_code;
    int    si_errno;
    uint64_t si_addr;
    int    si_uid;
    int    si_pid;
    union {
        int    sival_int;
        void*  sival_ptr;
    } si_value;
} k_siginfo_t;

typedef void (*sighandler_t)(int);

typedef struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, k_siginfo_t*, void*);
    k_sigset_t sa_mask;
    uint32_t    sa_flags;
} k_sigaction_t;

#define SIGQUEUE_MAX  8

typedef struct sigqueue_entry {
    k_siginfo_t info;
    struct sigqueue_entry* next;
} sigqueue_entry_t;

typedef struct signal_struct {
    k_sigaction_t actions[SIG_MAX];
    k_sigset_t    blocked;
    k_sigset_t    pending;
    k_sigset_t    shared_pending;
    sigqueue_entry_t* queues[SIG_MAX];
    uint64_t flags;
} signal_struct_t;

#define SIGNAL_FLAG_UNKILLABLE  0x01

void signal_init(void);

signal_struct_t* signal_create(void);
void signal_destroy(signal_struct_t* sig);

int signal_send(uint64_t target_pid, int sig, k_siginfo_t* info);
int signal_send_info(uint64_t target_pid, int sig, int code, uint64_t addr);
int signal_send_group(uint64_t target_pid, int sig);

void signal_do_pending(void);

int signal_sigaction(int sig, const k_sigaction_t* act, k_sigaction_t* oldact);

int signal_sigprocmask(int how, const k_sigset_t* set, k_sigset_t* oldset);

int signal_sigpending(k_sigset_t* set);
int signal_sigsuspend(const k_sigset_t* mask);

const char* signal_name(int sig);

#endif
