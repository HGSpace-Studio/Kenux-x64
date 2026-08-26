#include "kapi.h"
#include <interrupt.h>
#include <string.h>

#define KAPI_IRQ_MAX IRQ_MAX

typedef struct {
    int used;
    kapi_irq_handler_t handler;
    void* arg;
} kapi_irq_entry_t;

static kapi_irq_entry_t kapi_irq_table[KAPI_IRQ_MAX];

int kapi_irq_init(void)
{
    memset(kapi_irq_table, 0, sizeof(kapi_irq_table));
    return KAPI_OK;
}

int kapi_request_irq(int irq, kapi_irq_handler_t handler, void* arg)
{
    if (irq < 0 || irq >= KAPI_IRQ_MAX || !handler) {
        return KAPI_EINVAL;
    }
    if (kapi_irq_table[irq].used) {
        return KAPI_EBUSY;
    }

    kapi_irq_table[irq].used = 1;
    kapi_irq_table[irq].handler = handler;
    kapi_irq_table[irq].arg = arg;
    return KAPI_OK;
}

void kapi_free_irq(int irq)
{
    if (irq < 0 || irq >= KAPI_IRQ_MAX) {
        return;
    }

    memset(&kapi_irq_table[irq], 0, sizeof(kapi_irq_table[irq]));
}

void kapi_irq_dispatch(int irq)
{
    if (irq < 0 || irq >= KAPI_IRQ_MAX) {
        return;
    }
    if (!kapi_irq_table[irq].used || !kapi_irq_table[irq].handler) {
        return;
    }

    kapi_irq_table[irq].handler(irq, kapi_irq_table[irq].arg);
}
