

#include <arch/apic.h>
#include <arch/io.h>
#include <arch/spinlock.h>

static volatile uint32_t *lapic_base = NULL;

static uint32_t lapic_timer_freq = 0;

static spinlock_t lapic_lock = SPINLOCK_INIT;

#define PIT_BASE_FREQ       1193182UL
#define PIT_CHANNEL2         0x42
#define PIT_COMMAND          0x43
#define PIT_SPEAKER_PORT     0x61

#define PIT_CMD_CH2_LH_MODE0 0xB0

static inline void apic_mb(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

void lapic_init(uint64_t base_addr)
{

    lapic_base = (volatile uint32_t *)(uintptr_t)base_addr;

    lapic_write(LAPIC_ESR, 0);

    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xFF);

    lapic_write(LAPIC_LVT_THERMAL, LAPIC_TIMER_MASKED | 0);

    lapic_write(LAPIC_LVT_PERF, LAPIC_TIMER_MASKED | 0);

    lapic_write(LAPIC_LVT_LINT0, LAPIC_TIMER_MASKED | 0);

    lapic_write(LAPIC_LVT_LINT1, LAPIC_TIMER_MASKED | 0);

    lapic_write(LAPIC_LVT_ERROR, LAPIC_TIMER_MASKED | 0);

    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_MASKED);
    lapic_write(LAPIC_TIMER_INIT, 0);
    lapic_write(LAPIC_TIMER_DIV, 0x03);

    lapic_write(LAPIC_TPR, 0);
}

uint32_t lapic_read(uint32_t reg)
{
    return lapic_base[reg / 4];
}

void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_base[reg / 4] = value;

    apic_mb();
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_id(void)
{

    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

void lapic_send_ipi(uint32_t target_apic_id, uint32_t vector)
{
    spin_lock(&lapic_lock);

    while (lapic_read(LAPIC_ICR0) & LAPIC_ICR_BUSY) {
        __asm__ volatile ("pause");
    }

    if (target_apic_id != 0xFF) {
        lapic_write(LAPIC_ICR1, (uint32_t)target_apic_id << 24);
    } else {

        lapic_write(LAPIC_ICR1, 0xFF << 24);
    }

    lapic_write(LAPIC_ICR0, vector | LAPIC_ICR_ASSERT | LAPIC_ICR_FIXED);

    while (lapic_read(LAPIC_ICR0) & LAPIC_ICR_BUSY) {
        __asm__ volatile ("pause");
    }

    spin_unlock(&lapic_lock);
}

void lapic_timer_calibrate(void)
{

    const uint32_t CALIBRATE_SAMPLES = 5;
    const uint32_t PIT_COUNT_1MS    = PIT_BASE_FREQ / 1000;
    uint32_t total_ticks = 0;

    spin_lock(&lapic_lock);

    for (uint32_t sample = 0; sample < CALIBRATE_SAMPLES; sample++) {

        outb(PIT_COMMAND, PIT_CMD_CH2_LH_MODE0);
        outb(PIT_CHANNEL2, (uint8_t)(PIT_COUNT_1MS & 0xFF));
        outb(PIT_CHANNEL2, (uint8_t)((PIT_COUNT_1MS >> 8) & 0xFF));

        lapic_write(LAPIC_TIMER_DIV, 0x0B);
        lapic_write(LAPIC_TIMER_INIT, 0xFFFFFFFF);
        lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | 0xFE);

        uint8_t speaker_val = inb(PIT_SPEAKER_PORT);
        outb(PIT_SPEAKER_PORT, (speaker_val & ~0x01) | 0x01);

        while (1) {
            outb(PIT_COMMAND, 0xE8);
            uint8_t status = inb(PIT_CHANNEL2);
            if (status & 0x80) break;
            __asm__ volatile ("pause");
        }

        speaker_val = inb(PIT_SPEAKER_PORT);
        outb(PIT_SPEAKER_PORT, speaker_val & ~0x01);

        uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CURR);
        total_ticks += elapsed;
    }

    spin_unlock(&lapic_lock);

    lapic_timer_freq = total_ticks / CALIBRATE_SAMPLES * 1000;

    lapic_write(LAPIC_TIMER_DIV, 0x03);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_MASKED);
    lapic_write(LAPIC_TIMER_INIT, 0);
}

