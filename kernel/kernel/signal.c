

#include "signal.h"
#include <arch/spinlock.h>
#include <arch/pagefault.h>
#include <string.h>
#include <slab.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];

static const char* signal_names[SIG_MAX + 1] = {
    NULL, "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE",
    "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM",
    "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU",
    "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO",
    "PWR", "SYS"
};

const char* signal_name(int sig)
{
    if (sig >= 1 && sig <= 31) return signal_names[sig];
    if (sig >= SIGRTMIN && sig <= SIGRTMAX) return "RT";
    return "UNKNOWN";
}

void signal_init(void)
{
    signal_struct_t* sig = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig) {
        sig = signal_create();
        processes[current_process].signal_data = sig;
    }

    /* 初始化信号队列为空，所有信号处理设为默认 SIG_DFL */
    for (int i = 0; i < SIG_MAX; i++) {
        sig->actions[i].sa_handler = NULL;   /* SIG_DFL */
        sig->actions[i].sa_flags = 0;

        sigqueue_entry_t* entry = sig->queues[i];
        while (entry) {
            sigqueue_entry_t* next = entry->next;
            kfree(entry);
            entry = next;
        }
        sig->queues[i] = NULL;
    }

    /* SIGKILL 与 SIGSTOP 不可捕获 */
    sig->actions[SIGKILL - 1].sa_handler = NULL;
    sig->actions[SIGSTOP - 1].sa_handler = NULL;

    memset(&sig->blocked, 0, sizeof(k_sigset_t));
    memset(&sig->pending, 0, sizeof(k_sigset_t));
    memset(&sig->shared_pending, 0, sizeof(k_sigset_t));
    sig->flags = 0;
}

signal_struct_t* signal_create(void)
{
    signal_struct_t* sig = (signal_struct_t*)kzalloc(sizeof(signal_struct_t));
    if (!sig) return NULL;

    for (int i = 0; i < SIG_MAX; i++) {
        sig->actions[i].sa_handler = NULL;
        sig->actions[i].sa_flags = 0;
    }

    sig->actions[SIGKILL - 1].sa_handler = NULL;
    sig->actions[SIGSTOP - 1].sa_handler = NULL;

    memset(&sig->blocked, 0, sizeof(k_sigset_t));
    memset(&sig->pending, 0, sizeof(k_sigset_t));
    memset(&sig->shared_pending, 0, sizeof(k_sigset_t));
    memset(sig->queues, 0, sizeof(sig->queues));
    sig->flags = 0;

    return sig;
}

void signal_destroy(signal_struct_t* sig)
{
    if (!sig) return;

    for (int i = 0; i < SIG_MAX; i++) {
        sigqueue_entry_t* entry = sig->queues[i];
        while (entry) {
            sigqueue_entry_t* next = entry->next;
            kfree(entry);
            entry = next;
        }
    }
    kfree(sig);
}

static int __signal_blocked(signal_struct_t* sig, int signo)
{
    return sigismember(&sig->blocked, signo);
}

static void __signal_default(int sig, uint64_t pid)
{
    switch (sig) {
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        case SIGIO:

            break;
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            processes[pid].state = PROCESS_STOPPED;
            break;
        case SIGCONT:
            if (processes[pid].state == PROCESS_STOPPED) {
                processes[pid].state = PROCESS_READY;
            }
            break;
        case SIGKILL:
        default:

            processes[pid].state = PROCESS_DEAD;
            break;
    }
}

