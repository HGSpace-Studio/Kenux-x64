

#include "kapi_rcu.h"
#include "kapi.h"

#include <rcu.h>

void kapi_rcu_read_lock(void)
{
    rcu_read_lock();
}

void kapi_rcu_read_unlock(void)
{
    rcu_read_unlock();
}

void kapi_call_rcu(kapi_rcu_head_t* head, void (*func)(void*), void* arg)
{
    if (!head || !func) {
        return;
    }
    head->func = func;
    head->arg = arg;
    head->next = NULL;
    call_rcu((rcu_head_t*)head, (rcu_callback_t)func, arg);
}

void kapi_rcu_barrier(void)
{
    rcu_barrier();
}

void kapi_srcu_init(kapi_srcu_struct_t* sp)
{
    if (!sp) {
        return;
    }
    srcu_init((srcu_struct_t*)sp);
}

void kapi_srcu_read_lock(kapi_srcu_struct_t* sp)
{
    if (!sp) {
        return;
    }
    srcu_read_lock((srcu_struct_t*)sp);
}

void kapi_srcu_read_unlock(kapi_srcu_struct_t* sp)
{
    if (!sp) {
        return;
    }
    srcu_read_unlock((srcu_struct_t*)sp);
}

void kapi_srcu_synchronize(kapi_srcu_struct_t* sp)
{
    if (!sp) {
        return;
    }
    srcu_synchronize((srcu_struct_t*)sp);
}
