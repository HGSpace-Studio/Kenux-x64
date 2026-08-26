

#ifndef KERNEL_SYNC_H
#define KERNEL_SYNC_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "wait.h"

typedef struct {
    volatile int32_t count;
    spinlock_t lock;
    wait_queue_t waiters;
} semaphore_t;

#define SEMAPHORE_INIT(n) { (n), SPINLOCK_INIT, WAIT_QUEUE_INIT(waiters) }

void semaphore_init(semaphore_t* sem, int32_t initial);
void semaphore_down(semaphore_t* sem);
int  semaphore_down_timeout(semaphore_t* sem, uint64_t ms);
void semaphore_up(semaphore_t* sem);
int32_t semaphore_get_value(const semaphore_t* sem);

typedef struct {
    volatile uint32_t locked;
    uint64_t owner;
    uint32_t recurse_count;
    spinlock_t lock;
    wait_queue_t waiters;
} mutex_t;

#define MUTEX_INIT { 0, (uint64_t)-1, 0, SPINLOCK_INIT, WAIT_QUEUE_INIT(waiters) }

void mutex_init(mutex_t* mtx);
void mutex_lock(mutex_t* mtx);
int  mutex_trylock(mutex_t* mtx);
void mutex_unlock(mutex_t* mtx);
int  mutex_is_locked(const mutex_t* mtx);

typedef struct {
    wait_queue_t waiters;
    spinlock_t lock;
} condvar_t;

#define CONDVAR_INIT { WAIT_QUEUE_INIT(waiters), SPINLOCK_INIT }

void condvar_init(condvar_t* cv);

void condvar_wait(condvar_t* cv, mutex_t* mtx);
int  condvar_wait_timeout(condvar_t* cv, mutex_t* mtx, uint64_t ms);

void condvar_signal(condvar_t* cv);

void condvar_broadcast(condvar_t* cv);

typedef struct {
    volatile int done;
    wait_queue_t waiters;
    spinlock_t lock;
} completion_t;

#define COMPLETION_INIT { 0, WAIT_QUEUE_INIT(waiters), SPINLOCK_INIT }

void completion_init(completion_t* c);
void completion_wait(completion_t* c);
void completion_complete(completion_t* c);

#define BARRIER_MAX 64

typedef struct {
    uint32_t count;
    uint32_t waiting;
    uint32_t threshold;
    spinlock_t lock;
    wait_queue_t waiters;
} barrier_t;

void barrier_init(barrier_t* b, uint32_t threshold);
void barrier_wait(barrier_t* b);

#endif
