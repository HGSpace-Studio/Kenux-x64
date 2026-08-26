

#ifndef KERNEL_WORKQUEUE_H
#define KERNEL_WORKQUEUE_H

#include <arch/types.h>
#include <arch/spinlock.h>

struct workqueue_struct;

typedef void (*work_func_t)(struct workqueue_struct* wq, void* data);

typedef struct work_struct {
    struct work_struct* next;
    work_func_t         func;
    void*               data;
    uint32_t            pending;
} work_struct_t;

#define WORK_INIT(name, _func, _data) { NULL, _func, _data, 0 }

#define WQ_MAX_PENDING     256

typedef struct workqueue_struct {
    spinlock_t    lock;
    work_struct_t* head;
    work_struct_t* tail;
    uint32_t      pending_count;
    char          name[32];

    uint64_t      thread_pid;
    volatile int  should_stop;
} workqueue_struct_t;

extern workqueue_struct_t system_wq;

workqueue_struct_t* create_workqueue(const char* name);
void destroy_workqueue(workqueue_struct_t* wq);

static inline void init_work(work_struct_t* work, work_func_t func, void* data)
{
    work->next = NULL;
    work->func = func;
    work->data = data;
    work->pending = 0;
}

int queue_work(workqueue_struct_t* wq, work_struct_t* work);

int queue_delayed_work(workqueue_struct_t* wq, work_struct_t* work, uint64_t delay_ms);

int cancel_work(workqueue_struct_t* wq, work_struct_t* work);

void flush_workqueue(workqueue_struct_t* wq);

int workqueue_thread(void* arg);

void workqueue_init(void);

#define TASKLET_MAX         64

typedef void (*tasklet_func_t)(void* data);

typedef struct tasklet_struct {
    tasklet_func_t func;
    void*          data;
    uint32_t       state;
    uint32_t       count;
    struct tasklet_struct* next;
} tasklet_struct_t;

#define TASKLET_STATE_IDLE      0
#define TASKLET_STATE_SCHEDULED 1
#define TASKLET_STATE_RUNNING   2

static inline void tasklet_init(tasklet_struct_t* t, tasklet_func_t func, void* data)
{
    t->func = func;
    t->data = data;
    t->state = TASKLET_STATE_IDLE;
    t->count = 0;
    t->next = NULL;
}

void tasklet_schedule(tasklet_struct_t* t);

void tasklet_hi_schedule(tasklet_struct_t* t);

void tasklet_disable(tasklet_struct_t* t);
void tasklet_enable(tasklet_struct_t* t);

void tasklet_kill(tasklet_struct_t* t);

void tasklet_init_subsystem(void);
void tasklet_run_pending(void);

#endif
