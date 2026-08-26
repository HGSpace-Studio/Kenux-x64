#ifndef ARCH_X86_64_GDT_H
#define ARCH_X86_64_GDT_H

#include <arch/types.h>

#define GDT_ENTRIES 8

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags_limit;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_register_t;

void gdt_init(void);
void gdt_set_gate(uint8_t index, uint64_t base, uint64_t limit, uint8_t access, uint8_t flags);
void gdt_flush(uint64_t addr);

extern gdt_entry_t gdt[GDT_ENTRIES];
extern gdt_register_t gdt_register;

#endif
