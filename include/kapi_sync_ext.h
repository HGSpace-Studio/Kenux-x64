#ifndef KAPI_SYNC_EXT_H
#define KAPI_SYNC_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_MUTEX_NORMAL      0
#define KAPI_MUTEX_RECURSIVE   1
#define KAPI_MUTEX_ERRORCHECK  2
#define KAPI_MUTEX_DEFAULT     KAPI_MUTEX_NORMAL

#define KAPI_RWLOCK_PREFER_READER_NONE  0
#define KAPI_RWLOCK_PREFER_WRITER_NONE  1
#define KAPI_RWLOCK_PREFER_READER       2
#define KAPI_RWLOCK_PREFER_WRITER       3

#define KAPI_SPINLOCK_NORMAL    0
#define KAPI_SPINLOCK_IRQ       1
#define KAPI_SPINLOCK_IRQSAVE   2
#define KAPI_SPINLOCK_BH        3

#define KAPI_SEM_FIFO          0
#define KAPI_SEM_PRIORITY      1
#define KAPI_SEM_PROTECT_CEILING 2
#define KAPI_SEM_PROTECT_INHERIT 3

#define KAPI_COND_CLOCK_MONOTONIC 0
#define KAPI_COND_CLOCK_REALTIME  1
#define KAPI_COND_CLOCK_BOOTTIME  2

#define KAPI_BARRIER_SERIAL_THREAD -1

#define KAPI_RWLOCK_ACQUIRE_READ  0x01
#define KAPI_RWLOCK_ACQUIRE_WRITE 0x02
#define KAPI_RWLOCK_TRY_READ      0x04
#define KAPI_RWLOCK_TRY_WRITE     0x08
#define KAPI_RWLOCK_RELEASE_READ  0x10
#define KAPI_RWLOCK_RELEASE_WRITE 0x20

typedef struct kapi_mutex* kapi_mutex_t;
typedef struct kapi_rwlock* kapi_rwlock_t;
typedef struct kapi_spinlock* kapi_spinlock_t;
typedef struct kapi_semaphore* kapi_sem_t;
typedef struct kapi_condition* kapi_cond_t;
typedef struct kapi_barrier* kapi_barrier_t;
typedef struct kapi_read_write_lock* kapi_rwlock_ext_t;
typedef struct kapi_futex* kapi_futex_t;

typedef struct {
    int type;
    bool is_locked;
    bool is_recursive;
    int lock_count;
    int owner_tid;
    int owner_pid;
    uint64_t lock_time;
    uint64_t hold_time;
    uint64_t wait_count;
    uint64_t contention_count;
} kapi_mutex_stats_t;

typedef struct {
    int reader_count;
    int writer_count;
    bool writer_active;
    bool write_pending;
    int reader_waiters;
    int writer_waiters;
    uint64_t read_hold_time;
    uint64_t write_hold_time;
    uint64_t wait_count;
} kapi_rwlock_stats_t;

typedef struct {
    bool is_locked;
    int cpu_id;
    int depth;
    uint64_t acquire_time;
    uint64_t spin_count;
    uint64_t fail_count;
} kapi_spinlock_stats_t;

typedef struct {
    int value;
    int max_value;
    int waiting_count;
    uint64_t post_count;
    uint64_t wait_count;
    uint64_t zero_count;
} kapi_sem_stats_t;

typedef struct {
    int waiting_count;
    int signaled_count;
    int broadcast_count;
    uint64_t total_wakeups;
    uint64_t spurious_wakeups;
} kapi_cond_stats_t;

kapi_mutex_t kapi_mutex_create(int type);

int kapi_mutex_destroy(kapi_mutex_t mutex);

int kapi_mutex_lock(kapi_mutex_t mutex);

int kapi_mutex_trylock(kapi_mutex_t mutex);

int kapi_mutex_timedlock(kapi_mutex_t mutex, const struct timespec* abs_timeout);

int kapi_mutex_unlock(kapi_mutex_t mutex);

int kapi_mutex_getprioceiling(kapi_mutex_t mutex, int* prioceiling);

int kapi_mutex_setprioceiling(kapi_mutex_t mutex, int prioceiling, int* old_ceiling);

int kapi_mutex_get_stats(kapi_mutex_t mutex, kapi_mutex_stats_t* stats);

kapi_rwlock_t kapi_rwlock_create(int pref);

int kapi_rwlock_destroy(kapi_rwlock_t rwlock);

int kapi_rwlock_rdlock(kapi_rwlock_t rwlock);

int kapi_rwlock_tryrdlock(kapi_rwlock_t rwlock);

int kapi_rwlock_wrlock(kapi_rwlock_t rwlock);

int kapi_rwlock_trywrlock(kapi_rwlock_t rwlock);

int kapi_rwlock_unlock(kapi_rwlock_t rwlock);

int kapi_rwlock_timedrdlock(kapi_rwlock_t rwlock, const struct timespec* abs_timeout);

int kapi_rwlock_timedwrlock(kapi_rwlock_t rwlock, const struct timespec* abs_timeout);

