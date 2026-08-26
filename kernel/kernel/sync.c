

#include "sync.h"
#include <string.h>
#include <slab.h>
#include "timer.h"

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];

void semaphore_init(semaphore_t* sem, int32_t initial)
{
    sem->count = initial;
    spin_init(&sem->lock);
    wait_queue_init(&sem->waiters);
}

void semaphore_down(semaphore_t* sem)
{
    spin_lock(&sem->lock);
    sem->count--;
    if (sem->count < 0) {

        wait_queue_node_t node;
        process_t* proc = &processes[current_process];
        node.proc = proc;
        node.next = node.prev = NULL;

        spin_lock(&sem->waiters.lock);
        node.next = NULL;
        node.prev = sem->waiters.tail;
        if (sem->waiters.tail) {
            sem->waiters.tail->next = &node;
        } else {
            sem->waiters.head = &node;
        }
        sem->waiters.tail = &node;
        proc->state = PROCESS_WAITING;
        spin_unlock(&sem->waiters.lock);
        spin_unlock(&sem->lock);

        process_yield();
        return;
    }
    spin_unlock(&sem->lock);
}

int semaphore_down_timeout(semaphore_t* sem, uint64_t ms)
{
    spin_lock(&sem->lock);
    sem->count--;
    if (sem->count < 0) {
        wait_queue_node_t node;
        process_t* proc = &processes[current_process];
        node.proc = proc;
        node.next = node.prev = NULL;

        uint64_t deadline = 0;
        if (ms > 0) {
            extern uint64_t timer_get_jiffies(void);
            deadline = timer_get_jiffies() + timer_ms_to_jiffies(ms);
        }

        spin_lock(&sem->waiters.lock);
        node.next = NULL;
        node.prev = sem->waiters.tail;
        if (sem->waiters.tail) {
            sem->waiters.tail->next = &node;
        } else {
            sem->waiters.head = &node;
        }
        sem->waiters.tail = &node;
        proc->state = PROCESS_WAITING;
        spin_unlock(&sem->waiters.lock);
        spin_unlock(&sem->lock);

        while (proc->state == PROCESS_WAITING) {
            process_yield();
            if (ms > 0) {
                extern uint64_t timer_get_jiffies(void);
                if (timer_get_jiffies() >= deadline) {
                    spin_lock(&sem->waiters.lock);
                    if (node.next || node.prev || sem->waiters.head == &node) {
                        if (node.prev) node.prev->next = node.next;
                        else sem->waiters.head = node.next;
                        if (node.next) node.next->prev = node.prev;
                        else sem->waiters.tail = node.prev;
                        node.next = node.prev = NULL;
                    }
                    spin_unlock(&sem->waiters.lock);

                    spin_lock(&sem->lock);
                    sem->count++;
                    spin_unlock(&sem->lock);
                    return -1;
                }
            }
        }
        return 0;
    }
    spin_unlock(&sem->lock);
    return 0;
}

void semaphore_up(semaphore_t* sem)
{
    spin_lock(&sem->lock);
    sem->count++;
    if (sem->count <= 0) {

        spin_lock(&sem->waiters.lock);
        wait_queue_node_t* node = sem->waiters.head;
        while (node) {
            if (node->proc && node->proc->state == PROCESS_WAITING) {

                if (node->prev) node->prev->next = node->next;
                else sem->waiters.head = node->next;
                if (node->next) node->next->prev = node->prev;
                else sem->waiters.tail = node->prev;
                node->next = node->prev = NULL;

                node->proc->state = PROCESS_READY;
                extern void enqueue_process(uint64_t);
                enqueue_process(node->proc->id);
                break;
            }
            node = node->next;
        }
        spin_unlock(&sem->waiters.lock);
    }
    spin_unlock(&sem->lock);
}

int32_t semaphore_get_value(const semaphore_t* sem)
{
    return sem->count;
}

void mutex_init(mutex_t* mtx)
{
    mtx->locked = 0;
    mtx->owner = (uint64_t)-1;
    mtx->recurse_count = 0;
    spin_init(&mtx->lock);
    wait_queue_init(&mtx->waiters);
}

