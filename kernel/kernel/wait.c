

#include "wait.h"
#include <string.h>
#include <slab.h>
#include "timer.h"

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void process_yield(void);
extern void enqueue_process(uint64_t pid);

void wait_queue_init(wait_queue_t* wq)
{
    wq->head = NULL;
    wq->tail = NULL;
    spin_init(&wq->lock);
}

static void __wq_add(wait_queue_t* wq, wait_queue_node_t* node)
{
    node->next = NULL;
    node->prev = wq->tail;
    if (wq->tail) {
        wq->tail->next = node;
    } else {
        wq->head = node;
    }
    wq->tail = node;
}

static void __wq_remove(wait_queue_t* wq, wait_queue_node_t* node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        wq->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        wq->tail = node->prev;
    }
    node->next = node->prev = NULL;
}

void wait_queue_wait(wait_queue_t* wq, spinlock_t* lock)
{
    wait_queue_node_t node;
    process_t* proc = &processes[current_process];

    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&wq->lock);
    __wq_add(wq, &node);
    proc->state = PROCESS_WAITING;
    spin_unlock(&wq->lock);

    if (lock) {
        spin_unlock(lock);
    }

    process_yield();

}

void wait_queue_wait_irqlock(wait_queue_t* wq, irqlock_t* lock)
{
    wait_queue_node_t node;
    process_t* proc = &processes[current_process];

    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&wq->lock);
    __wq_add(wq, &node);
    proc->state = PROCESS_WAITING;
    spin_unlock(&wq->lock);

    if (lock) {
        irqlock_unlock(lock);
    }

    process_yield();
}

int wait_queue_wait_timeout(wait_queue_t* wq, spinlock_t* lock, uint64_t timeout_ms)
{
    wait_queue_node_t node;
    process_t* proc = &processes[current_process];
    uint64_t deadline = 0;
    int ret = 0;

    if (timeout_ms > 0) {
        extern uint64_t timer_get_jiffies(void);
        deadline = timer_get_jiffies() + timer_ms_to_jiffies(timeout_ms);
    }

    node.proc = proc;
    node.next = node.prev = NULL;

    spin_lock(&wq->lock);
    __wq_add(wq, &node);
    proc->state = PROCESS_WAITING;
    spin_unlock(&wq->lock);

    if (lock) {
        spin_unlock(lock);
    }

    while (proc->state == PROCESS_WAITING) {
        process_yield();
        if (timeout_ms > 0) {
            extern uint64_t timer_get_jiffies(void);
            if (timer_get_jiffies() >= deadline) {
                spin_lock(&wq->lock);
                if (node.next || node.prev || wq->head == &node) {
                    __wq_remove(wq, &node);
                    ret = -1;
                }
                spin_unlock(&wq->lock);
                return ret;
            }
        }
    }

    spin_lock(&wq->lock);
    if (node.next || node.prev || wq->head == &node) {
        __wq_remove(wq, &node);
    }
    spin_unlock(&wq->lock);

    return ret;
}

void wait_queue_wake_one(wait_queue_t* wq)
{
    spin_lock(&wq->lock);
    wait_queue_node_t* node = wq->head;
    while (node) {
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            __wq_remove(wq, node);
            node->proc->state = PROCESS_READY;
            enqueue_process(node->proc->id);
            break;
        }
        node = node->next;
    }
    spin_unlock(&wq->lock);
}

void wait_queue_wake_all(wait_queue_t* wq)
{
    spin_lock(&wq->lock);
    wait_queue_node_t* node = wq->head;
    while (node) {
        wait_queue_node_t* next = node->next;
        if (node->proc && node->proc->state == PROCESS_WAITING) {
            __wq_remove(wq, node);
            node->proc->state = PROCESS_READY;
            enqueue_process(node->proc->id);
        }
        node = next;
    }
    spin_unlock(&wq->lock);
}
