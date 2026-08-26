#ifndef ARCH_X86_64_IDT_H
#define ARCH_X86_64_IDT_H

#include <arch/types.h>

#define IDT_ENTRIES 256

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

void idt_init(void);
void idt_set_gate(uint8_t index, uint64_t handler, uint16_t selector, uint8_t type_attr, uint8_t ist);
void idt_flush(uint64_t addr);

#endif
