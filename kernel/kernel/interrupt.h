#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <arch/types.h>

#define IRQ_MAX 16
#define EXCEPTION_MAX 32

typedef struct {
    void* handler;
} interrupt_entry_t;

void interrupt_init(void);
void interrupt_register(uint8_t irq, void* handler);
void interrupt_dispatch(void);

#endif
