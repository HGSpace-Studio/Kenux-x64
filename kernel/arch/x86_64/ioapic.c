

#include <arch/ioapic.h>
#include <arch/spinlock.h>

static volatile uint32_t *ioapic_base = NULL;

static uint32_t ioapic_max_entries = 0;

static uint32_t ioapic_version = 0;

static spinlock_t ioapic_lock = SPINLOCK_INIT;

static inline void ioapic_mb(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

void ioapic_init(uint64_t base_addr)
{

    ioapic_base = (volatile uint32_t *)(uintptr_t)base_addr;

    ioapic_version = ioapic_read_reg(IOAPIC_REG_VER);
    ioapic_max_entries = ((ioapic_version >> 16) & 0xFF) + 1;

    for (uint32_t i = 0; i < ioapic_max_entries; i++) {
        uint32_t reg = IOAPIC_REG_REDIR_BASE + i * 2;

        ioapic_write_reg(reg + 1, 0x00);

        ioapic_write_reg(reg, (uint32_t)(0x20 + i));
    }
}

uint32_t ioapic_read_reg(uint32_t reg)
{
    uint32_t value;

    spin_lock(&ioapic_lock);

    ioapic_base[0] = reg;

    ioapic_mb();

    value = ioapic_base[4];

    spin_unlock(&ioapic_lock);

    return value;
}

void ioapic_write_reg(uint32_t reg, uint32_t value)
{
    spin_lock(&ioapic_lock);

    ioapic_base[0] = reg;

    ioapic_mb();

    ioapic_base[4] = value;

    ioapic_mb();

    spin_unlock(&ioapic_lock);
}

void ioapic_set_redirect(uint8_t irq_num, uint8_t apic_id, uint8_t vector, uint32_t flags)
{
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq_num * 2;
    uint32_t low = 0, high = 0;

    low = (uint32_t)vector;

    if (flags & IOAPIC_RED_FLAG_NMI) {
        low |= IOAPIC_DELIVERY_NMI;
    } else if (flags & IOAPIC_RED_FLAG_SMI) {
        low |= IOAPIC_DELIVERY_SMI;
    } else if (flags & IOAPIC_RED_FLAG_EXTINT) {
        low |= IOAPIC_DELIVERY_EXTINT;
    } else if (flags & IOAPIC_RED_FLAG_LOWPRIO) {
        low |= IOAPIC_DELIVERY_LOWPRIO;
    } else {
        low |= IOAPIC_DELIVERY_FIXED;
    }

    if (flags & IOAPIC_RED_FLAG_LEVEL) {
        low |= IOAPIC_TRIGGER_LEVEL;
    }

    if (flags & IOAPIC_RED_FLAG_ACTIVE_LOW) {
        low |= IOAPIC_POLARITY_ACTIVE_LOW;
    }

    if (flags & IOAPIC_RED_FLAG_MASKED) {
        low |= IOAPIC_MASKED;
    }

    if (flags & IOAPIC_RED_FLAG_LOGICAL) {
        low |= (1u << 11);
    }

    high = (uint32_t)apic_id << 24;

    ioapic_write_reg(reg, low);
    ioapic_write_reg(reg + 1, high);
}

void ioapic_mask_irq(uint8_t irq_num)
{
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq_num * 2;

    uint32_t low = ioapic_read_reg(reg);
    low |= IOAPIC_MASKED;
    ioapic_write_reg(reg, low);
}

void ioapic_unmask_irq(uint8_t irq_num)
{
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq_num * 2;

    uint32_t low = ioapic_read_reg(reg);
    low &= ~IOAPIC_MASKED;
    ioapic_write_reg(reg, low);
}

uint64_t ioapic_get_redirect_entry(uint8_t irq_num)
{
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq_num * 2;

    uint32_t low  = ioapic_read_reg(reg);
    uint32_t high = ioapic_read_reg(reg + 1);

    return ((uint64_t)high << 32) | (uint64_t)low;
}

void ioapic_get_version(uint32_t *version, uint32_t *max_irq)
{
    if (version) *version = ioapic_version & 0xFF;
    if (max_irq)  *max_irq  = ioapic_max_entries;
}
