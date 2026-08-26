

#include "rcu.h"
#include <string.h>
#include <slab.h>

rcu_state_t rcu_global;

void rcu_init(void)
{
    memset(&rcu_global, 0, sizeof(rcu_state_t));
    spin_init(&rcu_global.lock);
    rcu_global.gp_seq = 1;
    rcu_global.gp_start_seq = 0;
    rcu_global.gp_active = 0;
    rcu_global.passed_gp = 0;

    for (int i = 0; i < RCU_GP_STAGES; i++) {
        rcu_global.cbs[i].head = NULL;
        rcu_global.cbs[i].tail = NULL;
        rcu_global.cbs[i].count = 0;
        spin_init(&rcu_global.cbs[i].lock);
    }

    rcu_global.percpu[0].nesting = 0;
    rcu_global.percpu[0].seq = 0;
    rcu_global.percpu[0].gp_seq = rcu_global.gp_seq;
    rcu_global.percpu[0].last_seen_gp = 0;
}

static void __rcu_add_cb(int stage, rcu_head_t* head)
{
    spin_lock(&rcu_global.cbs[stage].lock);

    head->next = NULL;
    if (rcu_global.cbs[stage].tail) {
        rcu_global.cbs[stage].tail->next = head;
    } else {
        rcu_global.cbs[stage].head = head;
    }
    rcu_global.cbs[stage].tail = head;
    rcu_global.cbs[stage].count++;

    spin_unlock(&rcu_global.cbs[stage].lock);
}

static void __rcu_invoke_cbs(int stage)
{
    spin_lock(&rcu_global.cbs[stage].lock);

    rcu_head_t* head = rcu_global.cbs[stage].head;
    rcu_global.cbs[stage].head = NULL;
    rcu_global.cbs[stage].tail = NULL;
    rcu_global.cbs[stage].count = 0;

    spin_unlock(&rcu_global.cbs[stage].lock);

    while (head) {
        rcu_head_t* next = head->next;
        if (head->func) {
            head->func(head->arg);
        }
        head = next;
    }
}

static int __rcu_all_readers_exited(void)
{
    int ncpus = (int)(sizeof(rcu_global.percpu) / sizeof(rcu_global.percpu[0]));
    for (int i = 0; i < ncpus; i++) {
        if (rcu_global.percpu[i].nesting > 0) {
            return 0;
        }
    }
    return 1;
}

void rcu_check_gp(void)
{
    spin_lock(&rcu_global.lock);

    if (!rcu_global.gp_active) {

        if (rcu_global.cbs[0].count > 0) {

            rcu_global.gp_active = 1;
            rcu_global.gp_start_seq = rcu_global.gp_seq;
        }
    }

    if (rcu_global.gp_active) {

        if (__rcu_all_readers_exited()) {

            rcu_global.gp_seq++;
            rcu_global.gp_active = 0;
            rcu_global.passed_gp++;

            __rcu_invoke_cbs(0);

            spin_lock(&rcu_global.cbs[1].lock);
            rcu_global.cbs[0].head = rcu_global.cbs[1].head;
            rcu_global.cbs[0].tail = rcu_global.cbs[1].tail;
            rcu_global.cbs[0].count = rcu_global.cbs[1].count;
            rcu_global.cbs[1].head = NULL;
            rcu_global.cbs[1].tail = NULL;
            rcu_global.cbs[1].count = 0;
            spin_unlock(&rcu_global.cbs[1].lock);
        }
    }

    spin_unlock(&rcu_global.lock);
}

void call_rcu(rcu_head_t* head, rcu_callback_t func, void* arg)
{
    if (!head || !func) return;

    head->func = func;
    head->arg = arg;
    head->next = NULL;

    __rcu_add_cb(1, head);

    rcu_check_gp();
}

void rcu_barrier(void)
{

    while (rcu_global.cbs[0].count > 0 || rcu_global.cbs[1].count > 0) {
        rcu_check_gp();

        extern void process_yield(void);
        process_yield();
    }
}

/* synchronize_rcu: 等待所有读者退出 */
static void __rcu_wake_cb(void* arg)
{
    /* 简单回调，仅用于等待完成 */
    volatile int* done = (volatile int*)arg;
    *done = 1;
    __sync_synchronize();
}

void synchronize_rcu(void)
{
    volatile int done = 0;
    rcu_head_t head;
    rcu_init_head(&head, __rcu_wake_cb, (void*)&done);
    call_rcu(&head, __rcu_wake_cb, (void*)&done);
    while (!done) {
        rcu_check_gp();
        extern void process_yield(void);
        process_yield();
    }
}

int rcu_gp_in_progress(void)
{
    return rcu_global.gp_active;
}

void srcu_init(srcu_struct_t* sp)
{
    spin_init(&sp->lock);
    sp->idx = 0;
    sp->readers[0] = sp->readers[1] = 0;
}

void srcu_read_lock(srcu_struct_t* sp)
{
    int idx;

    spin_lock(&sp->lock);
    idx = sp->idx;
    sp->readers[idx]++;
    spin_unlock(&sp->lock);

    extern int __srcu_read_idx;
    __srcu_read_idx = idx;
}

void srcu_read_unlock(srcu_struct_t* sp)
{
    extern int __srcu_read_idx;
    int idx = __srcu_read_idx;

    spin_lock(&sp->lock);
    if (sp->readers[idx] > 0) sp->readers[idx]--;
    spin_unlock(&sp->lock);
}

void srcu_synchronize(srcu_struct_t* sp)
{
    int idx;

    spin_lock(&sp->lock);
    idx = sp->idx;

    sp->idx = idx ^ 1;
    spin_unlock(&sp->lock);

    while (sp->readers[idx] > 0) {

        extern void process_yield(void);
        process_yield();
    }
}
