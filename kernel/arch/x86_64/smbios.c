#include <arch/smbios.h>
#include <arch/memory.h>
#include <string.h>

static smbios_bios_t* smbios_bios_entry = NULL;
static smbios_system_t* smbios_system_entry = NULL;
static smbios_memory_t* smbios_memory_entries[SMBIOS_MAX_ENTRIES];
static uint8_t smbios_memory_count = 0;
static uint8_t smbios_entry_count = 0;

void smbios_init(void)
{
    /* In UEFI mode, the 0xF0000-0xFFFFF region may not be identity-mapped.
     * Skip SMBIOS table discovery to avoid page faults.
     * TODO: Get SMBIOS from EFI configuration table. */
    return;
}

smbios_bios_t* smbios_get_bios(void)
{
    return smbios_bios_entry;
}

smbios_system_t* smbios_get_system(void)
{
    return smbios_system_entry;
}

smbios_memory_t* smbios_get_memory(uint8_t index)
{
    if (index < smbios_memory_count) {
        return smbios_memory_entries[index];
    }
    return NULL;
}

uint8_t smbios_get_entry_count(void)
{
    return smbios_entry_count;
}
