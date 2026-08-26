

#ifndef KAPI_PROCESS_H
#define KAPI_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_PROC_DEAD       0
#define KAPI_PROC_READY      1
#define KAPI_PROC_RUNNING    2
#define KAPI_PROC_WAITING    3
#define KAPI_PROC_ZOMBIE     4
#define KAPI_PROC_CREATED    5
#define KAPI_PROC_START      6
#define KAPI_PROC_FUTEX      7
#define KAPI_PROC_OUT        8

#define KAPI_PRIO_IDLE       0
#define KAPI_PRIO_LOW        1
#define KAPI_PRIO_NORMAL     2
#define KAPI_PRIO_HIGH       3
#define KAPI_PRIO_REALTIME   4

#define KAPI_TASK_KERNEL_LEVEL      0
#define KAPI_TASK_IDLE_LEVEL        1
#define KAPI_TASK_APPLICATION_LEVEL 2

#define KAPI_MAX_PROCESS_NAME   32
#define KAPI_MAX_ARGS           64
#define KAPI_MAX_ENVS           128

typedef struct kapi_process* kapi_proc_t;

typedef struct kapi_thread* kapi_thread_t;

typedef struct kapi_vma* kapi_vma_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t ss, cs, ds, es;
} kapi_task_context_t;

typedef struct {
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} kapi_registers_t;

typedef enum {
    KAPI_VMA_NONE = 0,
    KAPI_VMA_READ = 1,
    KAPI_VMA_WRITE = 2,
    KAPI_VMA_EXEC = 4,
    KAPI_VMA_SHARED = 8,
    KAPI_VMA_ANONYMOUS = 16,
    KAPI_VMA_FIXED = 32,
    KAPI_VMA_GROWSDOWN = 64,
    KAPI_VMA_GROWSUP = 128,
} kapi_vma_flags_t;

struct kapi_vma {
    uint64_t start;
    uint64_t end;
    uint64_t flags;
    uint64_t offset;
    kapi_vma_t next;
    kapi_vma_t prev;
};

typedef struct {
    int       pid;
    int       ppid;
    int       state;
    int       priority;
    char      name[KAPI_MAX_PROCESS_NAME];
    uint64_t  cpu_time;
    uint64_t  mem_usage;
    uint64_t  vmem_usage;
    int       thread_count;
    int       fd_count;
    uint64_t  start_time;
    uint64_t  user_time;
    uint64_t  sys_time;
    int       exit_code;
    int       task_level;
    char      exe_path[256];
} kapi_proc_info_t;

typedef struct {
    int       tid;
    int       pid;
    int       state;
    int       priority;
    uint64_t  cpu_time;
    uint64_t  user_time;
    uint64_t  sys_time;
    kapi_task_context_t context;
} kapi_thread_info_t;

typedef struct {
    int signal_num;
    int sender_pid;
    void* data;
    size_t data_size;
    union sigval value;
} kapi_signal_info_t;

typedef void (*kapi_signal_handler_t)(int signum, kapi_signal_info_t* info);

typedef struct {
    int         pid;
    const char* name;
    void*       entry;
    void**      argv;
    int         argc;
    char**      envp;
    int         envc;
    int         prio;
    int         task_level;
    bool        vfork;
    const char* exe_path;
    uint64_t    brk_start;
    uint64_t    brk_end;
    uint64_t    mmap_start;
    uint64_t    mmap_end;
} kapi_proc_attr_t;

kapi_proc_t kapi_proc_create(const char* name, void (*entry)(void*), void* arg, int prio);

kapi_proc_t kapi_proc_create_ex(const kapi_proc_attr_t* attr);

void kapi_proc_exit(int status);

int kapi_proc_kill(kapi_proc_t proc, int force);

int kapi_proc_kill_by_pid(int pid, int force);

int kapi_proc_signal(int pid, int signum, kapi_signal_info_t* info);

kapi_proc_t kapi_proc_current(void);

int kapi_proc_get_pid(kapi_proc_t proc);

int kapi_proc_get_ppid(kapi_proc_t proc);

