#include <arch/cfs.h>
#include <arch/process.h>
#include <arch/spinlock.h>
#include <string.h>
#include <arch/time.h>

#define EEVDF_TICK_NS        1000000ULL
#define EEVDF_BASE_SLICE_NS  (4 * EEVDF_TICK_NS)
#define EEVDF_MIN_SLICE_NS   (1 * EEVDF_TICK_NS)
#define EEVDF_WAKEUP_CREDIT  EEVDF_BASE_SLICE_NS
#define EEVDF_SLEEPER_CREDIT (EEVDF_BASE_SLICE_NS * 2)
#define EEVDF_DEFAULT_WEIGHT 1024ULL

static const uint64_t nice_to_weight[40] = {
      88761, 71755, 56483, 46273, 36291,
      29154, 23254, 18705, 14949, 11916,
      9548,  7620,  6100,  4904,  3906,
      3121,  2501,  1991,  1586,  1277,
      1024,  820,   655,   526,   423,
      335,   272,   215,   172,   137,
      110,   87,    70,    56,    45,
      36,    29,    23,    18,    15
};

typedef struct scheduler_stats {
    uint64_t context_switches;
    uint64_t schedule_calls;
    uint64_t idle_time_ns;
    uint64_t total_run_time_ns;
    uint64_t avg_latency_ns;
    uint64_t max_latency_ns;
    uint64_t migrations;
    uint64_t wakeups;
    uint64_t preemptions;
    uint64_t load_balance_count;
    int current_cpu_load;
    double cpu_utilization;
} scheduler_stats_t;

static scheduler_stats_t sched_stats;
static spinlock_t sched_lock;
static cfs_rq_t* runqueues[MAX_CPUS];
static int num_cpus = 1;
static int initialized = 0;

uint64_t sysctl_sched_min_granularity = 750000ULL;
uint64_t sysctl_sched_latency = 6000000ULL;
uint64_t sysctl_sched_max_granularity = 3000000ULL;
uint64_t sysctl_sched_wakeup_granularity = 1000000ULL;
uint64_t sysctl_sched_migration_cost = 500000ULL;
uint64_t sysctl_sched_nr_migrate = 32;

void scheduler_init(int cpu_count)
{
    if (initialized) return;

    spin_init(&sched_lock);
    memset(&sched_stats, 0, sizeof(scheduler_stats_t));

    if (cpu_count > MAX_CPUS) cpu_count = MAX_CPUS;
    num_cpus = cpu_count;

    for (int i = 0; i < num_cpus; i++) {
        runqueues[i] = kzalloc(sizeof(cfs_rq_t));
        if (runqueues[i]) {
            rbtree_init(&runqueues[i]->tasks_timeline);
            runqueues[i]->min_vruntime = 0;
            runqueues[i]->nr_running = 0;
            runqueues[i]->load.weight = 0;
            runqueues[i]->nr_queued = 0;
            spin_init(&runqueues[i]->lock);
            runqueues[i]->cpu_id = i;
        }
    }

    initialized = 1;
}

void scheduler_destroy(void)
{
    if (!initialized) return;

    for (int i = 0; i < num_cpus; i++) {
        if (runqueues[i]) {
            kfree(runqueues[i]);
            runqueues[i] = NULL;
        }
    }

    initialized = 0;
}

uint64_t calc_delta_fair(uint64_t delta, uint64_t weight)
{
    if (weight == EEVDF_DEFAULT_WEIGHT || weight == 0)
        return delta;

    return (uint64_t)((double)delta * EEVDF_DEFAULT_WEIGHT / (double)weight);
}

uint64_t calc_delta_vruntime(uint64_t delta_exec, cfs_task_t* task)
{
    if (!task) return delta_exec;

    return calc_delta_fair(delta_exec, task->weight);
}

void update_curr(cfs_rq_t* rq, cfs_task_t* curr)
{
    if (!rq || !curr) return;

    uint64_t now = get_current_time_ns();
    uint64_t delta_exec = now - curr->exec_start;

    if (delta_exec == 0) return;

    curr->sum_exec_runtime += delta_exec;
    curr->exec_start = now;

    curr->vruntime += calc_delta_vruntime(delta_exec, curr);

    if (curr->vruntime < rq->min_vruntime) {
        rq->min_vruntime = curr->vruntime;
    }

    sched_stats.total_run_time_ns += delta_exec;
}

