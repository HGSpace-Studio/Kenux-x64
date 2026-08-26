#ifndef ARCH_X86_64_SMBIOS_H
#define ARCH_X86_64_SMBIOS_H

#include <arch/types.h>

#define SMBIOS_MAX_ENTRIES 256

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t handle;
} __attribute__((packed)) smbios_header_t;

typedef struct {
    smbios_header_t header;
    uint8_t vendor;
    uint8_t version;
    uint16_t start_segment;
    uint8_t release_date;
    uint8_t rom_size;
    uint64_t start;
} __attribute__((packed)) smbios_bios_t;

typedef struct {
    smbios_header_t header;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t version;
    uint8_t serial;
    uint8_t uuid[16];
    uint8_t wake_up_type;
    uint8_t sku;
    uint8_t family;
} __attribute__((packed)) smbios_system_t;

typedef struct {
    smbios_header_t header;
    uint8_t manufacturer;
    uint8_t version;
    uint8_t serial;
    uint8_t asset_tag;
    uint8_t feature_flags;
    uint8_t location;
    uint16_t start_address;
    uint8_t type;
    uint8_t width;
    uint8_t density;
} __attribute__((packed)) smbios_memory_t;

void smbios_init(void);
smbios_bios_t* smbios_get_bios(void);
smbios_system_t* smbios_get_system(void);
smbios_memory_t* smbios_get_memory(uint8_t index);
uint8_t smbios_get_entry_count(void);

#endif
