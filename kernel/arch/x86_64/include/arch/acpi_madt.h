

#ifndef ARCH_ACPI_MADT_H
#define ARCH_ACPI_MADT_H

#include <arch/types.h>

#define MADT_ENTRY_LAPIC     0
#define MADT_ENTRY_IOAPIC    1
#define MADT_ENTRY_INT_SRC   2
#define MADT_ENTRY_LAPIC_NMI 3
#define MADT_ENTRY_INT_OVERRIDE 4

typedef struct {
    uint8_t  type;
    uint8_t  length;
    uint8_t  acpi_processor_id;
    uint8_t  apic_id;
    uint32_t flags;
} __attribute__((packed)) madt_lapic_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  length;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed)) madt_ioapic_entry_t;

typedef struct {
    uint8_t  signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  oem_id[6];
    uint8_t  oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t  entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t acpi_id;
    uint8_t apic_id;
    int     enabled;
} acpi_cpu_info_t;

#define ACPI_MAX_CPUS  64

typedef struct {
    acpi_cpu_info_t cpus[ACPI_MAX_CPUS];
    uint32_t        cpu_count;
    uint32_t        lapic_addr;
} acpi_madt_info_t;

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_t;

typedef struct {
    uint8_t  signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint64_t entries[];
} __attribute__((packed)) acpi_rsdt_t;

int acpi_madt_parse(acpi_madt_info_t* info);

int acpi_verify_rsdp(const rsdp_t* rsdp);

const rsdp_t* acpi_find_rsdp(void);

#endif
