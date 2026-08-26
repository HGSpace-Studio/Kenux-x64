#ifndef ARCH_X86_64_PROCESS_H
#define ARCH_X86_64_PROCESS_H

#include <arch/types.h>
#include <arch/cfs.h>

#define PROCESS_MAX            256
#define PROCESS_STACK_SIZE     16384
#define PROCESS_NAME_MAX       32
#define THREAD_NAME_MAX        32

#define PROCESS_TERMINATED     0
#define PROCESS_READY          1
#define PROCESS_RUNNING        2
#define PROCESS_SLEEPING       3
#define PROCESS_WAITING        4
#define PROCESS_ZOMBIE         5
#define PROCESS_STOPPED        6
#define PROCESS_DEAD           7
#define PROCESS_UNUSED         8

#define PRIORITY_IDLE          0
#define PRIORITY_LOW           1
#define PRIORITY_NORMAL        2
#define PRIORITY_HIGH          3
#define PRIORITY_REALTIME      4
#define PRIORITY_COUNT         5

#define PROC_FLAG_KTHREAD      0x0001
#define PROC_FLAG_USER         0x0002
#define PROC_FLAG_FIXED        0x0004
#define PROC_FLAG_EXITING      0x0008

/* 兼容性宏 */
#define PROCESS_FLAG_KTHREAD   PROC_FLAG_KTHREAD
#define PROCESS_FLAG_FIXED     PROC_FLAG_FIXED

#define THREAD_MAX_PER_PROC    16
#define THREAD_MAX             1024

typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi;
    uint64_t rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

    uint64_t cr3;
    uint64_t gs_base;
    uint64_t fs_base;
} process_context_t;

typedef struct {
    uint64_t          id;
    uint64_t          parent_id;
    uint64_t          state;
    uint64_t          priority;
    uint64_t          flags;
    char              name[PROCESS_NAME_MAX];

    process_context_t context;
    void*             stack;
    void*             stack_bottom;
    void*             entry;
    void*             entry_arg;

    uint64_t cpu_time_ms;
    uint64_t sched_count;
    uint64_t mem_usage;
    uint64_t rss;

    void*    stack_top;
    void*    stack_base;

    uint64_t pending_signals;
    uint64_t signal_mask;
    void*    signal_data;

    void*    wait_channel;
    uint64_t wakeup_time;

    int      fd_table[16];
    int      fd_count;

    char     cwd[256];

    uint8_t  fpu_state[512] __attribute__((aligned(16)));
    uint64_t fs_base;
    uint64_t gs_base;
    uint16_t fs_selector;
    uint16_t gs_selector;
    int      fpu_used;

    void*    kthread_data;  /* 内核线程私有数据 */
    void*    cred;          /* 进程凭证 */
    cfs_task_t cfs_task;    /* CFS 调度实体 */
    int      nice;          /* nice 值 (-20..19) */
} process_t;

typedef struct {
    uint64_t total_switches;
    uint64_t idle_switches;
    uint64_t stolen_time_ms;
} scheduler_stats_t;

void process_init(void);
uint64_t process_create(const char* name, void* entry, uint64_t priority);
void process_switch(process_context_t* old, process_context_t* new);
void process_yield(void);
void process_start(void);
void process_exit(uint64_t code);

uint64_t process_create_ex(const char* name, void* entry, void* arg,
                           uint64_t priority, uint32_t flags, uint64_t parent);
int      process_kill(uint64_t pid, int signal);
int      process_wait(uint64_t pid, int* status, uint64_t timeout_ms);
void     process_sleep(uint64_t ms);
void     process_wakeup(uint64_t pid);
uint64_t process_current_id(void);
process_t* process_get(uint64_t pid);
uint64_t process_find_by_name(const char* name);
int      process_set_priority(uint64_t pid, uint64_t priority);
void     process_dump(const char* prefix);
scheduler_stats_t* scheduler_get_stats(void);

void scheduler_tick(void);

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
#define SIGSYS     31
#define SIGRTMIN   32
#define SIGRTMAX   63

#define RLIMIT_CPU        0
#define RLIMIT_FSIZE      1
#define RLIMIT_DATA       2
#define RLIMIT_STACK      3
#define RLIMIT_CORE       4
#define RLIMIT_RSS        5
#define RLIMIT_NPROC      6
#define RLIMIT_NOFILE     7
#define RLIMIT_MEMLOCK    8
#define RLIMIT_AS         9
#define RLIMIT_LOCKS      10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE   12
#define RLIMIT_NICE       13
#define RLIMIT_RTPRIO     14
#define RLIMIT_RTTIME     15
#define RLIMIT_NLIMITS    16

#define RLIM_INFINITY     (~0ULL)

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} rlimit_t;

int process_signal_send(uint64_t pid, int sig);
int process_signal_mask(uint64_t how, uint64_t set, uint64_t* oldset);
int process_setrlimit(int resource, const rlimit_t* rlim);
int process_getrlimit(int resource, rlimit_t* rlim);
uint64_t process_get_uid(uint64_t pid);
uint64_t process_get_gid(uint64_t pid);
int process_set_uid(uint64_t pid, uint64_t uid);
int process_set_gid(uint64_t pid, uint64_t gid);
int process_fork(void);
int process_execve(const char* path, char* const argv[], char* const envp[]);

#endif