void enqueue_entity(cfs_rq_t* rq, cfs_task_t* task, bool wakeup)
{
    if (!rq || !task) return;

    task->on_rq = 1;

    if (wakeup) {
        uint64_t vlag = rq->min_vruntime - task->vruntime;
        if (vlag > EEVDF_WAKEUP_CREDIT) {
            task->vruntime = rq->min_vruntime - EEVDF_WAKEUP_CREDIT;
        } else if (task->flags & TASK_FLAG_SLEEPER) {
            uint64_t sleeper_credit = EEVDF_SLEEPER_CREDIT;
            if (task->vruntime > sleeper_credit) {
                task->vruntime -= sleeper_credit;
            } else {
                task->vruntime = 0;
            }
        }
    }

    if (rq->nr_running == 0) {
        task->vruntime = rq->min_vruntime;
    } else if (task->vruntime < rq->min_vruntime) {
        task->vruntime = rq->min_vruntime;
    }

    rbtree_insert(&rq->tasks_timeline, &task->rb_node, task->vruntime);
    rq->nr_running++;
    rq->load.weight += task->weight;
    rq->nr_queued++;

    sched_stats.wakeups++;
}

void dequeue_entity(cfs_rq_t* rq, cfs_task_t* task)
{
    if (!rq || !task) return;

    task->on_rq = 0;

    rbtree_delete(&rq->tasks_timeline, &task->rb_node);
    rq->nr_running--;
    rq->load.weight -= task->weight;
    rq->nr_queued--;

    if (rq->nr_running > 0) {
        cfs_task_t* leftmost = pick_next_entity(rq);
        if (leftmost && leftmost->vruntime > rq->min_vruntime) {
            rq->min_vruntime = leftmost->vruntime;
        }
    }
}

cfs_task_t* pick_next_entity(cfs_rq_t* rq)
{
    if (!rq || rq->nr_running == 0) return NULL;

    rb_node_t* left = rbtree_leftmost(&rq->tasks_timeline);
    if (!left || left == &rq->tasks_timeline.nil) return NULL;

    return container_of(left, cfs_task_t, rb_node);
}

cfs_task_t* pick_next_task(int cpu)
{
    if (cpu >= num_cpus || !runqueues[cpu]) return NULL;

    cfs_rq_t* rq = runqueues[cpu];
    spin_lock(&rq->lock);

    cfs_task_t* next = pick_next_entity(rq);

    if (next) {
        dequeue_entity(rq, next);
        next->state = CFS_TASK_RUNNING;
        next->last_schedule_time = get_current_time_ns();
        sched_stats.schedule_calls++;

        uint64_t now = get_current_time_ns();
        if (next->last_wakeup_time > 0) {
            uint64_t latency = now - next->last_wakeup_time;
            sched_stats.avg_latency_ns =
                (sched_stats.avg_latency_ns + latency) / 2;
            if (latency > sched_stats.max_latency_ns) {
                sched_stats.max_latency_ns = latency;
            }
        }
    } else {
        sched_stats.idle_time_ns += EEVDF_TICK_NS;
    }

    spin_unlock(&rq->lock);
    return next;
}

void put_prev_task(cfs_rq_t* rq, cfs_task_t* prev)
{
    if (!rq || !prev) return;

    spin_lock(&rq->lock);

    update_curr(rq, prev);
    enqueue_entity(rq, prev, false);

    spin_unlock(&rq->lock);
}

void check_preempt_tick(cfs_rq_t* rq, cfs_task_t* curr)
{
    if (!rq || !curr) return;

    uint64_t ideal_runtime = sched_slice(rq, curr);
    uint64_t delta_exec = get_current_time_ns() - curr->exec_start;

    if (delta_exec >= ideal_runtime) {
        resched_task(curr);
    }
}

