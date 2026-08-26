#include <arch/watchdog.h>
#include <arch/hpet.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/interrupt.h>
#include <arch/acpi.h>
#include <string.h>

static watchdog_device_t watchdog_devices[WATCHDOG_MAX_DEVICES];
static int watchdog_count_val = 0;
static spinlock_t watchdog_global_lock = SPINLOCK_INIT;

static uint64_t watchdog_hpet_base = 0;
static uint64_t watchdog_hpet_period = 0;
static int watchdog_hpet_available = 0;

static uint16_t watchdog_acpi_pmbase = 0;
static int watchdog_acpi_available = 0;

static uint64_t watchdog_boot_tick = 0;

static inline uint64_t watchdog_get_ticks(void)
{
    if (watchdog_hpet_available && watchdog_hpet_base) {
        return hpet_get_ticks();
    }
    return 0;
}

static inline uint64_t watchdog_ticks_to_ms(uint64_t ticks)
{
    if (watchdog_hpet_period) {
        return (ticks * watchdog_hpet_period) / 1000000;
    }
    return 0;
}

static inline uint64_t watchdog_ms_to_ticks(uint64_t ms)
{
    if (watchdog_hpet_period) {
        return (ms * 1000000) / watchdog_hpet_period;
    }
    return 0;
}

static int watchdog_hpet_set_timer(uint32_t timeout_ms)
{
    if (!watchdog_hpet_available || !watchdog_hpet_base) return -1;

    uint64_t cmp_val = watchdog_ms_to_ticks((uint64_t)timeout_ms);
    uint64_t current = watchdog_get_ticks();
    uint64_t target = current + cmp_val;

    uint64_t timer_offset = WDT_HPET_TIMER_NUM * 0x20;
    uint64_t cmp_reg = watchdog_hpet_base + timer_offset + 0x08;
    uint64_t config_reg = watchdog_hpet_base + timer_offset + 0x10;

    *((volatile uint64_t*)cmp_reg) = target;

    uint64_t config = *((volatile uint64_t*)config_reg);
    config |= 0x04;
    config &= ~(uint64_t)0x02;
    config |= 0x01;
    *((volatile uint64_t*)config_reg) = config;

    return 0;
}

static void watchdog_hpet_stop_timer(void)
{
    if (!watchdog_hpet_available || !watchdog_hpet_base) return;

    uint64_t timer_offset = WDT_HPET_TIMER_NUM * 0x20;
    uint64_t config_reg = watchdog_hpet_base + timer_offset + 0x10;

    uint64_t config = *((volatile uint64_t*)config_reg);
    config &= ~(uint64_t)0x01;
    *((volatile uint64_t*)config_reg) = config;
}

static void watchdog_hpet_reload_timer(uint32_t timeout_ms)
{
    if (!watchdog_hpet_available || !watchdog_hpet_base) return;

    watchdog_hpet_stop_timer();
    watchdog_hpet_set_timer(timeout_ms);
}

static int watchdog_acpi_set_timer(uint32_t timeout_sec)
{
    if (!watchdog_acpi_available || !watchdog_acpi_pmbase) return -1;

    uint32_t sci_en = *((volatile uint32_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_SMI_EN));
    if (sci_en & WDT_ACPI_WDT_SMI_EN_LOCK) return -1;

    *((volatile uint16_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_PM1a_EVT)) = 0;

    uint32_t wdt_val = (uint32_t)timeout_sec;
    wdt_val <<= 16;
    wdt_val |= 0x0001;

    uint32_t evt_reg = *((volatile uint32_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_PM1a_EVT));
    evt_reg |= 0x2000;
    *((volatile uint32_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_PM1a_EVT)) = evt_reg;

    return 0;
}

static void watchdog_acpi_stop_timer(void)
{
    if (!watchdog_acpi_available || !watchdog_acpi_pmbase) return;

    uint32_t evt_reg = *((volatile uint32_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_PM1a_EVT));
    evt_reg &= ~0x2000;
    *((volatile uint32_t*)(watchdog_acpi_pmbase + WDT_ACPI_WDT_PM1a_EVT)) = evt_reg;
}

static int watchdog_software_set_timer(watchdog_device_t* wdt, uint32_t timeout_sec)
{
    if (!wdt) return -1;
    wdt->expires_tick = watchdog_get_ticks() + watchdog_ms_to_ticks((uint64_t)timeout_sec * 1000);
    return 0;
}

static void watchdog_software_stop_timer(watchdog_device_t* wdt)
{
    if (!wdt) return;
    wdt->expires_tick = 0;
}

