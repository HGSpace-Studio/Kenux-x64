#include "eevdf.h"
#include <string.h>

void eevdf_rq_init(eevdf_rq_t* rq)
{
    if (!rq) return;
    memset(rq, 0, sizeof(eevdf_rq_t));
    spin_init(&rq->lock);
}

void eevdf_entity_init(eevdf_entity_t* entity, int32_t nice)
{
    if (!entity) return;
    memset(entity, 0, sizeof(eevdf_entity_t));
    entity->nice = nice;
    entity->prio = nice + 20;
    entity->weight = eevdf_calc_weight(nice);
    entity->slice = EEVDF_BASE_SLICE;
    entity->vruntime = 0;
    entity->deadline = EEVDF_BASE_SLICE;
}

void eevdf_task_init(eevdf_task_t* task, int task_id, int32_t nice)
{
    if (!task) return;
    memset(task, 0, sizeof(eevdf_task_t));
    task->task_id = task_id;
    task->cpu = -1;
    task->running = 0;
    task->on_rq = 0;
    task->rb_left = NULL;
    task->rb_right = NULL;
    task->rb_parent = NULL;
    task->rb_color = 0;
    task->rq = NULL;
    spin_init(&task->lock);
    eevdf_entity_init(&task->entity, nice);
}

void eevdf_scheduler_init(eevdf_scheduler_t* sched, int num_cpus)
{
    if (!sched) return;
    memset(sched, 0, sizeof(eevdf_scheduler_t));
    spin_init(&sched->global_lock);
    sched->num_cpus = num_cpus > 256 ? 256 : num_cpus;
    for (int i = 0; i < sched->num_cpus; i++) {
        eevdf_rq_init(&sched->runqueues[i]);
    }
}

static void eevdf_rb_rotate_left(eevdf_rq_t* rq, eevdf_task_t* node)
{
    eevdf_task_t* right = node->rb_right;
    node->rb_right = right->rb_left;
    if (right->rb_left) right->rb_left->rb_parent = node;
    right->rb_parent = node->rb_parent;
    if (!node->rb_parent) rq->rb_root = right;
    else if (node == node->rb_parent->rb_left) node->rb_parent->rb_left = right;
    else node->rb_parent->rb_right = right;
    right->rb_left = node;
    node->rb_parent = right;
}

static void eevdf_rb_rotate_right(eevdf_rq_t* rq, eevdf_task_t* node)
{
    eevdf_task_t* left = node->rb_left;
    node->rb_left = left->rb_right;
    if (left->rb_right) left->rb_right->rb_parent = node;
    left->rb_parent = node->rb_parent;
    if (!node->rb_parent) rq->rb_root = left;
    else if (node == node->rb_parent->rb_right) node->rb_parent->rb_right = left;
    else node->rb_parent->rb_left = left;
    left->rb_right = node;
    node->rb_parent = left;
}

static void eevdf_rb_insert_fixup(eevdf_rq_t* rq, eevdf_task_t* node)
{
    while (node->rb_parent && node->rb_parent->rb_color == 1) {
        if (node->rb_parent == node->rb_parent->rb_parent->rb_left) {
            eevdf_task_t* uncle = node->rb_parent->rb_parent->rb_right;
            if (uncle && uncle->rb_color == 1) {
                node->rb_parent->rb_color = 0;
                uncle->rb_color = 0;
                node->rb_parent->rb_parent->rb_color = 1;
                node = node->rb_parent->rb_parent;
            } else {
                if (node == node->rb_parent->rb_right) {
                    node = node->rb_parent;
                    eevdf_rb_rotate_left(rq, node);
                }
                node->rb_parent->rb_color = 0;
                node->rb_parent->rb_parent->rb_color = 1;
                eevdf_rb_rotate_right(rq, node->rb_parent->rb_parent);
            }
        } else {
            eevdf_task_t* uncle = node->rb_parent->rb_parent->rb_left;
            if (uncle && uncle->rb_color == 1) {
                node->rb_parent->rb_color = 0;
                uncle->rb_color = 0;
                node->rb_parent->rb_parent->rb_color = 1;
                node = node->rb_parent->rb_parent;
            } else {
                if (node == node->rb_parent->rb_left) {
                    node = node->rb_parent;
                    eevdf_rb_rotate_right(rq, node);
                }
                node->rb_parent->rb_color = 0;
                node->rb_parent->rb_parent->rb_color = 1;
                eevdf_rb_rotate_left(rq, node->rb_parent->rb_parent);
            }
        }
    }
    rq->rb_root->rb_color = 0;
}

