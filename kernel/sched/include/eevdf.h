#ifndef KERNEL_SCHED_EEVDF_H
#define KERNEL_SCHED_EEVDF_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define EEVDF_WEIGHT_0        1024
#define EEVDF_MAX_PRIO        20
#define EEVDF_MIN_GRANULARITY 750000
#define EEVDF_BASE_SLICE     3000000
#define EEVDF_LAG_PREIOD     60000000

typedef struct {
    uint64_t vruntime;
    uint64_t deadline;
    uint64_t slice;
    uint64_t weight;
    int32_t  nice;
    int32_t  prio;
    uint64_t total_runtime;
    uint64_t lag;
} eevdf_entity_t;

typedef struct eevdf_rq eevdf_rq_t;

typedef struct eevdf_task {
    int                      task_id;
    int                      cpu;
    int                      running;
    int                      on_rq;
    eevdf_entity_t           entity;
    struct eevdf_task*       rb_left;
    struct eevdf_task*       rb_right;
    struct eevdf_task*       rb_parent;
    uint8_t                  rb_color;
    eevdf_rq_t*              rq;
    spinlock_t               lock;
} eevdf_task_t;

struct eevdf_rq {
    eevdf_task_t*            rb_root;
    eevdf_task_t*            rb_leftmost;
    uint64_t                 min_vruntime;
    uint64_t                 total_weight;
    uint32_t                 nr_running;
    uint64_t                 clock;
    spinlock_t               lock;
};

typedef struct {
    eevdf_rq_t               runqueues[256];
    int                      num_cpus;
    uint64_t                 global_clock;
    spinlock_t               global_lock;
} eevdf_scheduler_t;

static inline uint64_t eevdf_calc_weight(int32_t nice)
{
    static const uint64_t prio_to_weight[40] = {
        88, 110, 138, 172, 215, 269, 336, 421, 526, 658,
        823, 1024, 1277, 1597, 1996, 2494, 3117, 3893, 4863, 6074,
        7585, 9476, 11832, 14781, 18454, 23042, 28773, 35920, 44873, 56061,
        70032, 87456, 109196, 136365, 170299, 212636, 265600, 331600, 414000, 517000
    };
    int idx = nice + 20;
    if (idx < 0) idx = 0;
    if (idx > 39) idx = 39;
    return prio_to_weight[idx];
}

static inline uint64_t eevdf_calc_slice(uint64_t weight, uint64_t total_weight)
{
    if (total_weight == 0) return EEVDF_BASE_SLICE;
    uint64_t slice = (EEVDF_BASE_SLICE * weight) / total_weight;
    if (slice < EEVDF_MIN_GRANULARITY) slice = EEVDF_MIN_GRANULARITY;
    return slice;
}

static inline uint64_t eevdf_calc_deadline(uint64_t vruntime, uint64_t slice)
{
    return vruntime + slice;
}

void     eevdf_scheduler_init(eevdf_scheduler_t* sched, int num_cpus);
void     eevdf_rq_init(eevdf_rq_t* rq);
void     eevdf_entity_init(eevdf_entity_t* entity, int32_t nice);
void     eevdf_task_init(eevdf_task_t* task, int task_id, int32_t nice);
void     eevdf_enqueue(eevdf_rq_t* rq, eevdf_task_t* task);
void     eevdf_dequeue(eevdf_rq_t* rq, eevdf_task_t* task);
eevdf_task_t* eevdf_pick_next(eevdf_rq_t* rq);
void     eevdf_update_curr(eevdf_rq_t* rq, eevdf_task_t* task, uint64_t delta);
void     eevdf_update_clock(eevdf_rq_t* rq, uint64_t now);
void     eevdf_set_nice(eevdf_task_t* task, int32_t nice);
void     eevdf_reweight(eevdf_rq_t* rq, eevdf_task_t* task, int32_t new_nice);
int      eevdf_preempt(eevdf_rq_t* rq, eevdf_task_t* curr, eevdf_task_t* next);
eevdf_task_t* eevdf_schedule(eevdf_scheduler_t* sched, int cpu);
void     eevdf_tick(eevdf_scheduler_t* sched, int cpu, uint64_t now);

#endif