void mutex_lock(mutex_t* mtx)
{
    uint64_t self = processes[current_process].id;

    spin_lock(&mtx->lock);
    if (mtx->locked && mtx->owner == self) {

        mtx->recurse_count++;
        spin_unlock(&mtx->lock);
        return;
    }

    while (mtx->locked) {

        wait_queue_node_t node;
        process_t* proc = &processes[current_process];
        node.proc = proc;
        node.next = node.prev = NULL;

        spin_lock(&mtx->waiters.lock);
        node.prev = mtx->waiters.tail;
        if (mtx->waiters.tail) {
            mtx->waiters.tail->next = &node;
        } else {
            mtx->waiters.head = &node;
        }
        mtx->waiters.tail = &node;
        proc->state = PROCESS_WAITING;
        spin_unlock(&mtx->waiters.lock);
        spin_unlock(&mtx->lock);

        process_yield();

        spin_lock(&mtx->lock);
    }

    mtx->locked = 1;
    mtx->owner = self;
    mtx->recurse_count = 1;
    spin_unlock(&mtx->lock);
}

int mutex_trylock(mutex_t* mtx)
{
    uint64_t self = processes[current_process].id;
    int ret = 0;

    spin_lock(&mtx->lock);
    if (!mtx->locked) {
        mtx->locked = 1;
        mtx->owner = self;
        mtx->recurse_count = 1;
        ret = 1;
    } else if (mtx->owner == self) {
        mtx->recurse_count++;
        ret = 1;
    }
    spin_unlock(&mtx->lock);
    return ret;
}

void mutex_unlock(mutex_t* mtx)
{
    spin_lock(&mtx->lock);
    if (!mtx->locked) {
        spin_unlock(&mtx->lock);
        return;
    }

    mtx->recurse_count--;
    if (mtx->recurse_count > 0) {
        spin_unlock(&mtx->lock);
        return;
    }

    mtx->locked = 0;
    mtx->owner = (uint64_t)-1;

    spin_lock(&mtx->waiters.lock);
    wait_queue_node_t* node = mtx->waiters.head;
    while (node) {
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            if (node->prev) node->prev->next = node->next;
            else mtx->waiters.head = node->next;
            if (node->next) node->next->prev = node->prev;
            else mtx->waiters.tail = node->prev;
            node->next = node->prev = NULL;

            node->proc->state = PROCESS_READY;
            extern void enqueue_process(uint64_t);
            enqueue_process(node->proc->id);
            break;
        }
        node = node->next;
    }
    spin_unlock(&mtx->waiters.lock);
    spin_unlock(&mtx->lock);
}

int mutex_is_locked(const mutex_t* mtx)
{
    return mtx->locked;
}

void condvar_init(condvar_t* cv)
{
    wait_queue_init(&cv->waiters);
    spin_init(&cv->lock);
}

void condvar_wait(condvar_t* cv, mutex_t* mtx)
{

    wait_queue_node_t node;
    process_t* proc = &processes[current_process];
    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&cv->lock);
    node.prev = cv->waiters.tail;
    if (cv->waiters.tail) {
        cv->waiters.tail->next = &node;
    } else {
        cv->waiters.head = &node;
    }
    cv->waiters.tail = &node;
    proc->state = PROCESS_WAITING;
    spin_unlock(&cv->lock);

    mutex_unlock(mtx);
    process_yield();
    mutex_lock(mtx);
}

int condvar_wait_timeout(condvar_t* cv, mutex_t* mtx, uint64_t ms)
{
    wait_queue_node_t node;
    process_t* proc = &processes[current_process];
    node.proc = proc;
    node.next = node.prev = NULL;

    uint64_t deadline = 0;
    if (ms > 0) {
        extern uint64_t timer_get_jiffies(void);
        deadline = timer_get_jiffies() + timer_ms_to_jiffies(ms);
    }

    spin_lock(&cv->lock);
    node.prev = cv->waiters.tail;
    if (cv->waiters.tail) {
        cv->waiters.tail->next = &node;
    } else {
        cv->waiters.head = &node;
    }
    cv->waiters.tail = &node;
    proc->state = PROCESS_WAITING;
    spin_unlock(&cv->lock);

    mutex_unlock(mtx);

    while (proc->state == PROCESS_WAITING) {
        process_yield();
        if (ms > 0) {
            extern uint64_t timer_get_jiffies(void);
            if (timer_get_jiffies() >= deadline) {
                spin_lock(&cv->lock);
                if (node.next || node.prev || cv->waiters.head == &node) {
                    if (node.prev) node.prev->next = node.next;
                    else cv->waiters.head = node.next;
                    if (node.next) node.next->prev = node.prev;
                    else cv->waiters.tail = node.prev;
                    node.next = node.prev = NULL;
                }
                spin_unlock(&cv->lock);
                mutex_lock(mtx);
                return -1;
            }
        }
    }

    mutex_lock(mtx);
    return 0;
}

