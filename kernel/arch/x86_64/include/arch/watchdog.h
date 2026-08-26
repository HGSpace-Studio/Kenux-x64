#ifndef ARCH_X86_64_WATCHDOG_H
#define ARCH_X86_64_WATCHDOG_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define WATCHDOG_MAX_DEVICES     8
#define WATCHDOG_NAME_MAX        32
#define WATCHDOG_DEFAULT_TIMEOUT 30

#define WDTSOURCE_HPET           0
#define WDTSOURCE_ACPI_WDT       1
#define WDTSOURCE_SOFTWARE       2
#define WDTSOURCE_PCI_WDT        3

#define WDT_ACTION_NONE          0
#define WDT_ACTION_RESET         1
#define WDT_ACTION_PANIC         2
#define WDT_ACTION_CALLBACK      3

#define WDT_STATUS_STOPPED       0
#define WDT_STATUS_RUNNING        1
#define WDT_STATUS_EXPIRED       2

#define WDT_ACPI_WDT_PM1a_EVT    0x60
#define WDT_ACPI_WDT_SMI_EN      0xB0
#define WDT_ACPI_WDT_SMI_EN_LOCK 0x08
#define WDT_ACPI_WDT_NMI_EN      0x80

#define WDT_HPET_TIMER_NUM       2
#define WDT_HPET_MIN_PERIOD_MS   100

typedef void (*watchdog_callback_t)(void* data);

typedef struct watchdog_device {
    uint32_t timeout_sec;
    uint32_t min_timeout;
    uint32_t max_timeout;
    uint8_t source;
    uint8_t status;
    uint8_t action;
    uint32_t last_ping_tick;
    uint64_t expires_tick;
    char name[WATCHDOG_NAME_MAX];
    watchdog_callback_t callback;
    void* callback_data;
    spinlock_t lock;
    int registered;
} watchdog_device_t;

int watchdog_register(watchdog_device_t* wdt);
int watchdog_unregister(watchdog_device_t* wdt);
watchdog_device_t* watchdog_get_by_name(const char* name);

int watchdog_start(watchdog_device_t* wdt);
int watchdog_stop(watchdog_device_t* wdt);
int watchdog_ping(watchdog_device_t* wdt);

int watchdog_set_timeout(watchdog_device_t* wdt, uint32_t timeout);
int watchdog_get_timeout(watchdog_device_t* wdt, uint32_t* timeout);
uint8_t watchdog_get_status(watchdog_device_t* wdt);

void watchdog_init_hpet(void);
void watchdog_init_acpi_wdt(void);
int watchdog_init(void);

int watchdog_count(void);
void watchdog_check_all(void);

#endif
