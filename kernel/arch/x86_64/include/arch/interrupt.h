#ifndef ARCH_X86_64_INTERRUPT_H
#define ARCH_X86_64_INTERRUPT_H

#include <arch/types.h>

void interrupt_init(void);
void interrupt_register(uint8_t irq, void* handler);

/* Assembly entry points — called by CPU on interrupt/exception.
   These push all registers and call interrupt_dispatch(). */
void interrupt_handler(void);       /* for IRQs and exceptions without error code */
void interrupt_handler_err(void);   /* for exceptions that push an error code */

#endif
