#ifndef ARCH_X86_64_ACPI_H
#define ARCH_X86_64_ACPI_H

#include <arch/types.h>

#define ACPI_MAX_TABLES 32

typedef struct {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

typedef struct {
    acpi_header_t header;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdt_t;

typedef struct {
    acpi_header_t header;
    uint64_t xsdt_address;
} __attribute__((packed)) xsdt_t;

typedef struct {
    acpi_header_t header;
    uint32_t lmbr_base;
    uint8_t lmbr_size;
    uint8_t flags;
} __attribute__((packed)) mcfg_t;

void acpi_init(void);
acpi_header_t* acpi_get_table(uint32_t signature);
uint8_t acpi_get_table_count(void);
uint32_t acpi_get_lmbr_base(void);
uint8_t acpi_get_lmbr_size(void);

#endif