uint64_t sched_slice(cfs_rq_t* rq, cfs_task_t* task)
{
    if (!rq || !task) return EEVDF_BASE_SLICE_NS;

    if (rq->nr_running == 1) {
        return sysctl_sched_min_granularity;
    }

    uint64_t slice = sysctl_sched_latency;
    slice *= task->weight;
    slice /= rq->load.weight;

    if (slice < sysctl_sched_min_granularity) {
        slice = sysctl_sched_min_granularity;
    }
    if (slice > sysctl_sched_max_granularity) {
        slice = sysctl_sched_max_granularity;
    }

    return slice;
}

void resched_task(cfs_task_t* task)
{
    if (!task) return;

    task->need_resched = 1;
    sched_stats.preemptions++;
}

int try_to_wake_up(cfs_task_t* task, int state, int wake_flags)
{
    if (!task) return -EINVAL;

    if (!(task->state & state)) return 0;

    int target_cpu = select_task_rq(task);
    if (target_cpu < 0) target_cpu = 0;
    if (target_cpu >= num_cpus) target_cpu = num_cpus - 1;

    cfs_rq_t* rq = runqueues[target_cpu];
    if (!rq) return -ENOMEM;

    spin_lock(&rq->lock);

    task->state = CFS_TASK_RUNNING;
    task->last_wakeup_time = get_current_time_ns();
    task->wake_flags = wake_flags;

    enqueue_entity(rq, task, true);

    spin_unlock(&rq->lock);

    if (target_cpu != smp_processor_id()) {
        send_reschedule_ipi(target_cpu);
        sched_stats.migrations++;
    }

    return 0;
}

int select_task_rq(cfs_task_t* task)
{
    if (!task) return 0;

    int best_cpu = 0;
    uint64_t min_load = UINT64_MAX;

    for (int i = 0; i < num_cpus; i++) {
        if (!runqueues[i]) continue;

        cfs_rq_t* rq = runqueues[i];
        uint64_t load = rq->load.weight;

        if (task->preferred_cpu == i) {
            load /= 2;
        }

        if (load < min_load) {
            min_load = load;
            best_cpu = i;
        }
    }

    return best_cpu;
}

void set_task_nice(cfs_task_t* task, int nice)
{
    if (!task) return;

    if (nice < CFS_NICE_MIN) nice = CFS_NICE_MIN;
    if (nice > CFS_NICE_MAX) nice = CFS_NICE_MAX;

    uint64_t old_weight = task->weight;
    task->nice = nice;
    task->weight = cfs_nice_to_weight(nice);

    if (task->on_rq && task->rq) {
        cfs_rq_t* rq = task->rq;
        spin_lock(&rq->lock);

        rq->load.weight += task->weight - old_weight;

        uint64_t delta = task->vruntime;
        if (old_weight) {
            delta = (delta * task->weight) / old_weight;
        }
        task->vruntime = delta;

        spin_unlock(&rq->lock);
    }
}

int get_task_nice(cfs_task_t* task)
{
    return task ? task->nice : 0;
}

void normalize_task_vruntime(cfs_task_t* task, uint64_t base_vruntime)
{
    if (!task) return;

    task->vruntime = base_vruntime;
}

void yield_task(void)
{
    cfs_task_t* curr = get_current_cfs_task();
    if (curr) {
        resched_task(curr);
        curr->state = CFS_TASK_YIELDING;
    }
}

void scheduler_tick(void)
{
    int cpu = smp_processor_id();
    if (cpu >= num_cpus || !runqueues[cpu]) return;

    cfs_rq_t* rq = runqueues[cpu];
    cfs_task_t* curr = rq->curr;

    if (curr) {
        update_curr(rq, curr);
        check_preempt_tick(rq, curr);
    }
}

void migrate_task(cfs_task_t* task, int dest_cpu)
{
    if (!task || dest_cpu < 0 || dest_cpu >= num_cpus) return;

    int src_cpu = task->cpu_id;
    if (src_cpu == dest_cpu) return;

    cfs_rq_t* src_rq = runqueues[src_cpu];
    cfs_rq_t* dst_rq = runqueues[dest_cpu];

    if (!src_rq || !dst_rq) return;

    spin_lock(&src_rq->lock);
    if (task->on_rq) {
        dequeue_entity(src_rq, task);
    }
    spin_unlock(&src_rq->lock);

    task->cpu_id = dest_cpu;
    task->rq = dst_rq;

    spin_lock(&dst_rq->lock);
    if (task->state == CFS_TASK_RUNNING ||
        task->state == CFS_TASK_YIELDING) {
        enqueue_entity(dst_rq, task, false);
    }
    spin_unlock(&dst_rq->lock);

    sched_stats.migrations++;
}