void condvar_signal(condvar_t* cv)
{
    spin_lock(&cv->lock);
    wait_queue_node_t* node = cv->waiters.head;
    while (node) {
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            if (node->prev) node->prev->next = node->next;
            else cv->waiters.head = node->next;
            if (node->next) node->next->prev = node->prev;
            else cv->waiters.tail = node->prev;
            node->next = node->prev = NULL;

            node->proc->state = PROCESS_READY;
            extern void enqueue_process(uint64_t);
            enqueue_process(node->proc->id);
            break;
        }
        node = node->next;
    }
    spin_unlock(&cv->lock);
}

void condvar_broadcast(condvar_t* cv)
{
    spin_lock(&cv->lock);
    wait_queue_node_t* node = cv->waiters.head;
    while (node) {
        wait_queue_node_t* next = node->next;
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            if (node->prev) node->prev->next = node->next;
            else cv->waiters.head = node->next;
            if (node->next) node->next->prev = node->prev;
            else cv->waiters.tail = node->prev;
            node->next = node->prev = NULL;

            node->proc->state = PROCESS_READY;
            extern void enqueue_process(uint64_t);
            enqueue_process(node->proc->id);
        }
        node = next;
    }
    spin_unlock(&cv->lock);
}

void completion_init(completion_t* c)
{
    c->done = 0;
    wait_queue_init(&c->waiters);
    spin_init(&c->lock);
}

void completion_wait(completion_t* c)
{
    spin_lock(&c->lock);
    if (c->done) {
        spin_unlock(&c->lock);
        return;
    }

    wait_queue_node_t node;
    process_t* proc = &processes[current_process];
    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&c->waiters.lock);
    node.prev = c->waiters.tail;
    if (c->waiters.tail) {
        c->waiters.tail->next = &node;
    } else {
        c->waiters.head = &node;
    }
    c->waiters.tail = &node;
    proc->state = PROCESS_WAITING;
    spin_unlock(&c->waiters.lock);
    spin_unlock(&c->lock);

    process_yield();
}

void completion_complete(completion_t* c)
{
    spin_lock(&c->lock);
    if (c->done) {
        spin_unlock(&c->lock);
        return;
    }
    c->done = 1;

    spin_lock(&c->waiters.lock);
    wait_queue_node_t* node = c->waiters.head;
    while (node) {
        wait_queue_node_t* next = node->next;
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            if (node->prev) node->prev->next = node->next;
            else c->waiters.head = node->next;
            if (node->next) node->next->prev = node->prev;
            else c->waiters.tail = node->prev;
            node->next = node->prev = NULL;

            node->proc->state = PROCESS_READY;
            extern void enqueue_process(uint64_t);
            enqueue_process(node->proc->id);
        }
        node = next;
    }
    spin_unlock(&c->waiters.lock);
    spin_unlock(&c->lock);
}

void barrier_init(barrier_t* b, uint32_t threshold)
{
    b->count = 0;
    b->waiting = 0;
    b->threshold = threshold;
    spin_init(&b->lock);
    wait_queue_init(&b->waiters);
}

void barrier_wait(barrier_t* b)
{
    spin_lock(&b->lock);
    b->count++;
    if (b->count >= b->threshold) {

        b->count = 0;
        spin_lock(&b->waiters.lock);
        wait_queue_node_t* node = b->waiters.head;
        while (node) {
            wait_queue_node_t* next = node->next;
            if (node->proc && node->proc->state == PROCESS_WAITING) {
                if (node->prev) node->prev->next = node->next;
                else b->waiters.head = node->next;
                if (node->next) node->next->prev = node->prev;
                else b->waiters.tail = node->prev;
                node->next = node->prev = NULL;

                node->proc->state = PROCESS_READY;
                extern void enqueue_process(uint64_t);
                enqueue_process(node->proc->id);
            }
            node = next;
        }
        b->waiters.head = b->waiters.tail = NULL;
        spin_unlock(&b->waiters.lock);
        spin_unlock(&b->lock);
        return;
    }

    wait_queue_node_t node;
    process_t* proc = &processes[current_process];
    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&b->waiters.lock);
    node.prev = b->waiters.tail;
    if (b->waiters.tail) {
        b->waiters.tail->next = &node;
    } else {
        b->waiters.head = &node;
    }
    b->waiters.tail = &node;
    proc->state = PROCESS_WAITING;
    spin_unlock(&b->waiters.lock);
    spin_unlock(&b->lock);

    process_yield();
}
