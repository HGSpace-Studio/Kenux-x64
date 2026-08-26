

#include "kapi_completion.h"
#include "kapi.h"

#include <sync.h>
#include <arch/memory.h>
#include <string.h>

struct kapi_completion {
    completion_t c;
};

kapi_completion_t kapi_completion_create(void)
{
    kapi_completion_t c = (kapi_completion_t)memory_alloc(sizeof(struct kapi_completion));
    if (!c) {
        return NULL;
    }
    completion_init(&c->c);
    return c;
}

void kapi_completion_destroy(kapi_completion_t c)
{
    if (!c) {
        return;
    }
    memory_free(c);
}

void kapi_completion_wait(kapi_completion_t c)
{
    if (!c) {
        return;
    }
    completion_wait(&c->c);
}

void kapi_completion_complete(kapi_completion_t c)
{
    if (!c) {
        return;
    }
    completion_complete(&c->c);
}

void kapi_completion_reset(kapi_completion_t c)
{
    if (!c) {
        return;
    }
    c->c.done = 0;
}
