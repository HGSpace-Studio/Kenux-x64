#include <arch/efi_runtime.h>
#include <arch/efi.h>
#include <arch/spinlock.h>
#include <arch/io.h>
#include <string.h>

static efi_rt_services_t* efi_rt = NULL;
static int efi_rt_initialized = 0;
static spinlock_t efi_rt_lock = SPINLOCK_INIT;

#define EFI_RUNTIME_SERVICES_SIGNATURE_VAL 0x5652455354555200ULL
#define EFI_RUNTIME_SERVICES_REVISION_2    (2 << 16)

void efi_runtime_init(void* system_table)
{
    if (!system_table) return;

    efi_system_table_t* st = (efi_system_table_t*)system_table;

    uint64_t rt_ptr = st->runtime_services;
    if (!rt_ptr) return;

    efi_rt_services_t* rs = (efi_rt_services_t*)rt_ptr;
    if (rs->signature != EFI_RUNTIME_SERVICES_SIGNATURE_VAL) return;

    uint32_t major_rev = (rs->revision >> 16) & 0xFFFF;
    if (major_rev < 2) return;

    efi_rt = rs;
    efi_rt_initialized = 1;
}

int efi_runtime_ready(void)
{
    return efi_rt_initialized;
}

int efi_rt_get_variable(uint16_t* name, efi_rt_guid_t* guid,
                          uint32_t* attr, uint64_t* data_size, void* data)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!name || !guid || !data_size) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->get_variable(name, guid, attr, data_size, data);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_set_variable(uint16_t* name, efi_rt_guid_t* guid,
                          uint32_t attr, uint64_t data_size, void* data)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!name || !guid) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->set_variable(name, guid, attr, data_size, data);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_get_next_variable_name(uint64_t* name_size, uint16_t* name, efi_rt_guid_t* guid)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!name_size || !name || !guid) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->get_next_variable_name(name_size, name, guid);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_get_time(efi_rt_time_t* time, efi_rt_time_cap_t* cap)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!time) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->get_time(time, cap);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_set_time(efi_rt_time_t* time)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!time) return -1;

    if (time->year < 2000 || time->year > 9999) return -1;
    if (time->month < 1 || time->month > 12) return -1;
    if (time->day < 1 || time->day > 31) return -1;
    if (time->hour > 23) return -1;
    if (time->minute > 59) return -1;
    if (time->second > 59) return -1;
    if (time->nanoseconds > 999999999) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->set_time(time);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_get_wakeup_time(uint8_t* enabled, uint8_t* pending, efi_rt_time_t* time)
{
    if (!efi_rt_initialized || !efi_rt) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->get_wakeup_time(enabled, pending, time);

    spin_unlock(&efi_rt_lock);
    return ret;
}

int efi_rt_set_wakeup_time(uint8_t enable, efi_rt_time_t* time)
{
    if (!efi_rt_initialized || !efi_rt) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->set_wakeup_time(enable, time);

    spin_unlock(&efi_rt_lock);
    return ret;
}

void efi_rt_reset_system(uint32_t reset_type, int status, uint64_t data_size, uint16_t* data)
{
    if (!efi_rt_initialized || !efi_rt) {
        uint8_t shutdown_port = 0x64;
        uint8_t value = 0xFE;

        for (volatile int i = 0; i < 100000; i++) {
            if (!(inb(shutdown_port) & 0x02)) break;
        }
        outb(shutdown_port, value);

        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );

        return;
    }

    efi_rt->reset_system(reset_type, status, data_size, data);

    __asm__ volatile (
        "cli\n"
        "hlt\n"
    );
}

int efi_rt_query_variable_info(uint32_t attributes,
                                 uint64_t* max_storage,
                                 uint64_t* remaining_storage,
                                 uint64_t* max_size)
{
    if (!efi_rt_initialized || !efi_rt) return -1;
    if (!max_storage || !remaining_storage || !max_size) return -1;

    if (!efi_rt->query_variable_info) return -1;

    spin_lock(&efi_rt_lock);

    int ret = efi_rt->query_variable_info(attributes, max_storage, remaining_storage, max_size);

    spin_unlock(&efi_rt_lock);
    return ret;
}
