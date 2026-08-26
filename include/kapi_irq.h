

#ifndef KAPI_IRQ_H
#define KAPI_IRQ_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*kapi_irq_handler_t)(int irq, void* arg);

int kapi_request_irq(int irq, kapi_irq_handler_t handler, void* arg);

void kapi_free_irq(int irq);

void kapi_irq_dispatch(int irq);

#ifdef __cplusplus
}
#endif

#endif
