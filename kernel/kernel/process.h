#ifndef PROCESS_H
#define PROCESS_H

#include <arch/types.h>

#define MAX_PROCESSES 128
#define MAX_THREADS 256

/* 进程状态 */
#define PROCESS_READY   0
#define PROCESS_RUNNING 1
#define PROCESS_BLOCKED 2
#define PROCESS_ZOMBIE  3
#define PROCESS_DEAD    5

typedef struct process {
    int pid;
    int ppid;
    int state;
    void* stack;
    void* cr3;
    void* kthread_data;  /* 内核线程私有数据 */
} process_t;

typedef struct thread {
    int tid;
    int pid;
    void* rip;
    void* rsp;
} thread_t;

void process_init(void);
int process_create(void* entry, void* arg);
void process_exit(int status);
int thread_create(void* entry, void* arg);
void thread_yield(void);

/* 扩展进程创建 */
uint64_t process_create_ex(const char* name, void* entry, void* arg,
                           uint64_t priority, uint64_t flags, uint64_t parent);

#endif
