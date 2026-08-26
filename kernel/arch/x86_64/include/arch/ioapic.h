

#ifndef ARCH_IOAPIC_H
#define ARCH_IOAPIC_H

#include <arch/types.h>

#define IOAPIC_BASE_DEFAULT  0xFEC00000ULL

#define IOAPIC_REG_ID          0x00
#define IOAPIC_REG_VER         0x01
#define IOAPIC_REG_ARB         0x02
#define IOAPIC_REG_BOOT        0x03

#define IOAPIC_REG_REDIR_BASE  0x10

#define IOAPIC_DELIVERY_FIXED     0x000
#define IOAPIC_DELIVERY_LOWPRIO   0x100
#define IOAPIC_DELIVERY_SMI       0x200
#define IOAPIC_DELIVERY_NMI       0x300
#define IOAPIC_DELIVERY_INIT      0x400
#define IOAPIC_DELIVERY_EXTINT    0x500

#define IOAPIC_POLARITY_ACTIVE_HIGH  0x000
#define IOAPIC_POLARITY_ACTIVE_LOW   0x800

#define IOAPIC_TRIGGER_EDGE          0x000
#define IOAPIC_TRIGGER_LEVEL         0x400

#define IOAPIC_MASKED    0x10000
#define IOAPIC_UNMASKED  0x00000

#define IOAPIC_DEST_SHIFT   56

#define IOAPIC_RED_FLAG_MASKED       (1u << 0)
#define IOAPIC_RED_FLAG_LEVEL        (1u << 1)
#define IOAPIC_RED_FLAG_ACTIVE_LOW   (1u << 2)
#define IOAPIC_RED_FLAG_NMI          (1u << 3)
#define IOAPIC_RED_FLAG_SMI          (1u << 4)
#define IOAPIC_RED_FLAG_EXTINT       (1u << 5)
#define IOAPIC_RED_FLAG_LOWPRIO      (1u << 6)
#define IOAPIC_RED_FLAG_LOGICAL      (1u << 7)

void ioapic_init(uint64_t base_addr);

uint32_t ioapic_read_reg(uint32_t reg);

void ioapic_write_reg(uint32_t reg, uint32_t value);

void ioapic_set_redirect(uint8_t irq_num, uint8_t apic_id, uint8_t vector, uint32_t flags);

void ioapic_mask_irq(uint8_t irq_num);

void ioapic_unmask_irq(uint8_t irq_num);

uint64_t ioapic_get_redirect_entry(uint8_t irq_num);

void ioapic_get_version(uint32_t *version, uint32_t *max_irq);

#endif
