

#include "kthread.h"
#include <arch/memory.h>
#include <arch/spinlock.h>
#include <arch/cfs.h>
#include <string.h>
#include <slab.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void process_yield(void);

static void __kthread_wrapper(void);

void kthread_entry(void)
{
    process_t* p = &processes[current_process];
    kthread_t* kt = (kthread_t*)p->kthread_data;

    if (!kt || !kt->func) {
        p->state = PROCESS_DEAD;
        return;
    }

    int ret = kt->func(kt->arg);

    spin_lock(&kt->lock);
    kt->result = ret;
    kt->has_stopped = 1;
    spin_unlock(&kt->lock);

    p->state = PROCESS_DEAD;
    process_yield();
}

process_t* kthread_create(const char* name, kthread_fn_t fn, void* arg)
{

    kthread_t* kt = (kthread_t*)kzalloc(sizeof(kthread_t));
    if (!kt) return NULL;

    kt->func = fn;
    kt->arg = arg;
    kt->should_stop = 0;
    kt->has_stopped = 0;
    kt->result = 0;
    spin_init(&kt->lock);

    uint64_t pid = process_create_ex(name, (void*)kthread_entry, NULL,
                                      PRIORITY_NORMAL,
                                      PROCESS_FLAG_KTHREAD | PROCESS_FLAG_FIXED, 0);
    if (pid == (uint64_t)-1) {
        kfree(kt);
        return NULL;
    }

    processes[pid].kthread_data = kt;

    return &processes[pid];
}

process_t* kthread_run(const char* name, kthread_fn_t fn, void* arg)
{
    process_t* p = kthread_create(name, fn, arg);
    if (p) {
        kthread_wake(p);
    }
    return p;
}

void kthread_stop(process_t* k)
{
    if (!k || !k->kthread_data) return;

    kthread_t* kt = (kthread_t*)k->kthread_data;
    spin_lock(&kt->lock);
    kt->should_stop = 1;
    spin_unlock(&kt->lock);

    if (k->state != PROCESS_READY) {
        k->state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &k->cfs_task);
    }
}

int kthread_join(process_t* k, uint64_t* ret)
{
    if (!k || !k->kthread_data) return -1;

    kthread_t* kt = (kthread_t*)k->kthread_data;

    for (;;) {
        spin_lock(&kt->lock);
        int stopped = kt->has_stopped;
        spin_unlock(&kt->lock);
        if (stopped) break;
        process_yield();
    }

    if (ret) *ret = kt->result;

    kfree(kt);
    k->kthread_data = NULL;

    return 0;
}

void kthread_wake(process_t* k)
{
    if (!k) return;
    if (k->state != PROCESS_READY && k->state != PROCESS_RUNNING) {
        k->state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &k->cfs_task);
    }
}

void kthread_bind(process_t* k, uint64_t cpu)
{
    (void)cpu;
    if (!k) return;
    k->flags |= PROC_FLAG_FIXED;
}

int kthread_should_park(void)
{
    return 0;
}

void kthread_park(void)
{
    process_t* p = &processes[current_process];
    p->state = PROCESS_WAITING;
    cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
    process_yield();
}

void kthread_unpark(process_t* k)
{
    kthread_wake(k);
}