int signal_send(uint64_t target_pid, int sig, k_siginfo_t* info)
{
    if (sig < 1 || sig > SIG_MAX) return -1;
    if (target_pid >= PROCESS_MAX) return -1;

    process_t* target = &processes[target_pid];
    if (target->state == PROCESS_DEAD) return -1;

    if (sig == SIGKILL) {
        target->state = PROCESS_DEAD;
        return 0;
    }

    signal_struct_t* sig_data = (signal_struct_t*)target->signal_data;
    if (!sig_data) return -1;

    if (sig >= SIGRTMIN) {
        sigqueue_entry_t* entry = (sigqueue_entry_t*)kzalloc(sizeof(sigqueue_entry_t));
        if (!entry) return -1;
        if (info) {
            entry->info = *info;
        }
        entry->info.si_signo = sig;

        sigqueue_entry_t** tail = &sig_data->queues[sig - 1];
        int count = 0;
        while (*tail) {
            tail = &(*tail)->next;
            count++;
        }
        if (count < SIGQUEUE_MAX) {
            *tail = entry;
            sigaddset(&sig_data->pending, sig);
        } else {
            kfree(entry);
        }
    } else {

        sigaddset(&sig_data->pending, sig);
    }

    if (target->state == PROCESS_SLEEPING || target->state == PROCESS_WAITING) {
        target->state = PROCESS_READY;
    }

    return 0;
}

int signal_send_info(uint64_t target_pid, int sig, int code, uint64_t addr)
{
    k_siginfo_t info;
    memset(&info, 0, sizeof(info));
    info.si_signo = sig;
    info.si_code = code;
    info.si_addr = addr;
    info.si_pid = current_process;
    return signal_send(target_pid, sig, &info);
}

int signal_send_group(uint64_t target_pid, int sig)
{
    return signal_send(target_pid, sig, NULL);
}

/* 简化版用户上下文结构，用于信号帧 */
typedef struct {
    uint64_t        uc_flags;
    uint64_t        uc_link;
    k_sigset_t      uc_sigmask;
    process_context_t uc_mcontext;
} k_ucontext_t;

static void __signal_setup_frame(int sig, k_sigaction_t* act,
                                  signal_struct_t* sig_data)
{
    process_t* p = &processes[current_process];
    uint64_t rsp = p->context.rsp;

    /* 16 字节栈对齐 */
    rsp &= ~0xFul;

    /* 压入 ucontext（包含旧上下文和信号掩码） */
    rsp -= sizeof(k_ucontext_t);
    k_ucontext_t* uc = (k_ucontext_t*)rsp;
    memset(uc, 0, sizeof(k_ucontext_t));
    uc->uc_sigmask = sig_data->blocked;
    uc->uc_mcontext = p->context;

    /* 压入 siginfo */
    rsp -= sizeof(k_siginfo_t);
    k_siginfo_t* siginfo_ptr = (k_siginfo_t*)rsp;
    memset(siginfo_ptr, 0, sizeof(k_siginfo_t));
    siginfo_ptr->si_signo = sig;
    siginfo_ptr->si_pid = current_process;

    /* 占位返回地址（实际应由 sigreturn trampoline 填充） */
    rsp -= 8;
    *(uint64_t*)rsp = 0;

    /* 修改进程上下文，指向用户自定义 handler */
    p->context.rsp = rsp;
    p->context.rip = (uint64_t)act->sa_handler;
    p->context.rdi = (uint64_t)sig;           /* 第一个参数 */

    if (act->sa_flags & SA_SIGINFO) {
        p->context.rsi = (uint64_t)siginfo_ptr; /* 第二个参数 */
        p->context.rdx = (uint64_t)uc;          /* 第三个参数 */
    }
}

