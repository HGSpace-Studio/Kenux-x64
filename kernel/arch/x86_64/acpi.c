#include <arch/acpi.h>
#include <arch/memory.h>
#include <string.h>

static acpi_header_t* acpi_tables[ACPI_MAX_TABLES];
static uint8_t acpi_table_count = 0;
static mcfg_t* acpi_mcfg = NULL;

static uint8_t acpi_checksum(uint8_t* data, uint32_t length)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

/* Scan a memory region for the RSDP signature.
 * Returns pointer to RSDP if found, NULL otherwise.
 * max_iterations limits the scan to prevent runaway loops. */
static uint8_t* scan_for_rsdp(uint32_t start, uint32_t end)
{
    /* Clamp to reasonable physical addresses */
    if (start < 0x1000) start = 0x1000;
    if (end > 0x100000) end = 0x100000;
    if (start >= end) return NULL;

    for (uint32_t addr = start; addr < end; addr += 16) {
        uint8_t* ptr = (uint8_t*)(uintptr_t)addr;
        if (memcmp(ptr, "RSD PTR ", 8) == 0) {
            /* Validate checksum (first 20 bytes for v1, full for v2) */
            uint8_t csum = acpi_checksum(ptr, 20);
            if (csum == 0) {
                return ptr;
            }
        }
    }
    return NULL;
}

void acpi_init(void)
{
    /* In UEFI mode, low memory (0x40E EBDA, 0xE0000-0xFFFFF ROM area)
     * may not be identity-mapped by the firmware's page tables.
     * Scanning these regions causes thousands of page faults.
     *
     * TODO: Get RSDP from EFI configuration table (passed by bootloader).
     * For now, skip ACPI table discovery entirely — the system runs
     * fine without ACPI for basic GUI operation. */
    return;
}

acpi_header_t* acpi_get_table(uint32_t signature)
{
    for (uint8_t i = 0; i < acpi_table_count; i++) {
        if (acpi_tables[i]->signature == signature) {
            return acpi_tables[i];
        }
    }
    return NULL;
}

uint8_t acpi_get_table_count(void)
{
    return acpi_table_count;
}

uint32_t acpi_get_lmbr_base(void)
{
    if (acpi_mcfg) {
        return acpi_mcfg->lmbr_base;
    }
    return 0;
}

uint8_t acpi_get_lmbr_size(void)
{
    if (acpi_mcfg) {
        return acpi_mcfg->lmbr_size;
    }
    return 0;
}