static void eevdf_rb_insert(eevdf_rq_t* rq, eevdf_task_t* task)
{
    eevdf_task_t* parent = NULL;
    eevdf_task_t** p = &rq->rb_root;
    int is_leftmost = 1;

    while (*p) {
        parent = *p;
        if (task->entity.deadline < parent->entity.deadline) {
            p = &parent->rb_left;
        } else if (task->entity.deadline > parent->entity.deadline) {
            p = &parent->rb_right;
            is_leftmost = 0;
        } else {
            if (task->entity.vruntime < parent->entity.vruntime) {
                p = &parent->rb_left;
            } else {
                p = &parent->rb_right;
                is_leftmost = 0;
            }
        }
    }

    task->rb_parent = parent;
    task->rb_left = NULL;
    task->rb_right = NULL;
    task->rb_color = 1;
    *p = task;

    if (is_leftmost || !rq->rb_leftmost ||
        task->entity.deadline < rq->rb_leftmost->entity.deadline) {
        rq->rb_leftmost = task;
    }

    if (parent) eevdf_rb_insert_fixup(rq, task);
}

static void eevdf_rb_update_leftmost(eevdf_rq_t* rq)
{
    rq->rb_leftmost = rq->rb_root;
    if (!rq->rb_leftmost) return;
    while (rq->rb_leftmost->rb_left) {
        rq->rb_leftmost = rq->rb_leftmost->rb_left;
    }
}

static void eevdf_rb_transplant(eevdf_rq_t* rq, eevdf_task_t* u, eevdf_task_t* v)
{
    if (!u->rb_parent) rq->rb_root = v;
    else if (u == u->rb_parent->rb_left) u->rb_parent->rb_left = v;
    else u->rb_parent->rb_right = v;
    if (v) v->rb_parent = u->rb_parent;
}

static eevdf_task_t* eevdf_rb_min(eevdf_task_t* node)
{
    if (!node) return NULL;
    while (node->rb_left) node = node->rb_left;
    return node;
}

static void eevdf_rb_delete_fixup(eevdf_rq_t* rq, eevdf_task_t* node)
{
    while (node != rq->rb_root && (!node || node->rb_color == 0)) {
        if (node == node->rb_parent->rb_left) {
            eevdf_task_t* sibling = node->rb_parent->rb_right;
            if (sibling && sibling->rb_color == 1) {
                sibling->rb_color = 0;
                node->rb_parent->rb_color = 1;
                eevdf_rb_rotate_left(rq, node->rb_parent);
                sibling = node->rb_parent->rb_right;
            }
            if ((!sibling->rb_left || sibling->rb_left->rb_color == 0) &&
                (!sibling->rb_right || sibling->rb_right->rb_color == 0)) {
                sibling->rb_color = 1;
                node = node->rb_parent;
            } else {
                if (!sibling->rb_right || sibling->rb_right->rb_color == 0) {
                    if (sibling->rb_left) sibling->rb_left->rb_color = 0;
                    sibling->rb_color = 1;
                    eevdf_rb_rotate_right(rq, sibling);
                    sibling = node->rb_parent->rb_right;
                }
                sibling->rb_color = node->rb_parent->rb_color;
                node->rb_parent->rb_color = 0;
                if (sibling->rb_right) sibling->rb_right->rb_color = 0;
                eevdf_rb_rotate_left(rq, node->rb_parent);
                node = rq->rb_root;
            }
        } else {
            eevdf_task_t* sibling = node->rb_parent->rb_left;
            if (sibling && sibling->rb_color == 1) {
                sibling->rb_color = 0;
                node->rb_parent->rb_color = 1;
                eevdf_rb_rotate_right(rq, node->rb_parent);
                sibling = node->rb_parent->rb_left;
            }
            if ((!sibling->rb_right || sibling->rb_right->rb_color == 0) &&
                (!sibling->rb_left || sibling->rb_left->rb_color == 0)) {
                sibling->rb_color = 1;
                node = node->rb_parent;
            } else {
                if (!sibling->rb_left || sibling->rb_left->rb_color == 0) {
                    if (sibling->rb_right) sibling->rb_right->rb_color = 0;
                    sibling->rb_color = 1;
                    eevdf_rb_rotate_left(rq, sibling);
                    sibling = node->rb_parent->rb_left;
                }
                sibling->rb_color = node->rb_parent->rb_color;
                node->rb_parent->rb_color = 0;
                if (sibling->rb_left) sibling->rb_left->rb_color = 0;
                eevdf_rb_rotate_right(rq, node->rb_parent);
                node = rq->rb_root;
            }
        }
    }
    if (node) node->rb_color = 0;
}

static void eevdf_rb_delete(eevdf_rq_t* rq, eevdf_task_t* task)
{
    eevdf_task_t* y = task;
    eevdf_task_t* x;
    uint8_t y_original_color = y->rb_color;

    if (!task->rb_left) {
        x = task->rb_right;
        eevdf_rb_transplant(rq, task, task->rb_right);
    } else if (!task->rb_right) {
        x = task->rb_left;
        eevdf_rb_transplant(rq, task, task->rb_left);
    } else {
        y = eevdf_rb_min(task->rb_right);
        y_original_color = y->rb_color;
        x = y->rb_right;
        if (y->rb_parent == task) {
            if (x) x->rb_parent = y;
        } else {
            eevdf_rb_transplant(rq, y, y->rb_right);
            y->rb_right = task->rb_right;
            y->rb_right->rb_parent = y;
        }
        eevdf_rb_transplant(rq, task, y);
        y->rb_left = task->rb_left;
        y->rb_left->rb_parent = y;
        y->rb_color = task->rb_color;
    }

    if (y_original_color == 0 && x) {
        eevdf_rb_delete_fixup(rq, x);
    }

    eevdf_rb_update_leftmost(rq);
}