int kapi_proc_get_info(kapi_proc_t proc, kapi_proc_info_t* info);

const char* kapi_proc_get_name(kapi_proc_t proc);

void kapi_proc_set_name(kapi_proc_t proc, const char* name);

uint64_t kapi_proc_get_brk(kapi_proc_t proc);

int kapi_proc_set_brk(kapi_proc_t proc, uint64_t brk);

void* kapi_proc_mmap(kapi_proc_t proc, void* addr, size_t length, int prot, int flags, int fd, off_t offset);

int kapi_proc_munmap(kapi_proc_t proc, void* addr, size_t length);

kapi_vma_t kapi_proc_find_vma(kapi_proc_t proc, void* addr);

void kapi_proc_sleep(uint64_t ms);

void kapi_proc_sleep_ns(uint64_t ns);

void kapi_proc_yield(void);

void kapi_proc_yield_to(kapi_proc_t target);

int kapi_proc_wait(int pid, int* status);

int kapi_proc_waitpid(int pid, int* status, int options);

int kapi_proc_waitid(int idtype, int id, kapi_proc_info_t* info, int options);

kapi_thread_t kapi_thread_create(void (*entry)(void*), void* arg);

kapi_thread_t kapi_thread_create_ex(void (*entry)(void*), void* arg, int prio, size_t stack_size);

void kapi_thread_exit(void);

void kapi_thread_exit_code(int code);

int kapi_thread_detach(kapi_thread_t thread);

int kapi_thread_join(kapi_thread_t thread, int* retval);

int kapi_thread_cancel(kapi_thread_t thread);

int kapi_thread_set_priority(kapi_thread_t thread, int prio);

int kapi_thread_get_priority(kapi_thread_t thread);

kapi_thread_t kapi_thread_current(void);

int kapi_thread_get_tid(kapi_thread_t thread);

int kapi_thread_get_info(kapi_thread_t thread, kapi_thread_info_t* info);

void kapi_thread_set_affinity(kapi_thread_t thread, uint64_t mask);

uint64_t kapi_thread_get_affinity(kapi_thread_t thread);

int kapi_ipc_send(int pid, const void* msg, size_t size);

int kapi_ipc_send_timeout(int pid, const void* msg, size_t size, uint64_t timeout_ms);

int kapi_ipc_recv(void* msg, size_t size, uint64_t timeout_ms);

int kapi_ipc_call(int pid, const void* req, size_t req_size, void* resp, size_t resp_size, uint64_t timeout_ms);

int kapi_proc_fork(void);

int kapi_proc_execve(const char* path, char* const argv[], char* const envp[]);

int kapi_proc_daemonize(void);

int kapi_proc_setsid(void);

int kapi_proc_setpgid(int pid, int pgid);

int kapi_proc_getpgid(int pid);

int kapi_proc_setuid(int uid);

int kapi_proc_getuid(void);

int kapi_proc_setgid(int gid);

int kapi_proc_getgid(void);

int kapi_proc_chdir(const char* path);

char* kapi_proc_getcwd(char* buf, size_t size);

int kapi_proc_setenv(const char* name, const char* value, int overwrite);

char* kapi_proc_getenv(const char* name);

int kapi_proc_unsetenv(const char* name);

char** kapi_proc_environ(void);

int kapi_proc_count(void);

int kapi_proc_list(kapi_proc_info_t* infos, int count);

int kapi_thread_list(kapi_thread_info_t* infos, int count);

int kapi_proc_register_signal(int signum, kapi_signal_handler_t handler);

int kapi_proc_ignore_signal(int signum);

int kapi_proc_default_signal(int signum);

int kapi_proc_raise_signal(int signum);

int kapi_proc_send_signal_to(int pid, int signum);

int kapi_proc_mask_signal(uint64_t mask);

uint64_t kapi_proc_get_signal_mask(void);

int kapi_proc_set_rlimit(int resource, uint64_t cur, uint64_t max);

int kapi_proc_get_rlimit(int resource, uint64_t* cur, uint64_t* max);

#ifdef __cplusplus
}
#endif

#endif