int watchdog_register(watchdog_device_t* wdt)
{
    if (!wdt) return -1;
    if (watchdog_count_val >= WATCHDOG_MAX_DEVICES) return -1;

    spin_lock(&watchdog_global_lock);

    for (int i = 0; i < watchdog_count_val; i++) {
        if (watchdog_devices[i].registered &&
            strcmp(watchdog_devices[i].name, wdt->name) == 0) {
            spin_unlock(&watchdog_global_lock);
            return -1;
        }
    }

    spin_init(&wdt->lock);
    wdt->status = WDT_STATUS_STOPPED;
    wdt->last_ping_tick = 0;
    wdt->expires_tick = 0;
    wdt->registered = 1;

    if (wdt->timeout_sec == 0) {
        wdt->timeout_sec = WATCHDOG_DEFAULT_TIMEOUT;
    }
    if (wdt->min_timeout == 0) {
        wdt->min_timeout = 1;
    }
    if (wdt->max_timeout == 0) {
        wdt->max_timeout = 0xFFFFFFFF;
    }
    if (wdt->source == 0) {
        if (watchdog_hpet_available) {
            wdt->source = WDTSOURCE_HPET;
        } else if (watchdog_acpi_available) {
            wdt->source = WDTSOURCE_ACPI_WDT;
        } else {
            wdt->source = WDTSOURCE_SOFTWARE;
        }
    }

    watchdog_devices[watchdog_count_val] = *wdt;
    watchdog_count_val++;

    spin_unlock(&watchdog_global_lock);
    return 0;
}

int watchdog_unregister(watchdog_device_t* wdt)
{
    if (!wdt) return -1;

    spin_lock(&watchdog_global_lock);

    for (int i = 0; i < watchdog_count_val; i++) {
        if (&watchdog_devices[i] == wdt || strcmp(watchdog_devices[i].name, wdt->name) == 0) {
            watchdog_stop(&watchdog_devices[i]);
            watchdog_devices[i].registered = 0;

            for (int j = i; j < watchdog_count_val - 1; j++) {
                watchdog_devices[j] = watchdog_devices[j + 1];
            }
            watchdog_count_val--;

            spin_unlock(&watchdog_global_lock);
            return 0;
        }
    }

    spin_unlock(&watchdog_global_lock);
    return -1;
}

watchdog_device_t* watchdog_get_by_name(const char* name)
{
    if (!name) return NULL;

    spin_lock(&watchdog_global_lock);

    for (int i = 0; i < watchdog_count_val; i++) {
        if (watchdog_devices[i].registered &&
            strcmp(watchdog_devices[i].name, name) == 0) {
            spin_unlock(&watchdog_global_lock);
            return &watchdog_devices[i];
        }
    }

    spin_unlock(&watchdog_global_lock);
    return NULL;
}

int watchdog_start(watchdog_device_t* wdt)
{
    if (!wdt || !wdt->registered) return -1;

    spin_lock(&wdt->lock);

    if (wdt->timeout_sec < wdt->min_timeout) {
        wdt->timeout_sec = wdt->min_timeout;
    }
    if (wdt->timeout_sec > wdt->max_timeout) {
        wdt->timeout_sec = wdt->max_timeout;
    }

    wdt->last_ping_tick = watchdog_get_ticks();

    int ret = 0;
    switch (wdt->source) {
        case WDTSOURCE_HPET:
            ret = watchdog_hpet_set_timer(wdt->timeout_sec * 1000);
            break;
        case WDTSOURCE_ACPI_WDT:
            ret = watchdog_acpi_set_timer(wdt->timeout_sec);
            break;
        case WDTSOURCE_SOFTWARE:
            ret = watchdog_software_set_timer(wdt, wdt->timeout_sec);
            break;
        case WDTSOURCE_PCI_WDT:
            ret = -1;
            break;
        default:
            ret = -1;
            break;
    }

    if (ret == 0) {
        wdt->status = WDT_STATUS_RUNNING;
    }

    spin_unlock(&wdt->lock);
    return ret;
}

int watchdog_stop(watchdog_device_t* wdt)
{
    if (!wdt || !wdt->registered) return -1;

    spin_lock(&wdt->lock);

    switch (wdt->source) {
        case WDTSOURCE_HPET:
            watchdog_hpet_stop_timer();
            break;
        case WDTSOURCE_ACPI_WDT:
            watchdog_acpi_stop_timer();
            break;
        case WDTSOURCE_SOFTWARE:
            watchdog_software_stop_timer(wdt);
            break;
        default:
            break;
    }

    wdt->status = WDT_STATUS_STOPPED;
    wdt->expires_tick = 0;

    spin_unlock(&wdt->lock);
    return 0;
}

