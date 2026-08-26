

#include <arch/acpi_madt.h>
#include <arch/memory.h>
#include <arch/ioapic.h>
#include <string.h>

static int __rsdp_checksum(const void* data, uint64_t len)
{
    uint8_t sum = 0;
    const uint8_t* p = (const uint8_t*)data;
    for (uint64_t i = 0; i < len; i++) {
        sum += p[i];
    }
    return (sum == 0);
}

int acpi_verify_rsdp(const rsdp_t* rsdp)
{
    if (!rsdp) return -1;
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) return -1;
    if (!__rsdp_checksum(rsdp, sizeof(rsdp_t))) return -1;
    return 0;
}

const rsdp_t* acpi_find_rsdp(void)
{

    uint16_t* ebda_seg = (uint16_t*)0x40E;
    uint64_t ebda_base = (uint64_t)(*ebda_seg) << 4;

    if (ebda_base && ebda_base < 0x100000) {
        for (uint64_t addr = ebda_base; addr < ebda_base + 1024; addr += 16) {
            const rsdp_t* rsdp = (const rsdp_t*)addr;
            if (acpi_verify_rsdp(rsdp) == 0) return rsdp;
        }
    }

    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const rsdp_t* rsdp = (const rsdp_t*)addr;
        if (acpi_verify_rsdp(rsdp) == 0) return rsdp;
    }

    return NULL;
}

static const void* __find_acpi_table(const void* rsdt, uint64_t rsdt_len, int is_xsdt, const char* sig)
{
    uint32_t entry_count;

    if (is_xsdt) {
        entry_count = (rsdt_len - sizeof(acpi_rsdt_t)) / 8;
    } else {
        entry_count = (rsdt_len - sizeof(acpi_rsdt_t)) / 4;
    }

    for (uint32_t i = 0; i < entry_count; i++) {
        uint64_t entry_addr;
        if (is_xsdt) {
            entry_addr = ((const uint64_t*)((const uint8_t*)rsdt + sizeof(acpi_rsdt_t)))[i];
        } else {
            entry_addr = ((const uint32_t*)((const uint8_t*)rsdt + sizeof(acpi_rsdt_t)))[i];
        }

        const uint8_t* table = (const uint8_t*)entry_addr;
        if (table && memcmp(table, sig, 4) == 0) {
            return table;
        }
    }

    return NULL;
}

int acpi_madt_parse(acpi_madt_info_t* info)
{
    if (!info) return -1;
    memset(info, 0, sizeof(acpi_madt_info_t));

    const rsdp_t* rsdp = acpi_find_rsdp();
    if (!rsdp) return -1;

    int is_xsdt = (rsdp->revision >= 2);

    const uint8_t* rsdt;
    if (is_xsdt) {

        uint64_t xsdt_addr = *(const uint64_t*)((const uint8_t*)rsdp + 24);
        rsdt = (const uint8_t*)xsdt_addr;
    } else {
        uint64_t rsdt_addr = rsdp->rsdt_address;
        rsdt = (const uint8_t*)rsdt_addr;
    }

    if (!rsdt) return -1;

    uint32_t rsdt_len = *(const uint32_t*)(rsdt + 4);
    const acpi_madt_t* madt = (const acpi_madt_t*)__find_acpi_table(rsdt, rsdt_len, is_xsdt, "APIC");
    if (!madt) return -1;

    info->lapic_addr = madt->lapic_addr;

    uint32_t offset = sizeof(acpi_madt_t) - sizeof(uint8_t[0]);
    uint32_t total_len = madt->length;

    while (offset + 2 <= total_len) {
        uint8_t type = madt->entries[offset];
        uint8_t length = madt->entries[offset + 1];

        if (offset + length > total_len) break;

        switch (type) {
            case MADT_ENTRY_LAPIC: {
                if (length >= sizeof(madt_lapic_entry_t) && info->cpu_count < ACPI_MAX_CPUS) {
                    const madt_lapic_entry_t* entry = (const madt_lapic_entry_t*)&madt->entries[offset];
                    acpi_cpu_info_t* cpu = &info->cpus[info->cpu_count];
                    cpu->acpi_id = entry->acpi_processor_id;
                    cpu->apic_id = entry->apic_id;
                    cpu->enabled = (entry->flags & 1) != 0;
                    if (cpu->enabled) info->cpu_count++;
                }
                break;
            }
            case MADT_ENTRY_IOAPIC: {
                if (length >= sizeof(madt_ioapic_entry_t)) {
                    const madt_ioapic_entry_t* entry = (const madt_ioapic_entry_t*)&madt->entries[offset];
                    ioapic_init(entry->ioapic_addr);
                }
                break;
            }
        }

        offset += length;
    }

    return 0;
}
