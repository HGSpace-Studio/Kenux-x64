

#ifndef ARCH_APIC_H
#define ARCH_APIC_H

#include <arch/types.h>

#define LAPIC_BASE_DEFAULT   0xFEE00000ULL

#define LAPIC_ID             0x020
#define LAPIC_VER            0x030

#define LAPIC_TPR            0x080
#define LAPIC_APR            0x090
#define LAPIC_PPR            0x0A0

#define LAPIC_EOI            0x0B0

#define LAPIC_LDR            0x0D0
#define LAPIC_DFR            0x0E0

#define LAPIC_SVR            0x0F0

#define LAPIC_ISR_BASE       0x100
#define LAPIC_TMR_BASE       0x180
#define LAPIC_IRR_BASE       0x200

#define LAPIC_ESR            0x280

#define LAPIC_ICR0           0x300
#define LAPIC_ICR1           0x310

#define LAPIC_LVT_TIMER      0x320
#define LAPIC_LVT_THERMAL    0x330
#define LAPIC_LVT_PERF       0x340
#define LAPIC_LVT_LINT0      0x350
#define LAPIC_LVT_LINT1      0x360
#define LAPIC_LVT_ERROR      0x370

#define LAPIC_TIMER_INIT     0x380
#define LAPIC_TIMER_CURR     0x390
#define LAPIC_TIMER_DIV      0x3E0

#define LAPIC_SVR_ENABLE     0x100
#define LAPIC_SVR_FOCUS      0x200

#define LAPIC_TIMER_ONESHOT  0x00000
#define LAPIC_TIMER_PERIODIC 0x20000
#define LAPIC_TIMER_TSCDEAD  0x40000

#define LAPIC_TIMER_MASKED   0x10000

#define LAPIC_ICR_ASSERT     (1u << 14)
#define LAPIC_ICR_DEASSERT   (0u << 14)
#define LAPIC_ICR_EDGE       (0u << 15)
#define LAPIC_ICR_LEVEL      (1u << 15)
#define LAPIC_ICR_FIXED      0x00000
#define LAPIC_ICR_LOWEST     0x00100
#define LAPIC_ICR_SMI        0x00200
#define LAPIC_ICR_NMI        0x00400
#define LAPIC_ICR_INIT       0x00500
#define LAPIC_ICR_STARTUP    0x00600
#define LAPIC_ICR_BUSY       (1u << 12)

#define LAPIC_LVT_MASKED     0x10000
#define LAPIC_LVT_EDGE       0x00000
#define LAPIC_LVT_LEVEL      0x08000
#define LAPIC_LVT_FIXED      0x00000
#define LAPIC_LVT_SMI        0x02000
#define LAPIC_LVT_NMI        0x04000
#define LAPIC_LVT_EXTINT     0x07000

#define LAPIC_DIV_1          0x00
#define LAPIC_DIV_2          0x01
#define LAPIC_DIV_4          0x02
#define LAPIC_DIV_8          0x03
#define LAPIC_DIV_16         0x08
#define LAPIC_DIV_32         0x09
#define LAPIC_DIV_64         0x0A
#define LAPIC_DIV_128        0x0B

#define IOAPIC_BASE_DEFAULT  0xFEC00000ULL
#define IOAPIC_REG_ID        0x00
#define IOAPIC_REG_VER       0x01
#define IOAPIC_REG_ARB       0x02
#define IOAPIC_REG_REDIR_BASE 0x10

#define IOAPIC_TRIGGER_EDGE  0
#define IOAPIC_TRIGGER_LEVEL 1
#define IOAPIC_POLARITY_HIGH 0
#define IOAPIC_POLARITY_LOW  1

void lapic_init(uint64_t base_addr);

uint32_t lapic_read(uint32_t reg);

void lapic_write(uint32_t reg, uint32_t value);

void lapic_eoi(void);

uint32_t lapic_id(void);

void lapic_send_ipi(uint32_t target_apic_id, uint32_t vector);

void lapic_timer_calibrate(void);

void lapic_setup_timer(uint32_t vector, uint32_t ms);

void lapic_lvt_mask(uint32_t reg);

void lapic_lvt_unmask(uint32_t reg);

uint32_t lapic_get_version(void);

void lapic_mask_irq(uint32_t vector);
void lapic_unmask_irq(uint32_t vector);
uint32_t irq_to_vector(uint32_t irq);

void ioapic_init(void);
void ioapic_enable_irq(uint32_t irq, int trigger, int polarity);
void ioapic_disable_irq(uint32_t irq);
void ioapic_eoi(uint32_t irq);
bool ioapic_is_level_triggered(uint32_t irq);

void disable_8259A(void);

#endif