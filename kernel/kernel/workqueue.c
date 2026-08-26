

#include "workqueue.h"
#include "kthread.h"
#include "timer.h"
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void process_yield(void);

workqueue_struct_t system_wq;
static workqueue_struct_t* all_wqs[16];
static uint32_t wq_count = 0;

workqueue_struct_t* create_workqueue(const char* name)
{
    workqueue_struct_t* wq = (workqueue_struct_t*)kzalloc(sizeof(workqueue_struct_t));
    if (!wq) return NULL;

    spin_init(&wq->lock);
    wq->head = wq->tail = NULL;
    wq->pending_count = 0;
    wq->should_stop = 0;

    if (name) {
        strncpy(wq->name, name, 31);
        wq->name[31] = '\0';
    } else {
        strncpy(wq->name, "wq", 31);
    }

    process_t* thread = kthread_run(wq->name, workqueue_thread, wq);
    if (thread) {
        wq->thread_pid = thread->id;
    }

    if (wq_count < 16) {
        all_wqs[wq_count++] = wq;
    }

    return wq;
}

void destroy_workqueue(workqueue_struct_t* wq)
{
    if (!wq) return;

    wq->should_stop = 1;
    process_t* thread = &processes[wq->thread_pid];
    if (thread) {
        kthread_stop(thread);
    }

    spin_lock(&wq->lock);
    work_struct_t* w = wq->head;
    while (w) {
        work_struct_t* next = w->next;
        w->pending = 0;
        w = next;
    }
    wq->head = wq->tail = NULL;
    spin_unlock(&wq->lock);

    kfree(wq);
}

int queue_work(workqueue_struct_t* wq, work_struct_t* work)
{
    if (!wq || !work || !work->func) return -1;

    spin_lock(&wq->lock);

    if (work->pending) {
        spin_unlock(&wq->lock);
        return 0;
    }

    work->next = NULL;
    work->pending = 1;

    if (wq->tail) {
        wq->tail->next = work;
    } else {
        wq->head = work;
    }
    wq->tail = work;
    wq->pending_count++;

    spin_unlock(&wq->lock);
    return 0;
}

typedef struct {
    work_struct_t* work;
    workqueue_struct_t* wq;
    timer_entry_t timer;
} delayed_work_wrapper_t;

static void delayed_work_callback(void* arg)
{
    delayed_work_wrapper_t* dw = (delayed_work_wrapper_t*)arg;
    queue_work(dw->wq, dw->work);
    kfree(dw);
}

int queue_delayed_work(workqueue_struct_t* wq, work_struct_t* work, uint64_t delay_ms)
{
    if (!wq || !work) return -1;

    if (delay_ms == 0) {
        return queue_work(wq, work);
    }

    delayed_work_wrapper_t* dw = (delayed_work_wrapper_t*)kzalloc(sizeof(delayed_work_wrapper_t));
    if (!dw) return -1;

    dw->work = work;
    dw->wq = wq;
    timer_add(&dw->timer, delay_ms, delayed_work_callback, dw);
    return 0;
}

int cancel_work(workqueue_struct_t* wq, work_struct_t* work)
{
    if (!wq || !work) return -1;

    spin_lock(&wq->lock);
    if (!work->pending) {
        spin_unlock(&wq->lock);
        return 0;
    }

    if (wq->head == work) {
        wq->head = work->next;
        if (!wq->head) wq->tail = NULL;
    } else {
        work_struct_t* prev = wq->head;
        while (prev && prev->next != work) prev = prev->next;
        if (prev) {
            prev->next = work->next;
            if (wq->tail == work) wq->tail = prev;
        }
    }
    work->next = NULL;
    work->pending = 0;
    wq->pending_count--;
    spin_unlock(&wq->lock);
    return 0;
}

void flush_workqueue(workqueue_struct_t* wq)
{
    if (!wq) return;
    while (wq->pending_count > 0) {
        process_yield();
    }
}

int workqueue_thread(void* arg)
{
    workqueue_struct_t* wq = (workqueue_struct_t*)arg;
    if (!wq) return -1;

    while (!wq->should_stop) {
        work_struct_t* work = NULL;

        spin_lock(&wq->lock);
        if (wq->head) {
            work = wq->head;
            wq->head = work->next;
            if (!wq->head) wq->tail = NULL;
            work->next = NULL;
            work->pending = 0;
            wq->pending_count--;
        }
        spin_unlock(&wq->lock);

        if (work && work->func) {
            work->func(wq, work->data);
        }

        if (!work) {

            process_yield();
        }
    }
    return 0;
}

void workqueue_init(void)
{
    create_workqueue("system_wq");
}

static tasklet_struct_t* tasklet_list[TASKLET_MAX];
static spinlock_t tasklet_lock = SPINLOCK_INIT;
static uint32_t tasklet_count = 0;

static void __tasklet_add(tasklet_struct_t* t, int hi)
{
    spin_lock(&tasklet_lock);

    if (t->state == TASKLET_STATE_SCHEDULED) {
        spin_unlock(&tasklet_lock);
        return;
    }

    t->state = TASKLET_STATE_SCHEDULED;
    t->count++;

    if (hi) {

        for (int i = tasklet_count; i > 0; i--) {
            tasklet_list[i] = tasklet_list[i - 1];
        }
        tasklet_list[0] = t;
    } else {
        if (tasklet_count < TASKLET_MAX) {
            tasklet_list[tasklet_count] = t;
        }
    }
    tasklet_count++;
    if (tasklet_count > TASKLET_MAX) tasklet_count = TASKLET_MAX;

    spin_unlock(&tasklet_lock);
}

void tasklet_schedule(tasklet_struct_t* t)
{
    if (!t || t->count) return;
    __tasklet_add(t, 0);
}

void tasklet_hi_schedule(tasklet_struct_t* t)
{
    if (!t || t->count) return;
    __tasklet_add(t, 1);
}

void tasklet_disable(tasklet_struct_t* t)
{
    if (!t) return;
    t->count++;
}

void tasklet_enable(tasklet_struct_t* t)
{
    if (!t) return;
    if (t->count > 0) t->count--;
}

void tasklet_kill(tasklet_struct_t* t)
{
    if (!t) return;
    while (t->state == TASKLET_STATE_SCHEDULED) {
        tasklet_run_pending();
    }
    t->count++;
}

void tasklet_init_subsystem(void)
{
    memset(tasklet_list, 0, sizeof(tasklet_list));
    tasklet_count = 0;
    spin_init(&tasklet_lock);
}

void tasklet_run_pending(void)
{
    spin_lock(&tasklet_lock);

    while (tasklet_count > 0) {
        tasklet_count--;
        tasklet_struct_t* t = tasklet_list[tasklet_count];
        tasklet_list[tasklet_count] = NULL;

        if (!t || t->state != TASKLET_STATE_SCHEDULED) continue;

        t->state = TASKLET_STATE_RUNNING;
        spin_unlock(&tasklet_lock);

        if (t->func) {
            t->func(t->data);
        }

        spin_lock(&tasklet_lock);
        t->state = TASKLET_STATE_IDLE;
        t->count--;
    }

    spin_unlock(&tasklet_lock);
}
