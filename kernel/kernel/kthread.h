

#ifndef KERNEL_KTHREAD_H
#define KERNEL_KTHREAD_H

#include <arch/types.h>
#include <arch/process.h>
#include <spinlock.h>

typedef int (*kthread_fn_t)(void* arg);

typedef struct kthread {
    kthread_fn_t func;
    void*        arg;
    int          should_stop;
    int          has_stopped;
    uint64_t     result;
    spinlock_t   lock;
} kthread_t;

process_t* kthread_create(const char* name, kthread_fn_t fn, void* arg);

process_t* kthread_run(const char* name, kthread_fn_t fn, void* arg);

void kthread_stop(process_t* k);

int kthread_join(process_t* k, uint64_t* ret);

static inline int kthread_should_stop(void)
{
    extern uint64_t current_process;
    extern process_t processes[PROCESS_MAX];
    process_t* p = &processes[current_process];
    kthread_t* kt = (kthread_t*)p->kthread_data;
    return kt ? kt->should_stop : 0;
}

void kthread_wake(process_t* k);
void kthread_bind(process_t* k, uint64_t cpu);
int  kthread_should_park(void);
void kthread_park(void);
void kthread_unpark(process_t* k);

void kthread_entry(void);

#endif
