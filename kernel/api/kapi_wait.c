

#include "kapi_wait.h"
#include "kapi.h"

#include <wait.h>
#include <arch/memory.h>
#include <string.h>

struct kapi_waitqueue {
    wait_queue_t wq;
};

kapi_waitqueue_t kapi_waitqueue_create(void)
{
    kapi_waitqueue_t wq = (kapi_waitqueue_t)memory_alloc(sizeof(struct kapi_waitqueue));
    if (!wq) {
        return NULL;
    }
    wait_queue_init(&wq->wq);
    return wq;
}

void kapi_waitqueue_destroy(kapi_waitqueue_t wq)
{
    if (!wq) {
        return;
    }
    memory_free(wq);
}

void kapi_waitqueue_wake_up(kapi_waitqueue_t wq)
{
    if (!wq) {
        return;
    }
    wait_queue_wake_one(&wq->wq);
}

void kapi_waitqueue_wake_up_all(kapi_waitqueue_t wq)
{
    if (!wq) {
        return;
    }
    wait_queue_wake_all(&wq->wq);
}
