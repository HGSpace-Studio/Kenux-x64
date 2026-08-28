

#ifndef ARCH_SPINLOCK_H
#define ARCH_SPINLOCK_H

#include <arch/types.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spin_init(spinlock_t* sl)
{
    sl->lock = 0;
}

static inline void spin_lock(spinlock_t* sl)
{
    while (__sync_lock_test_and_set(&sl->lock, 1)) {
        while (sl->lock) {
            __asm__ volatile ("pause");
        }
    }
}

static inline void spin_unlock(spinlock_t* sl)
{
    __sync_lock_release(&sl->lock);
}

static inline int spin_trylock(spinlock_t* sl)
{
    return __sync_lock_test_and_set(&sl->lock, 1) == 0;
}

typedef struct {
    volatile uint32_t ticket;
    volatile uint32_t serving;
} ticketlock_t;

#define TICKETLOCK_INIT { 0, 0 }

static inline void ticket_init(ticketlock_t* tl)
{
    tl->ticket = 0;
    tl->serving = 0;
}

static inline void ticket_lock(ticketlock_t* tl)
{
    uint32_t my_ticket = __sync_fetch_and_add(&tl->ticket, 1);
    while (tl->serving != my_ticket) {
        __asm__ volatile ("pause");
    }
}

static inline void ticket_unlock(ticketlock_t* tl)
{
    __sync_add_and_fetch(&tl->serving, 1);
}

typedef struct {
    spinlock_t lock;
    uint64_t   flags;
} irqlock_t;

#define IRQLOCK_INIT { SPINLOCK_INIT, 0 }

static inline void irqlock_lock(irqlock_t* il)
{
    uint64_t f;
    __asm__ volatile (
        "pushfq\n"
        "pop %0\n"
        "cli\n"
        : "=r"(f)
    );
    il->flags = f;
    spin_lock(&il->lock);
}

static inline void irqlock_unlock(irqlock_t* il)
{
    spin_unlock(&il->lock);
    __asm__ volatile (
        "push %0\n"
        "popfq\n"
        :
        : "r"(il->flags)
        : "memory"
    );
}

typedef struct {
    volatile int32_t readers;
    volatile uint32_t writer;
    spinlock_t guard;
} rwlock_t;

#define RWLOCK_INIT { 0, 0, SPINLOCK_INIT }

static inline void rwlock_init(rwlock_t* rw)
{
    rw->readers = 0;
    rw->writer = 0;
    spin_init(&rw->guard);
}

static inline void rwlock_read_lock(rwlock_t* rw)
{
    for (;;) {
        while (rw->writer) {
            __asm__ volatile ("pause");
        }
        spin_lock(&rw->guard);
        if (!rw->writer) {
            rw->readers++;
            spin_unlock(&rw->guard);
            break;
        }
        spin_unlock(&rw->guard);
    }
}

static inline void rwlock_read_unlock(rwlock_t* rw)
{
    __sync_sub_and_fetch(&rw->readers, 1);
}

static inline void rwlock_write_lock(rwlock_t* rw)
{
    for (;;) {
        while (rw->readers || rw->writer) {
            __asm__ volatile ("pause");
        }
        spin_lock(&rw->guard);
        if (!rw->readers && !rw->writer) {
            rw->writer = 1;
            spin_unlock(&rw->guard);
            break;
        }
        spin_unlock(&rw->guard);
    }
}

static inline void rwlock_write_unlock(rwlock_t* rw)
{
    rw->writer = 0;
}

typedef struct {
    volatile uint32_t sequence;
    spinlock_t lock;
} seqlock_t;

#define SEQLOCK_INIT { 0, SPINLOCK_INIT }

static inline void seqlock_init(seqlock_t* sl)
{
    sl->sequence = 0;
    spin_init(&sl->lock);
}

static inline void seqlock_write_begin(seqlock_t* sl)
{
    spin_lock(&sl->lock);
    sl->sequence++;
    __sync_synchronize();
}

static inline void seqlock_write_end(seqlock_t* sl)
{
    __sync_synchronize();
    sl->sequence++;
    spin_unlock(&sl->lock);
}

static inline uint32_t seqlock_read_begin(const seqlock_t* sl)
{
    uint32_t seq;
    do {
        seq = sl->sequence;
        __sync_synchronize();
    } while (seq & 1);
    return seq;
}

static inline int seqlock_read_retry(const seqlock_t* sl, uint32_t seq)
{
    __sync_synchronize();
    return (seq != sl->sequence);
}

typedef struct mcs_node {
    struct mcs_node* next;
    volatile int locked;
} mcs_node_t;

typedef struct {
    mcs_node_t* tail;
} mcslock_t;

#define MCSLOCK_INIT { NULL }

static inline void mcs_init(mcslock_t* ml)
{
    ml->tail = NULL;
}

static inline void mcs_lock(mcslock_t* ml, mcs_node_t* node)
{
    node->next = NULL;
    node->locked = 1;
    __sync_synchronize();
    mcs_node_t* prev = __sync_lock_test_and_set(&ml->tail, node);
    if (!prev) {
        node->locked = 0;
        return;
    }
    prev->next = node;
    __sync_synchronize();
    while (node->locked) {
        __asm__ volatile("pause");
    }
}

static inline void mcs_unlock(mcslock_t* ml, mcs_node_t* node)
{
    __sync_synchronize();
    if (!node->next) {
        mcs_node_t* expected = node;
        if (__sync_bool_compare_and_swap(&ml->tail, expected, NULL)) {
            return;
        }
        while (!node->next) {
            __asm__ volatile("pause");
        }
    }
    node->next->locked = 0;
    __sync_synchronize();
}

typedef struct {
    volatile uint64_t counter;
    spinlock_t lock;
} rcu_t;

#define RCU_INIT { 0, SPINLOCK_INIT }

static inline void spin_rcu_init(rcu_t* rcu)
{
    rcu->counter = 0;
    spin_init(&rcu->lock);
}

static inline uint64_t spin_rcu_read_lock(rcu_t* rcu)
{
    return __sync_add_and_fetch(&rcu->counter, 1);
}

static inline void spin_rcu_read_unlock(rcu_t* rcu)
{
    __sync_sub_and_fetch(&rcu->counter, 1);
}

static inline void spin_rcu_synchronize(rcu_t* rcu)
{
    while (rcu->counter > 0) {
        __asm__ volatile("pause");
    }
}

#endif