int watchdog_ping(watchdog_device_t* wdt)
{
    if (!wdt || !wdt->registered) return -1;

    spin_lock(&wdt->lock);

    if (wdt->status != WDT_STATUS_RUNNING) {
        spin_unlock(&wdt->lock);
        return -1;
    }

    wdt->last_ping_tick = watchdog_get_ticks();

    int ret = 0;
    switch (wdt->source) {
        case WDTSOURCE_HPET:
            watchdog_hpet_reload_timer(wdt->timeout_sec * 1000);
            break;
        case WDTSOURCE_ACPI_WDT:
            watchdog_acpi_stop_timer();
            ret = watchdog_acpi_set_timer(wdt->timeout_sec);
            break;
        case WDTSOURCE_SOFTWARE:
            ret = watchdog_software_set_timer(wdt, wdt->timeout_sec);
            break;
        default:
            break;
    }

    spin_unlock(&wdt->lock);
    return ret;
}

int watchdog_set_timeout(watchdog_device_t* wdt, uint32_t timeout)
{
    if (!wdt || !wdt->registered) return -1;

    spin_lock(&wdt->lock);

    if (timeout < wdt->min_timeout) timeout = wdt->min_timeout;
    if (timeout > wdt->max_timeout) timeout = wdt->max_timeout;

    wdt->timeout_sec = timeout;

    if (wdt->status == WDT_STATUS_RUNNING) {
        spin_unlock(&wdt->lock);
        return watchdog_ping(wdt);
    }

    spin_unlock(&wdt->lock);
    return 0;
}

int watchdog_get_timeout(watchdog_device_t* wdt, uint32_t* timeout)
{
    if (!wdt || !timeout) return -1;
    *timeout = wdt->timeout_sec;
    return 0;
}

uint8_t watchdog_get_status(watchdog_device_t* wdt)
{
    if (!wdt) return WDT_STATUS_STOPPED;
    return wdt->status;
}

void watchdog_init_hpet(void)
{
    if (watchdog_hpet_base) return;

    uint64_t base = 0;
    void* hpet_info = acpi_get_table(0x54455048); /* "HPET" */
    if (!hpet_info) return;

    base = *(uint64_t*)((uint8_t*)hpet_info + 0x28);

    if (!base) return;

    uint32_t period = *((volatile uint32_t*)(base + 0x04));
    if (period == 0 || period > 0x05F5E100) return;

    watchdog_hpet_base = base;
    watchdog_hpet_period = (uint64_t)period;
    watchdog_hpet_available = 1;
}

void watchdog_init_acpi_wdt(void)
{
    if (watchdog_acpi_pmbase) return;

    void* fadt = acpi_get_table(0x50434146); /* "FACP" */
    if (!fadt) return;

    uint16_t pm1a_evt = *(uint16_t*)((uint8_t*)fadt + 0x28);
    if (pm1a_evt == 0) return;

    watchdog_acpi_pmbase = pm1a_evt;

    uint32_t sci_en = *((volatile uint32_t*)(pm1a_evt + WDT_ACPI_WDT_SMI_EN));
    if (sci_en & WDT_ACPI_WDT_SMI_EN_LOCK) {
        watchdog_acpi_available = 0;
        return;
    }

    watchdog_acpi_available = 1;
}

int watchdog_init(void)
{
    watchdog_count_val = 0;
    spin_init(&watchdog_global_lock);

    for (int i = 0; i < WATCHDOG_MAX_DEVICES; i++) {
        watchdog_devices[i].registered = 0;
    }

    watchdog_init_hpet();
    watchdog_init_acpi_wdt();

    watchdog_boot_tick = watchdog_get_ticks();

    return 0;
}

int watchdog_count(void)
{
    return watchdog_count_val;
}

void watchdog_check_all(void)
{
    spin_lock(&watchdog_global_lock);

    uint64_t current = watchdog_get_ticks();

    for (int i = 0; i < watchdog_count_val; i++) {
        watchdog_device_t* wdt = &watchdog_devices[i];
        if (!wdt->registered) continue;
        if (wdt->status != WDT_STATUS_RUNNING) continue;

        if (wdt->source == WDTSOURCE_SOFTWARE && wdt->expires_tick) {
            if (current >= wdt->expires_tick) {
                wdt->status = WDT_STATUS_EXPIRED;

                switch (wdt->action) {
                    case WDT_ACTION_RESET:
                        spin_unlock(&watchdog_global_lock);
                        __asm__ volatile ("cli");
                        for (;;) {
                            uint8_t val = 0xFE;
                            outb(0x64, val);
                        }
                        break;
                    case WDT_ACTION_PANIC:
                        spin_unlock(&watchdog_global_lock);
                        __asm__ volatile ("int $0x13");
                        break;
                    case WDT_ACTION_CALLBACK:
                        if (wdt->callback) {
                            wdt->callback(wdt->callback_data);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    spin_unlock(&watchdog_global_lock);
}