void lapic_setup_timer(uint32_t vector, uint32_t ms)
{
    spin_lock(&lapic_lock);

    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_MASKED);

    lapic_write(LAPIC_TIMER_DIV, 0x03);

    uint32_t ticks = lapic_timer_freq * ms / 1000 / 16;
    if (ticks == 0) ticks = 1;

    lapic_write(LAPIC_TIMER_INIT, ticks);

    lapic_write(LAPIC_LVT_TIMER, vector | LAPIC_TIMER_PERIODIC);

    spin_unlock(&lapic_lock);
}

void lapic_lvt_mask(uint32_t reg)
{

    uint32_t val = lapic_read(reg);
    lapic_write(reg, val | LAPIC_TIMER_MASKED);
}

void lapic_lvt_unmask(uint32_t reg)
{

    uint32_t val = lapic_read(reg);
    lapic_write(reg, val & ~LAPIC_TIMER_MASKED);
}

uint32_t lapic_get_version(void)
{
    return lapic_read(LAPIC_VER) & 0xFF;
}

void lapic_mask_irq(uint32_t vector)
{
    uint32_t val = lapic_read(LAPIC_LVT_TIMER);
    lapic_write(LAPIC_LVT_TIMER, val | LAPIC_TIMER_MASKED);
}

void lapic_unmask_irq(uint32_t vector)
{
    uint32_t val = lapic_read(LAPIC_LVT_TIMER);
    lapic_write(LAPIC_LVT_TIMER, val & ~LAPIC_TIMER_MASKED);
}

uint32_t irq_to_vector(uint32_t irq)
{
    return irq + 0x20;
}

void disable_8259A(void)
{
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
}

static volatile uint32_t* ioapic_base = NULL;
static uint32_t ioapic_irq_trigger_type[256] = {0};

static uint32_t ioapic_read_reg(uint32_t reg)
{
    if (!ioapic_base) return 0;
    ioapic_base[0] = reg;
    return ioapic_base[4];
}

static void ioapic_write_reg(uint32_t reg, uint32_t value)
{
    if (!ioapic_base) return;
    ioapic_base[0] = reg;
    ioapic_base[4] = value;
}

void ioapic_init(void)
{
    ioapic_base = (volatile uint32_t*)IOAPIC_BASE_DEFAULT;

    uint32_t ver = ioapic_read_reg(IOAPIC_REG_VER);
    uint32_t max_entries = ((ver >> 16) & 0xFF) + 1;

    for (uint32_t i = 0; i < max_entries && i < 128; i++) {
        uint32_t reg = IOAPIC_REG_REDIR_BASE + i * 2;
        ioapic_write_reg(reg + 1, 0);
        ioapic_write_reg(reg, 0x10000);
        ioapic_irq_trigger_type[i] = IOAPIC_TRIGGER_EDGE;
    }
}

void ioapic_enable_irq(uint32_t irq, int trigger, int polarity)
{
    if (!ioapic_base || irq >= 128) return;

    uint32_t vector = irq_to_vector(irq);
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq * 2;

    uint64_t entry = (uint64_t)vector;
    entry |= ((uint64_t)lapic_id() << 56);

    if (trigger == IOAPIC_TRIGGER_LEVEL) {
        entry |= (1ULL << 13);
        ioapic_irq_trigger_type[irq] = IOAPIC_TRIGGER_LEVEL;
    } else {
        ioapic_irq_trigger_type[irq] = IOAPIC_TRIGGER_EDGE;
    }

    if (polarity == IOAPIC_POLARITY_LOW) {
        entry |= (1ULL << 13);
    }

    ioapic_write_reg(reg, (uint32_t)(entry & 0xFFFFFFFF));
    ioapic_write_reg(reg + 1, (uint32_t)(entry >> 32));
}

void ioapic_disable_irq(uint32_t irq)
{
    if (!ioapic_base || irq >= 128) return;

    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq * 2;
    uint32_t low = ioapic_read_reg(reg);
    ioapic_write_reg(reg, low | 0x10000);
}

void ioapic_eoi(uint32_t irq)
{
    (void)irq;
    lapic_eoi();
}

bool ioapic_is_level_triggered(uint32_t irq)
{
    if (irq >= 128) return false;
    return ioapic_irq_trigger_type[irq] == IOAPIC_TRIGGER_LEVEL;
}