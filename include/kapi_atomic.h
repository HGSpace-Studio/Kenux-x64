

#ifndef KAPI_ATOMIC_H
#define KAPI_ATOMIC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile int value;
} kapi_atomic_t;

#define KAPI_ATOMIC_INIT(i) { (i) }

static inline void kapi_atomic_set(kapi_atomic_t* v, int i)
{
    v->value = i;
}

static inline int kapi_atomic_read(const kapi_atomic_t* v)
{
    return v->value;
}

static inline void kapi_atomic_inc(kapi_atomic_t* v)
{
    __asm__ volatile (
        "lock; incl %0"
        : "+m" (v->value)
        :
        : "memory"
    );
}

static inline void kapi_atomic_dec(kapi_atomic_t* v)
{
    __asm__ volatile (
        "lock; decl %0"
        : "+m" (v->value)
        :
        : "memory"
    );
}

static inline int kapi_atomic_inc_return(kapi_atomic_t* v)
{
    int ret = 1;
    __asm__ volatile (
        "lock; xaddl %0, %1"
        : "+r" (ret), "+m" (v->value)
        :
        : "memory"
    );
    return ret + 1;
}

static inline int kapi_atomic_dec_return(kapi_atomic_t* v)
{
    int ret = -1;
    __asm__ volatile (
        "lock; xaddl %0, %1"
        : "+r" (ret), "+m" (v->value)
        :
        : "memory"
    );
    return ret - 1;
}

static inline void kapi_atomic_add(int i, kapi_atomic_t* v)
{
    __asm__ volatile (
        "lock; addl %1, %0"
        : "+m" (v->value)
        : "ir" (i)
        : "memory"
    );
}

static inline void kapi_atomic_sub(int i, kapi_atomic_t* v)
{
    __asm__ volatile (
        "lock; subl %1, %0"
        : "+m" (v->value)
        : "ir" (i)
        : "memory"
    );
}

static inline int kapi_atomic_sub_and_test(int i, kapi_atomic_t* v)
{
    int ret;
    __asm__ volatile (
        "lock; subl %2, %0; sete %1"
        : "+m" (v->value), "=qm" (ret)
        : "ir" (i)
        : "memory"
    );
    return ret;
}

static inline int kapi_atomic_cmpxchg(kapi_atomic_t* v, int old, int new_val)
{
    int ret;
    __asm__ volatile (
        "lock; cmpxchgl %2, %1"
        : "=a" (ret), "+m" (v->value)
        : "r" (new_val), "0" (old)
        : "memory"
    );
    return ret;
}

static inline int kapi_atomic_xchg(kapi_atomic_t* v, int new_val)
{
    int ret;
    __asm__ volatile (
        "lock; xchgl %0, %1"
        : "=r" (ret), "+m" (v->value)
        : "0" (new_val)
        : "memory"
    );
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif
