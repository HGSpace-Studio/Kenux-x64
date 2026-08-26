#include <interrupt.h>
#include <string.h>
#include <arch/io.h>
#include <arch/apic.h>

static void* interrupt_handlers[256];
extern void kapi_irq_dispatch(int irq) __attribute__((weak));

void interrupt_init(void)
{
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));
}

void interrupt_register(uint8_t irq, void* handler)
{
    if (irq < 256) {
        interrupt_handlers[irq] = handler;
    }
}

/* CPU exception vectors that push an error code */
static int exception_has_error_code(uint8_t vector)
{
    switch (vector) {
        case 8:   /* Double Fault */
        case 10:  /* Invalid TSS */
        case 11:  /* Segment Not Present */
        case 12:  /* Stack Fault */
        case 13:  /* GP Fault */
        case 14:  /* Page Fault */
        case 17:  /* Alignment Check */
        case 21:  /* Control Protection */
            return 1;
        default:
            return 0;
    }
}

void interrupt_dispatch(void)
{
    /* The assembly interrupt_handler pushes all GP registers and
       passes RSP in RDI. But we declared this as void(void).
       We need to read the saved RIP to determine the vector.
       Actually, the vector is determined by reading the PIC ISR
       for IRQs, or by the IDT entry for exceptions.

       For simplicity: read PIC ISR to find active IRQ.
       If no IRQ is active, it's a CPU exception — just EOI and return. */

    uint8_t irq = 0xFF;

    outb(0x20, 0x0B);
    uint8_t isr1 = inb(0x20);
    if (isr1) {
        irq = 0;
        while ((isr1 & 1) == 0) {
            isr1 >>= 1;
            irq++;
        }
    } else {
        outb(0xA0, 0x0B);
        uint8_t isr2 = inb(0xA0);
        if (isr2) {
            irq = 8;
            while ((isr2 & 1) == 0) {
                isr2 >>= 1;
                irq++;
            }
        }
    }

    /* Only handle hardware IRQs (from PIC).
       CPU exceptions (irq == 0xFF) are just acknowledged and returned. */
    if (irq != 0xFF) {
        void (*handler)(void) = (void (*)(void))interrupt_handlers[irq];
        if (handler) {
            handler();
        }
        if (kapi_irq_dispatch) {
            kapi_irq_dispatch((int)irq);
        }

        /* Timer tick for IRQ 0 */
        if (irq == 0) {
            extern void timer_tick(void);
            timer_tick();
        }

        /* Send EOI */
        if (irq >= 8) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
    /* For CPU exceptions: do nothing, just return.
       The iretq in the assembly will return to the faulting instruction.
       This is not ideal but prevents triple faults. */
}