void signal_do_pending(void)
{
    signal_struct_t* sig_data = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig_data) return;

    for (int sig = 1; sig <= SIG_MAX; sig++) {
        if (!sigismember(&sig_data->pending, sig)) continue;
        if (__signal_blocked(sig_data, sig)) continue;

        k_sigaction_t* act = &sig_data->actions[sig - 1];

        /* SIG_IGN：直接丢弃 */
        if (act->sa_handler == (void*)1) {
            sigdelset(&sig_data->pending, sig);
            if (sig >= SIGRTMIN && sig_data->queues[sig - 1]) {
                sigqueue_entry_t* entry = sig_data->queues[sig - 1];
                sig_data->queues[sig - 1] = entry->next;
                kfree(entry);
            }
            continue;
        }

        /* SIG_DFL：执行默认行为 */
        if (act->sa_handler == NULL) {
            sigdelset(&sig_data->pending, sig);
            if (sig >= SIGRTMIN && sig_data->queues[sig - 1]) {
                sigqueue_entry_t* entry = sig_data->queues[sig - 1];
                sig_data->queues[sig - 1] = entry->next;
                kfree(entry);
            }
            __signal_default(sig, current_process);
            continue;
        }

        /* 用户自定义 handler */
        sigdelset(&sig_data->pending, sig);

        /* 提取实时信号的附加信息 */
        k_siginfo_t info;
        memset(&info, 0, sizeof(info));
        info.si_signo = sig;
        info.si_pid = current_process;
        if (sig >= SIGRTMIN && sig_data->queues[sig - 1]) {
            sigqueue_entry_t* entry = sig_data->queues[sig - 1];
            info = entry->info;
            sig_data->queues[sig - 1] = entry->next;
            kfree(entry);
        }

        /* 应用 sa_mask 并自动阻塞当前信号（除非 SA_NODEFER） */
        k_sigset_t old_blocked = sig_data->blocked;
        for (int i = 0; i < SIG_WORDS; i++) {
            sig_data->blocked.bits[i] |= act->sa_mask.bits[i];
        }
        if (!(act->sa_flags & 0x40000000)) {
            sigaddset(&sig_data->blocked, sig);
        }
        sigdelset(&sig_data->blocked, SIGKILL);
        sigdelset(&sig_data->blocked, SIGSTOP);

        /* 构造信号帧并修改进程上下文 */
        __signal_setup_frame(sig, act, sig_data);

        /* 每次只处理一个未决信号，避免递归 */
        break;
    }
}

int signal_sigaction(int sig, const k_sigaction_t* act, k_sigaction_t* oldact)
{
    if (sig < 1 || sig > SIG_MAX) return -1;
    if (sig == SIGKILL || sig == SIGSTOP) return -1;

    signal_struct_t* sig_data = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig_data) return -1;

    if (oldact) {
        *oldact = sig_data->actions[sig - 1];
    }

    if (act) {
        sig_data->actions[sig - 1] = *act;
    }

    return 0;
}

int signal_sigprocmask(int how, const k_sigset_t* set, k_sigset_t* oldset)
{
    signal_struct_t* sig_data = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig_data) return -1;

    if (oldset) {
        *oldset = sig_data->blocked;
    }

    if (!set) return 0;

    switch (how) {
        case 0:
            for (int i = 0; i < SIG_WORDS; i++) {
                sig_data->blocked.bits[i] |= set->bits[i];
            }
            break;
        case 1:
            for (int i = 0; i < SIG_WORDS; i++) {
                sig_data->blocked.bits[i] &= ~set->bits[i];
            }
            break;
        case 2:
            sig_data->blocked = *set;
            break;
        default:
            return -1;
    }

    sigdelset(&sig_data->blocked, SIGKILL);
    sigdelset(&sig_data->blocked, SIGSTOP);

    return 0;
}

int signal_sigpending(k_sigset_t* set)
{
    signal_struct_t* sig_data = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig_data || !set) return -1;
    *set = sig_data->pending;
    return 0;
}

int signal_sigsuspend(const k_sigset_t* mask)
{
    signal_struct_t* sig_data = (signal_struct_t*)processes[current_process].signal_data;
    if (!sig_data || !mask) return -1;

    k_sigset_t old_blocked = sig_data->blocked;
    sig_data->blocked = *mask;
    sigdelset(&sig_data->blocked, SIGKILL);
    sigdelset(&sig_data->blocked, SIGSTOP);

    processes[current_process].state = PROCESS_SLEEPING;
    extern void process_yield(void);
    process_yield();

    sig_data->blocked = old_blocked;
    return 0;
}
