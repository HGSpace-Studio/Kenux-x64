#include <arch/process.h>
#include <arch/memory.h>
#include <string.h>

#define SIGNAL_MAX  64
#define SIG_DFL    ((void(*)(int))0)
#define SIG_IGN   ((void(*)(int))1)

#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGIOT     6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1    10
#define SIGSEGV    11
#define SIGUSR2    12
#define SIGPIPE    13
#define SIGALRM    14
#define SIGTERM    15
#define SIGSTKFLT  16
#define SIGCHLD    17
#define SIGCONT    18
#define SIGSTOP    19
#define SIGTSTP    20
#define SIGTTIN    21
#define SIGTTOU    22
#define SIGURG     23
#define SIGXCPU    24
#define SIGXFSZ    25
#define SIGVTALRM  26
#define SIGPROF    27
#define SIGWINCH   28
#define SIGIO      29
#define SIGPWR     30
#define SIGSYS     31

typedef void (*sighandler_t)(int);

typedef struct {
    uint64_t pending;
    uint64_t blocked;
    sighandler_t handlers[SIGNAL_MAX];
    uint64_t siginfo_signo;
    uint64_t siginfo_code;
    uint64_t siginfo_value;
} signal_state_t;

static signal_state_t signal_states[PROCESS_MAX];

void signal_init(void)
{
    memset(signal_states, 0, sizeof(signal_states));
    for (int i = 0; i < PROCESS_MAX; i++) {
        for (int j = 0; j < SIGNAL_MAX; j++) {
            signal_states[i].handlers[j] = SIG_DFL;
        }
    }
}

sighandler_t signal_register_handler(uint64_t pid, int signum, sighandler_t handler)
{
    if (pid >= PROCESS_MAX || signum <= 0 || signum >= SIGNAL_MAX) return SIG_DFL;
    if (signum == SIGKILL || signum == SIGSTOP) return SIG_DFL;

    sighandler_t old = signal_states[pid].handlers[signum];
    signal_states[pid].handlers[signum] = handler;
    return old;
}

int signal_send(uint64_t target_pid, int signum)
{
    if (target_pid >= PROCESS_MAX || signum <= 0 || signum >= SIGNAL_MAX) return -1;

    signal_state_t* ss = &signal_states[target_pid];
    ss->pending |= (1ULL << signum);

    process_t* proc = process_get_by_id(target_pid);
    if (!proc) return -2;

    if (proc->state == PROCESS_SLEEPING || proc->state == PROCESS_STOPPED) {
        if (signum == SIGCONT) {
            proc->state = PROCESS_READY;
        }
    }

    if (signum == SIGSTOP || signum == SIGTSTP || signum == SIGTTIN || signum == SIGTTOU) {
        proc->state = PROCESS_STOPPED;
    }

    if (signum == SIGKILL) {
        proc->state = PROCESS_TERMINATED;
    }

    return 0;
}

int signal_send_group(uint64_t pgid, int signum)
{
    if (signum <= 0 || signum >= SIGNAL_MAX) return -1;

    int count = 0;
    for (int i = 0; i < PROCESS_MAX; i++) {
        process_t* proc = process_get_by_id(i);
        if (proc && proc->pgid == pgid) {
            if (signal_send(i, signum) == 0) count++;
        }
    }
    return count;
}

static int signal_default_action(int signum)
{
    switch (signum) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGILL:
    case SIGTRAP:
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGKILL:
    case SIGSEGV:
    case SIGPIPE:
    case SIGALRM:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
    case SIGSTKFLT:
    case SIGXCPU:
    case SIGXFSZ:
    case SIGVTALRM:
    case SIGPROF:
    case SIGIO:
    case SIGPWR:
    case SIGSYS:
        return 0;
    case SIGCHLD:
    case SIGURG:
    case SIGWINCH:
    case SIGCONT:
        return 1;
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
        return 2;
    default:
        return 0;
    }
}

int signal_dispatch(uint64_t pid)
{
    if (pid >= PROCESS_MAX) return -1;

    signal_state_t* ss = &signal_states[pid];
    uint64_t pending = ss->pending & ~ss->blocked;

    if (pending == 0) return 0;

    int signum = 0;
    for (int i = 1; i < SIGNAL_MAX; i++) {
        if (pending & (1ULL << i)) {
            signum = i;
            break;
        }
    }

    if (signum == 0) return 0;

    ss->pending &= ~(1ULL << signum);
    ss->siginfo_signo = signum;

    sighandler_t handler = ss->handlers[signum];

    if (handler == SIG_IGN) {
        return 1;
    }

    if (handler == SIG_DFL) {
        int action = signal_default_action(signum);
        switch (action) {
        case 0:
            process_terminate(pid, signum);
            return 2;
        case 1:
            return 3;
        case 2:
            return 4;
        default:
            return 5;
        }
    }

    ss->blocked |= (1ULL << signum);

    process_t* proc = process_get_by_id(pid);
    if (proc) {
        proc->pending_signals = ss->pending;
        proc->signal_mask = ss->blocked;
    }

    ss->blocked &= ~(1ULL << signum);
    return 6;
}

int signal_block(uint64_t pid, int signum)
{
    if (pid >= PROCESS_MAX || signum <= 0 || signum >= SIGNAL_MAX) return -1;
    if (signum == SIGKILL || signum == SIGSTOP) return -2;
    signal_states[pid].blocked |= (1ULL << signum);
    return 0;
}

int signal_unblock(uint64_t pid, int signum)
{
    if (pid >= PROCESS_MAX || signum <= 0 || signum >= SIGNAL_MAX) return -1;
    if (signum == SIGKILL || signum == SIGSTOP) return -2;
    signal_states[pid].blocked &= ~(1ULL << signum);
    return 0;
}

uint64_t signal_get_pending(uint64_t pid)
{
    if (pid >= PROCESS_MAX) return 0;
    return signal_states[pid].pending;
}

uint64_t signal_get_blocked(uint64_t pid)
{
    if (pid >= PROCESS_MAX) return 0;
    return signal_states[pid].blocked;
}

int signal_sigprocmask(uint64_t pid, int how, uint64_t set, uint64_t* oldset)
{
    if (pid >= PROCESS_MAX) return -1;

    signal_state_t* ss = &signal_states[pid];

    if (oldset) *oldset = ss->blocked;

    switch (how) {
    case 0:
        ss->blocked = set;
        break;
    case 1:
        ss->blocked |= set;
        break;
    case 2:
        ss->blocked &= ~set;
        break;
    default:
        return -2;
    }

    ss->blocked |= (1ULL << SIGKILL) | (1ULL << SIGSTOP);

    return 0;
}