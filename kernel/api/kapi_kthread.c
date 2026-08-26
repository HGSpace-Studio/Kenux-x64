

#include "kapi_kthread.h"
#include "kapi.h"

#include <kthread.h>
#include <arch/memory.h>
#include <string.h>

struct kapi_kthread {
    process_t* p;
};

kapi_kthread_t kapi_kthread_run(const char* name, int (*func)(void*), void* arg)
{
    process_t* p = kthread_run(name, func, arg);
    if (!p) {
        return NULL;
    }

    kapi_kthread_t kt = (kapi_kthread_t)memory_alloc(sizeof(struct kapi_kthread));
    if (!kt) {
        return NULL;
    }
    kt->p = p;
    return kt;
}

void kapi_kthread_stop(kapi_kthread_t kt)
{
    if (!kt || !kt->p) {
        return;
    }
    kthread_stop(kt->p);
}

int kapi_kthread_join(kapi_kthread_t kt, uint64_t* ret)
{
    if (!kt || !kt->p) {
        return KAPI_EINVAL;
    }
    return kthread_join(kt->p, ret);
}

int kapi_kthread_should_stop(void)
{
    return kthread_should_stop();
}

void kapi_kthread_wake(kapi_kthread_t kt)
{
    if (!kt || !kt->p) {
        return;
    }
    kthread_wake(kt->p);
}
