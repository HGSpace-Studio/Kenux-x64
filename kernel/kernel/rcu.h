

#ifndef KERNEL_RCU_H
#define KERNEL_RCU_H

#include <arch/types.h>
#include <arch/spinlock.h>

typedef void (*rcu_callback_t)(void* arg);

#define RCU_NEST_DEPTH_MAX  16

typedef struct rcu_data {
    uint64_t nesting;
    uint64_t seq;
    uint64_t gp_seq;
    uint64_t last_seen_gp;
} rcu_data_t;

typedef struct rcu_head {
    struct rcu_head* next;
    rcu_callback_t   func;
    void*            arg;
} rcu_head_t;

typedef struct rcu_cblist {
    rcu_head_t* head;
    rcu_head_t* tail;
    uint64_t    count;
    spinlock_t  lock;
} rcu_cblist_t;

#define RCU_GP_STAGES  2

typedef struct rcu_state {
    volatile uint64_t gp_seq;
    volatile uint64_t gp_start_seq;
    volatile int     gp_active;
    rcu_cblist_t      cbs[RCU_GP_STAGES];
    uint64_t          passed_gp;
    spinlock_t        lock;
    rcu_data_t        percpu[1];
} rcu_state_t;

static inline void rcu_read_lock(void)
{

    extern rcu_state_t rcu_global;
    uint64_t cpu = 0;
    rcu_global.percpu[cpu].nesting++;
    __sync_synchronize();
}

static inline void rcu_read_unlock(void)
{
    extern rcu_state_t rcu_global;
    uint64_t cpu = 0;
    __sync_synchronize();
    rcu_global.percpu[cpu].nesting--;
}

void rcu_init(void);

void call_rcu(rcu_head_t* head, rcu_callback_t func, void* arg);

void rcu_barrier(void);

void synchronize_rcu(void);

static inline void rcu_init_head(rcu_head_t* head, rcu_callback_t func, void* arg)
{
    head->func = func;
    head->arg = arg;
    head->next = NULL;
}

#define rcu_assign_pointer(p, new_p) \
    do { \
        __sync_synchronize(); \
        (p) = (new_p); \
    } while (0)

#define rcu_dereference(p) \
    ({ __sync_synchronize(); (typeof(p))(p); })

int rcu_gp_in_progress(void);

void rcu_check_gp(void);

typedef struct {
    spinlock_t lock;
    int        idx;
    uint64_t   readers[2];
} srcu_struct_t;

void srcu_init(srcu_struct_t* sp);
void srcu_read_lock(srcu_struct_t* sp);
void srcu_read_unlock(srcu_struct_t* sp);
void srcu_synchronize(srcu_struct_t* sp);

#endif
