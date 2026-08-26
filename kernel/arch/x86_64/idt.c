#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/interrupt.h>
#include <string.h>
#include <arch/vga.h>

void syscall_handler(void);
void page_fault_handler(void);

extern void usermode_syscall_entry(void);
extern void usermode_page_fault_entry(void);

/* interrupt_handler and interrupt_handler_err are declared in arch/interrupt.h */

static idt_entry_t idt[IDT_ENTRIES];
static idt_register_t idt_register;

/* Exception vectors that push an error code */
#define HAS_ERR  (1 << 8 | 1 << 10 | 1 << 11 | 1 << 12 | 1 << 13 | 1 << 14 | 1 << 17)

static int vec_has_error_code(int v)
{
    return (HAS_ERR >> v) & 1;
}

void idt_init(void)
{
    idt_register.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_register.base = (uint64_t)idt;

    memset(idt, 0, sizeof(idt));

    /* Register ALL 256 IDT entries.
       - Exceptions with error code (8,10,11,12,13,14,17) use interrupt_handler_err
       - Everything else uses interrupt_handler
       This prevents triple faults from unhandled exceptions. */
    for (int i = 0; i < 256; i++) {
        if (vec_has_error_code(i)) {
            idt_set_gate(i, (uint64_t)interrupt_handler_err, 0x08, 0x8E, 0);
        } else {
            idt_set_gate(i, (uint64_t)interrupt_handler, 0x08, 0x8E, 0);
        }
    }

    /* Override specific entries with dedicated handlers */
    idt_set_gate(0x80, (uint64_t)usermode_syscall_entry, 0x1B, 0x8E, 0);
    idt_set_gate(0x0E, (uint64_t)usermode_page_fault_entry, 0x08, 0x8E, 0);

    /* IRQ entries (already set by loop, but explicit for clarity) */
    idt_set_gate(0x20, (uint64_t)interrupt_handler, 0x08, 0x8E, 0);
    idt_set_gate(0x21, (uint64_t)interrupt_handler, 0x08, 0x8E, 0);

    idt_flush((uint64_t)&idt_register);
}

void idt_set_gate(uint8_t index, uint64_t handler, uint16_t selector, uint8_t type_attr, uint8_t ist)
{
    idt[index].base_low = handler & 0xFFFF;
    idt[index].base_mid = (handler >> 16) & 0xFFFF;
    idt[index].base_high = (handler >> 32) & 0xFFFFFFFF;
    idt[index].zero = 0;
    idt[index].selector = selector;
    idt[index].ist = ist;
    idt[index].type_attr = type_attr;
}

void idt_flush(uint64_t addr)
{
    __asm__ volatile ("lidt (%0)" :: "r" (addr));
}
