

#ifndef ARCH_CFS_H
#define ARCH_CFS_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define MAX_CPUS            256

#define CFS_TASK_RUNNING    0
#define CFS_TASK_SLEEPING   1
#define CFS_TASK_YIELDING   2
#define CFS_TASK_STOPPED    3
#define CFS_TASK_DEAD       4

#define TASK_FLAG_SLEEPER   0x01
#define TASK_FLAG_RT        0x02

typedef struct rb_node {
    struct rb_node* parent;
    struct rb_node* left;
    struct rb_node* right;
    uint64_t key;
    int color;
} rb_node_t;

typedef struct {
    rb_node_t* root;
    rb_node_t  nil;
} rb_tree_t;

typedef struct cfs_rq cfs_rq_t;

typedef struct cfs_task {
    rb_node_t rb_node;
    uint64_t vruntime;
    uint64_t exec_start;
    uint64_t sum_exec_runtime;
    uint64_t prev_sum_exec_runtime;
    uint64_t eevdf_slice;
    uint64_t deadline;
    uint64_t weight;

    int nice;
    uint64_t load_weight;

    uint32_t on_rq;
    uint32_t state;
    uint32_t flags;
    uint32_t need_resched;
    uint32_t wake_flags;

    int cpu_id;
    int preferred_cpu;
    uint64_t last_schedule_time;
    uint64_t last_wakeup_time;

    cfs_rq_t* rq;
    void* process_ref;
} cfs_task_t;

typedef struct cfs_rq {
    rb_tree_t tasks_timeline;
    cfs_task_t* curr;
    cfs_task_t* idle;

    uint64_t min_vruntime;
    uint64_t exec_clock;
    uint32_t nr_running;
    uint32_t nr_tasks;
    uint32_t nr_queued;
    int cpu_id;

    struct {
        uint64_t weight;
    } load;

    spinlock_t lock;
} cfs_rq_t;

#define CFS_NICE_MIN        (-20)
#define CFS_NICE_MAX        19
#define CFS_NICE_0_LOAD     1024
#define CFS_TARGET_LATENCY  6000000ULL
#define CFS_MIN_GRANULARITY 750000ULL
#define CFS_TICK_NSEC       1000000ULL

#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

void cfs_init(void);
void cfs_rq_init(cfs_rq_t* rq);

void cfs_task_init(cfs_task_t* task, int nice);
void cfs_enqueue_task(cfs_rq_t* rq, cfs_task_t* task);
void cfs_dequeue_task(cfs_rq_t* rq, cfs_task_t* task);

cfs_task_t* cfs_pick_next_task(cfs_rq_t* rq);
void cfs_account_exec(cfs_rq_t* rq, cfs_task_t* task, uint64_t delta_ns);
void cfs_update_min_vruntime(cfs_rq_t* rq);

void cfs_scheduler_tick(cfs_rq_t* rq);

cfs_rq_t* cfs_get_rq(void);

uint64_t cfs_nice_to_weight(int nice);

void rbtree_init(rb_tree_t* tree);
void rbtree_insert(rb_tree_t* tree, rb_node_t* node, uint64_t key);
void rbtree_delete(rb_tree_t* tree, rb_node_t* node);
rb_node_t* rbtree_leftmost(rb_tree_t* tree);
rb_node_t* rbtree_min(rb_tree_t* tree);

#endif