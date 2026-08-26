

#ifndef KAPI_RCU_H
#define KAPI_RCU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_rcu_head {
    struct kapi_rcu_head* next;
    void (*func)(void*);
    void* arg;
} kapi_rcu_head_t;

typedef struct {
    uint32_t lock;
    int32_t  idx;
    uint64_t readers[2];
} kapi_srcu_struct_t;

void kapi_rcu_read_lock(void);

void kapi_rcu_read_unlock(void);

void kapi_call_rcu(kapi_rcu_head_t* head, void (*func)(void*), void* arg);

void kapi_rcu_barrier(void);

void kapi_srcu_init(kapi_srcu_struct_t* sp);

void kapi_srcu_read_lock(kapi_srcu_struct_t* sp);

void kapi_srcu_read_unlock(kapi_srcu_struct_t* sp);

void kapi_srcu_synchronize(kapi_srcu_struct_t* sp);

#ifdef __cplusplus
}
#endif

#endif