void eevdf_enqueue(eevdf_rq_t* rq, eevdf_task_t* task)
{
    if (!rq || !task) return;

    spinlock_acquire(&rq->lock);

    task->entity.slice = eevdf_calc_slice(task->entity.weight, rq->total_weight);
    task->entity.deadline = eevdf_calc_deadline(task->entity.vruntime, task->entity.slice);

    eevdf_rb_insert(rq, task);
    task->on_rq = 1;
    task->rq = rq;
    rq->nr_running++;
    rq->total_weight += task->entity.weight;

    if (rq->nr_running == 1 || task->entity.vruntime < rq->min_vruntime) {
        rq->min_vruntime = task->entity.vruntime;
    }

    spinlock_release(&rq->lock);
}

void eevdf_dequeue(eevdf_rq_t* rq, eevdf_task_t* task)
{
    if (!rq || !task || !task->on_rq) return;

    spinlock_acquire(&rq->lock);

    eevdf_rb_delete(rq, task);
    task->on_rq = 0;
    task->rq = NULL;
    rq->nr_running--;
    rq->total_weight -= task->entity.weight;

    if (rq->nr_running > 0 && rq->rb_leftmost) {
        rq->min_vruntime = rq->rb_leftmost->entity.vruntime;
    }

    spinlock_release(&rq->lock);
}

eevdf_task_t* eevdf_pick_next(eevdf_rq_t* rq)
{
    if (!rq || !rq->rb_leftmost) return NULL;

    spinlock_acquire(&rq->lock);
    eevdf_task_t* next = rq->rb_leftmost;
    spinlock_release(&rq->lock);
    return next;
}

void eevdf_update_curr(eevdf_rq_t* rq, eevdf_task_t* task, uint64_t delta)
{
    if (!rq || !task) return;

    spinlock_acquire(&rq->lock);

    uint64_t scaled_delta = (delta * EEVDF_WEIGHT_0) / task->entity.weight;
    task->entity.vruntime += scaled_delta;
    task->entity.total_runtime += delta;
    task->entity.deadline = eevdf_calc_deadline(task->entity.vruntime, task->entity.slice);

    rq->clock += delta;

    spinlock_release(&rq->lock);
}

void eevdf_update_clock(eevdf_rq_t* rq, uint64_t now)
{
    if (!rq) return;
    spinlock_acquire(&rq->lock);
    rq->clock = now;
    spinlock_release(&rq->lock);
}

void eevdf_set_nice(eevdf_task_t* task, int32_t nice)
{
    if (!task || nice < -20 || nice > 19) return;
    spinlock_acquire(&task->lock);
    task->entity.nice = nice;
    task->entity.prio = nice + 20;
    task->entity.weight = eevdf_calc_weight(nice);
    spinlock_release(&task->lock);
}

void eevdf_reweight(eevdf_rq_t* rq, eevdf_task_t* task, int32_t new_nice)
{
    if (!rq || !task) return;

    spinlock_acquire(&rq->lock);
    spinlock_acquire(&task->lock);

    uint64_t old_weight = task->entity.weight;
    task->entity.nice = new_nice;
    task->entity.prio = new_nice + 20;
    task->entity.weight = eevdf_calc_weight(new_nice);

    rq->total_weight = rq->total_weight - old_weight + task->entity.weight;
    task->entity.slice = eevdf_calc_slice(task->entity.weight, rq->total_weight);
    task->entity.deadline = eevdf_calc_deadline(task->entity.vruntime, task->entity.slice);

    if (task->on_rq) {
        eevdf_rb_delete(rq, task);
        eevdf_rb_insert(rq, task);
    }

    spinlock_release(&task->lock);
    spinlock_release(&rq->lock);
}

int eevdf_preempt(eevdf_rq_t* rq, eevdf_task_t* curr, eevdf_task_t* next)
{
    if (!rq || !curr || !next) return 0;

    if (next->entity.deadline < curr->entity.deadline) return 1;

    if (curr->entity.total_runtime >= curr->entity.slice) return 1;

    return 0;
}

eevdf_task_t* eevdf_schedule(eevdf_scheduler_t* sched, int cpu)
{
    if (!sched || cpu < 0 || cpu >= sched->num_cpus) return NULL;

    eevdf_rq_t* rq = &sched->runqueues[cpu];
    return eevdf_pick_next(rq);
}

void eevdf_tick(eevdf_scheduler_t* sched, int cpu, uint64_t now)
{
    if (!sched || cpu < 0 || cpu >= sched->num_cpus) return;

    eevdf_rq_t* rq = &sched->runqueues[cpu];
    eevdf_update_clock(rq, now);

    spinlock_acquire(&sched->global_lock);
    sched->global_clock = now;
    spinlock_release(&sched->global_lock);
}