int kapi_rwlock_get_stats(kapi_rwlock_t rwlock, kapi_rwlock_stats_t* stats);

kapi_spinlock_t kapi_spin_create(int flags);

void kapi_spin_destroy(kapi_spinlock_t spin);

void kapi_spin_lock(kapi_spinlock_t spin);

bool kapi_spin_trylock(kapi_spinlock_t spin);

void kapi_spin_unlock(kapi_spinlock_t spin);

bool kapi_spin_is_locked(kapi_spinlock_t spin);

bool kapi_spin_can_lock(kapi_spinlock_t spin);

int kapi_spin_get_stats(kapi_spinlock_t spin, kapi_spinlock_stats_t* stats);

kapi_sem_t kapi_sem_create(unsigned int value, int pshared);

int kapi_sem_destroy(kapi_sem_t sem);

int kapi_sem_wait(kapi_sem_t sem);

int kapi_sem_trywait(kapi_sem_t sem);

int kapi_sem_timedwait(kapi_sem_t sem, const struct timespec* abs_timeout);

int kapi_sem_post(kapi_sem_t sem);

int kapi_sem_getvalue(kapi_sem_t sem, int* sval);

int kapi_sem_get_stats(kapi_sem_t sem, kapi_sem_stats_t* stats);

kapi_cond_t kapi_cond_create(int clock_type);

int kapi_cond_destroy(kapi_cond_t cond);

int kapi_cond_wait(kapi_cond_t cond, kapi_mutex_t mutex);

int kapi_cond_timedwait(kapi_cond_t cond, kapi_mutex_t mutex, const struct timespec* abs_timeout);

int kapi_cond_signal(kapi_cond_t cond);

int kapi_cond_broadcast(kapi_cond_t cond);

int kapi_cond_get_stats(kapi_cond_t cond, kapi_cond_stats_t* stats);

kapi_barrier_t kapi_barrier_create(unsigned int count);

int kapi_barrier_destroy(kapi_barrier_t barrier);

int kapi_barrier_wait(kapi_barrier_t barrier);

kapi_futex_t kapi_futex_create(uint32_t* uaddr, int init_val);

int kapi_futex_destroy(kapi_futex_t futex);

int kapi_futex_wait(kapi_futex_t futex, uint32_t val, const struct timespec* timeout);

int kapi_futex_wake(kapi_futex_t futex, int count);

int kapi_futex_requeue(kapi_futex_t futex, uint32_t val, kapi_futex_t futex2, int count, int count2);

int kapi_futex_cmp_requeue(kapi_futex_t futex, uint32_t cmpval, uint32_t newval,
                            kapi_futex_t futex2, int count, int count2);

int kapi_read_write_lock_init(kapi_rwlock_ext_t* rwl);

int kapi_read_write_lock_destroy(kapi_rwlock_ext_t* rwl);

int kapi_read_lock(kapi_rwlock_ext_t* rwl);

int kapi_read_trylock(kapi_rwlock_ext_t* rwl);

int kapi_read_unlock(kapi_rwlock_ext_t* rwl);

int kapi_write_lock(kapi_rwlock_ext_t* rwl);

int kapi_write_trylock(kapi_rwlock_ext_t* rwl);

int kapi_write_unlock(kapi_rwlock_ext_t* rwl);

int kapi_read_write_lock_downgrade(kapi_rwlock_ext_t* rwl);

int kapi_read_write_lock_upgrade(kapi_rwlock_ext_t* rwl);

int kapi_atomic_inc(volatile int* addr);

int kapi_atomic_dec(volatile int* addr);

int kapi_atomic_add(volatile int* addr, int val);

int kapi_atomic_sub(volatile int* addr, int val);

int kapi_atomic_xchg(volatile int* addr, int newval);

int kapi_atomic_cmpxchg(volatile int* addr, int oldval, int newval);

int kapi_atomic_test_and_set(volatile int* addr, int newval);

void* kapi_atomic_xchg_ptr(void** addr, void* newval);

void* kapi_atomic_cmpxchg_ptr(void** addr, void* oldval, void* newval);

int kapi_memory_barrier(void);

int kapi_read_barrier(void);

int kapi_write_barrier(void);

int kapi_compiler_barrier(void);

int kapi_smp_mb(void);

int kapi_smp_rmb(void);

int kapi_smp_wmb(void);

int kapi_rcu_read_lock(void);

int kapi_rcu_read_unlock(void);

void kapi_rcu_synchronize(void);

void kapi_call_rcu(struct rcu_head* head, void (*func)(struct rcu_head* head));

int kapi_completion_init(kapi_completion_t* comp);

void kapi_completion_wait(kapi_completion_t* comp);

bool kapi_completion_trywait(kapi_completion_t* comp);

bool kapi_completion_timeout(kapi_completion_t* comp, unsigned long timeout);

void kapi_completion_complete(kapi_completion_t* comp);

void kapi_completion_complete_all(kapi_completion_t* comp);

bool kapi_completion_done(kapi_completion_t* comp);

#ifdef __cplusplus
}
#endif

#endif