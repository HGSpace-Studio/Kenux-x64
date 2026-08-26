

#ifndef KERNEL_WAIT_H
#define KERNEL_WAIT_H

#include <arch/types.h>
#include <arch/process.h>
#include <arch/spinlock.h>

typedef struct wait_queue_node {
    process_t* proc;
    struct wait_queue_node* next;
    struct wait_queue_node* prev;
} wait_queue_node_t;

typedef struct {
    wait_queue_node_t* head;
    wait_queue_node_t* tail;
    spinlock_t lock;
} wait_queue_t;

#define WAIT_QUEUE_INIT(name) { NULL, NULL, SPINLOCK_INIT }

void wait_queue_init(wait_queue_t* wq);

void wait_queue_wait(wait_queue_t* wq, spinlock_t* lock);
void wait_queue_wait_irqlock(wait_queue_t* wq, irqlock_t* lock);

void wait_queue_wake_one(wait_queue_t* wq);

void wait_queue_wake_all(wait_queue_t* wq);

int wait_queue_wait_timeout(wait_queue_t* wq, spinlock_t* lock, uint64_t timeout_ms);

#endif
