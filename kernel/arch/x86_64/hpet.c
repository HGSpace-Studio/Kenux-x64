#include <arch/types.h>

typedef struct {
    volatile u64 capabilities;
    volatile u64 reserved1;
    volatile u64 config;
    volatile u64 reserved2;
    volatile u64 interrupt_status;
    volatile u64 reserved3;
    volatile u64 reserved4;
    volatile u64 reserved5;
    volatile u64 main_counter;
    volatile u64 reserved6;
    struct {
        volatile u64 config;
        volatile u64 cmp;
        volatile u64 reserved;
    } timers[32];
} hpet_t;

#include <arch/hpet.h>
#include <arch/memory.h>
#include <string.h>

static hpet_t* hpet = NULL;
static u64 hpet_frequency = 0;
static u64 hpet_period_fs = 0;
static u64 hpet_boot_ns = 0;
static int hpet_enabled = 0;

void hpet_init(void)
{
    hpet = (hpet_t*)HPET_BASE_DEFAULT;

    u64 cap = hpet->capabilities;
    u64 period = (cap >> 32) & 0xFFFFFFFF;
    if (period == 0 || period > 100000000) {
        hpet = NULL;
        return;
    }

    hpet_period_fs = period;
    hpet_frequency = 1000000000000ULL / period;

    hpet->config &= ~HPET_CFG_ENABLE;
    hpet->main_counter = 0;
    hpet->config |= HPET_CFG_ENABLE;

    hpet_enabled = 1;
    hpet_boot_ns = 0;
}

u64 hpet_get_ticks(void)
{
    if (!hpet) return 0;
    return hpet->main_counter;
}

u64 hpet_get_frequency(void)
{
    return hpet_frequency;
}

u64 hpet_get_ns(void)
{
    if (!hpet || !hpet_period_fs) return 0;
    u64 mcv = hpet->main_counter;
    return (mcv * hpet_period_fs) / 1000000ULL;
}

u64 hpet_get_boot_ns(void)
{
    return hpet_get_ns() + hpet_boot_ns;
}

void hpet_sleep(u64 ms)
{
    if (!hpet) return;
    u64 target = hpet_get_ns() + ms * 1000000ULL;
    while (hpet_get_ns() < target) {
        __asm__ volatile("pause");
    }
}

void hpet_nsleep(u64 ns)
{
    if (!hpet) return;
    u64 target = hpet_get_ns() + ns;
    while (hpet_get_ns() < target) {
        __asm__ volatile("pause");
    }
}

void hpet_set_periodic(u32 hz)
{
    if (!hpet) return;
    u64 period = hpet_frequency / hz;
    hpet->timers[0].config = 0;
    hpet->timers[0].cmp = hpet->main_counter + period;
    hpet->timers[0].config = HPET_TMR_CFG_PERIODIC | HPET_TMR_CFG_INT_ENABLE;
}

void hpet_enable(void)
{
    if (!hpet) return;
    hpet->config |= HPET_CFG_ENABLE;
    hpet_enabled = 1;
}

void hpet_disable(void)
{
    if (!hpet) return;
    hpet->config &= ~HPET_CFG_ENABLE;
    hpet_enabled = 0;
}