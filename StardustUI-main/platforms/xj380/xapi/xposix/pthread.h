#pragma once

#include "sys/types.h"
#include "signal.h"

#define PTHREAD_MUTEX_INITIALIZER  { 0 }
#define PTHREAD_COND_INITIALIZER   { 0 }
#define PTHREAD_RWLOCK_INITIALIZER { 0 }

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_DEFAULT    0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2

#define PTHREAD_ONCE_INIT { 0 }

#define PTHREAD_INHERIT_SCHED  0
#define PTHREAD_EXPLICIT_SCHED 1

#define PTHREAD_SCOPE_SYSTEM  0
#define PTHREAD_SCOPE_PROCESS 1

typedef unsigned long pthread_t;
typedef int pthread_key_t;
typedef int pthread_once_t;
typedef int pthread_spinlock_t;

typedef struct {
    int             type;
    int             owner;
    int             count;
    unsigned int    locked;
} pthread_mutex_t;

typedef struct {
    unsigned int    waiting;
    unsigned int    signaled;
} pthread_cond_t;

typedef struct {
    unsigned int    readers;
    unsigned int    writer;
    int             writer_thread;
} pthread_rwlock_t;

typedef struct {
    int             detachstate;
    int             inheritsched;
    int             schedpolicy;
    struct sched_param schedparam;
    int             scope;
    size_t          guardsize;
    size_t          stacksize;
    void           *stackaddr;
} pthread_attr_t;

typedef struct {
    int             type;
} pthread_mutexattr_t;

typedef struct {
    int             pshared;
} pthread_condattr_t;

typedef struct {
    int             pshared;
} pthread_rwlockattr_t;

struct sched_param {
    int sched_priority;
};

#ifdef __cplusplus
extern "C" {
#endif

int  pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                    void *(*start_routine)(void *), void *arg);
int  pthread_join(pthread_t thread, void **retval);
int  pthread_detach(pthread_t thread);
int  pthread_cancel(pthread_t thread);
void pthread_exit(void *retval);
int  pthread_tryjoin_np(pthread_t thread, void **retval);

pthread_t pthread_self(void);
int       pthread_equal(pthread_t t1, pthread_t t2);
int       pthread_getcpuclockid(pthread_t thread, int *clock_id);

int  pthread_attr_init(pthread_attr_t *attr);
int  pthread_attr_destroy(pthread_attr_t *attr);
int  pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int  pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int  pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
int  pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int  pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr);
int  pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);
int  pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize);
int  pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);
int  pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);
int  pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int  pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched);
int  pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched);
int  pthread_attr_getscope(const pthread_attr_t *attr, int *scope);
int  pthread_attr_setscope(pthread_attr_t *attr, int scope);

int  pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int  pthread_mutex_destroy(pthread_mutex_t *mutex);
int  pthread_mutex_lock(pthread_mutex_t *mutex);
int  pthread_mutex_trylock(pthread_mutex_t *mutex);
int  pthread_mutex_unlock(pthread_mutex_t *mutex);
int  pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime);

int  pthread_mutexattr_init(pthread_mutexattr_t *attr);
int  pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int  pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int  pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
int  pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);
int  pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);

int  pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int  pthread_cond_destroy(pthread_cond_t *cond);
int  pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int  pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                            const struct timespec *abstime);
int  pthread_cond_signal(pthread_cond_t *cond);
int  pthread_cond_broadcast(pthread_cond_t *cond);

int  pthread_condattr_init(pthread_condattr_t *attr);
int  pthread_condattr_destroy(pthread_condattr_t *attr);
int  pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared);
int  pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);

int  pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int  pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int  pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int  pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int  pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int  pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int  pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

int  pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int  pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int  pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared);
int  pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared);

int  pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int  pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int   pthread_setspecific(pthread_key_t key, const void *value);

int  pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

int  pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int  pthread_spin_destroy(pthread_spinlock_t *lock);
int  pthread_spin_lock(pthread_spinlock_t *lock);
int  pthread_spin_trylock(pthread_spinlock_t *lock);
int  pthread_spin_unlock(pthread_spinlock_t *lock);

int  pthread_setcancelstate(int state, int *oldstate);
int  pthread_setcanceltype(int type, int *oldtype);
void pthread_testcancel(void);

int  pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));

int  sched_yield(void);
int  sched_get_priority_max(int policy);
int  sched_get_priority_min(int policy);

#ifdef __cplusplus
}
#endif