void balance_load(void)
{
    if (num_cpus <= 1) return;

    uint64_t max_load = 0, min_load = UINT64_MAX;
    int busiest_cpu = 0, idlest_cpu = 0;

    for (int i = 0; i < num_cpus; i++) {
        if (!runqueues[i]) continue;

        uint64_t load = runqueues[i]->load.weight;
        if (load > max_load) {
            max_load = load;
            busiest_cpu = i;
        }
        if (load < min_load) {
            min_load = load;
            idlest_cpu = i;
        }
    }

    if (max_load == 0 || busiest_cpu == idlest_cpu) return;

    if (max_load > min_load * 2) {
        cfs_rq_t* src_rq = runqueues[busiest_cpu];
        cfs_task_t* victim = pick_next_entity(src_rq);

        if (victim && victim->weight <= (max_load - min_load)) {
            migrate_task(victim, idlest_cpu);
            sched_stats.load_balance_count++;
        }
    }
}

void scheduler_idle(void)
{
    int cpu = smp_processor_id();

    while (true) {
        cfs_task_t* next = pick_next_task(cpu);
        if (next) {
            switch_to(next);
            break;
        }

        asm volatile("hlt");
        sched_stats.idle_time_ns += EEVDF_TICK_NS;
    }
}

int scheduler_get_stats(scheduler_stats_t* stats)
{
    if (!stats || !initialized) return -1;

    spin_lock(&sched_lock);
    memcpy(stats, &sched_stats, sizeof(scheduler_stats_t));

    if (sched_stats.total_run_time_ns > 0) {
        stats->cpu_utilization =
            (double)(sched_stats.total_run_time_ns - sched_stats.idle_time_ns) /
            (double)sched_stats.total_run_time_ns * 100.0;
    } else {
        stats->cpu_utilization = 0.0;
    }

    stats->current_cpu_load = 0;
    int cpu = smp_processor_id();
    if (cpu < num_cpus && runqueues[cpu]) {
        stats->current_cpu_load = runqueues[cpu]->nr_running;
    }

    spin_unlock(&sched_lock);
    return 0;
}

void scheduler_reset_stats(void)
{
    spin_lock(&sched_lock);
    memset(&sched_stats, 0, sizeof(scheduler_stats_t));
    spin_unlock(&sched_lock);
}

void scheduler_dump_info(void)
{
    scheduler_stats_t stats;
    scheduler_get_stats(&stats);

    printk("Scheduler Statistics:\n");
    printk("  Context switches:     %llu\n", stats.context_switches);
    printk("  Schedule calls:       %llu\n", stats.schedule_calls);
    printk("  Total run time:       %llu ms\n",
           stats.total_run_time_ns / 1000000ULL);
    printk("  Idle time:            %llu ms\n",
           stats.idle_time_ns / 1000000ULL);
    printk("  Avg latency:          %llu ns\n", stats.avg_latency_ns);
    printk("  Max latency:          %llu ns\n", stats.max_latency_ns);
    printk("  Migrations:           %llu\n", stats.migrations);
    printk("  Wakeups:              %llu\n", stats.wakeups);
    printk("  Preemptions:          %llu\n", stats.preemptions);
    printk("  Load balances:       %llu\n", stats.load_balance_count);
    printk("  CPU utilization:      %.1f%%\n", stats.cpu_utilization);
    printk("  Current queue depth:  %d\n", stats.current_cpu_load);

    printk("\nPer-CPU Run Queues:\n");
    for (int i = 0; i < num_cpus; i++) {
        if (runqueues[i]) {
            cfs_rq_t* rq = runqueues[i];
            printk("  CPU %d: running=%d queued=%d load=%lu vruntime=%llu\n",
                   i, rq->nr_running, rq->nr_queued,
                   rq->load.weight, rq->min_vruntime);
        }
    }
}