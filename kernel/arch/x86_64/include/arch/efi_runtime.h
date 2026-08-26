#ifndef ARCH_X86_64_EFI_RUNTIME_H
#define ARCH_X86_64_EFI_RUNTIME_H

#include <arch/types.h>

#define EFI_VARIABLE_NON_VOLATILE                          0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                    0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                        0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                 0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS            0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x00000020
#define EFI_VARIABLE_APPEND_WRITE                          0x00000040
#define EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS          0x00000080

#define EFI_GLOBAL_VARIABLE_GUID \
    {0x8BE4DF61, 0x93CA, 0x11D2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C}}

#define EFI_TIME_ADJUST_DAYLIGHT 0x01
#define EFI_TIME_IN_DAYLIGHT     0x02

#define EFI_UNSPECIFIED_TIMEZONE 0x07FF

#define EFI_NVME_NAMESPACE_GUID \
    {0xD10C62E3, 0x95E2, 0x44B5, {0xBD, 0x45, 0x96, 0x38, 0x18, 0x55, 0xDD, 0xB4}}

#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI_GUID \
    {0xFB81AE30, 0x6212, 0x4B3E, {0xB2, 0x95, 0x92, 0x75, 0x49, 0x23, 0xB3, 0xF6}}

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_rt_guid_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t pad1;
    uint32_t nanoseconds;
    int16_t timezone;
    uint8_t daylight;
    uint8_t pad2;
} efi_rt_time_t;

typedef struct {
    uint32_t resolution;
    uint32_t accuracy;
    uint8_t sets_to_zero;
} efi_rt_time_cap_t;

typedef struct efi_rt_services {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;

    int (*get_time)(efi_rt_time_t* time, efi_rt_time_cap_t* capabilities);
    int (*set_time)(efi_rt_time_t* time);
    int (*get_wakeup_time)(uint8_t* enabled, uint8_t* pending, efi_rt_time_t* time);
    int (*set_wakeup_time)(uint8_t enable, efi_rt_time_t* time);

    void (*set_virtual_address_map)(uint64_t memory_map_size,
                                     uint32_t descriptor_version,
                                     uint64_t descriptor_size,
                                     void* virtual_map);
    uint64_t convert_pointer;

    int (*get_variable)(uint16_t* variable_name,
                         efi_rt_guid_t* vendor_guid,
                         uint32_t* attributes,
                         uint64_t* data_size,
                         void* data);
    int (*get_next_variable_name)(uint64_t* name_size, uint16_t* name, efi_rt_guid_t* guid);

    int (*set_variable)(uint16_t* variable_name,
                         efi_rt_guid_t* vendor_guid,
                         uint32_t attributes,
                         uint64_t data_size,
                         void* data);

    uint64_t get_next_high_monotonic_count;

    void (*reset_system)(uint32_t reset_type,
                         int reset_status,
                         uint64_t data_size,
                         uint16_t* reset_data);

    void (*update_capsule)(uint64_t* capsule_list, uint64_t capsule_count,
                            uint64_t scatter_gather_list);
    int (*query_capsule_capabilities)(uint64_t* capsule_list, uint64_t capsule_count,
                                       uint64_t* maximum_capsule_size,
                                       uint32_t* reset_type);
    int (*query_variable_info)(uint32_t attributes,
                                 uint64_t* maximum_variable_storage_size,
                                 uint64_t* remaining_variable_storage_size,
                                 uint64_t* maximum_variable_size);
} efi_rt_services_t;

void efi_runtime_init(void* system_table);

int efi_rt_get_variable(uint16_t* name, efi_rt_guid_t* guid,
                          uint32_t* attr, uint64_t* data_size, void* data);
int efi_rt_set_variable(uint16_t* name, efi_rt_guid_t* guid,
                          uint32_t attr, uint64_t data_size, void* data);
int efi_rt_get_next_variable_name(uint64_t* name_size, uint16_t* name, efi_rt_guid_t* guid);

int efi_rt_get_time(efi_rt_time_t* time, efi_rt_time_cap_t* cap);
int efi_rt_set_time(efi_rt_time_t* time);
int efi_rt_get_wakeup_time(uint8_t* enabled, uint8_t* pending, efi_rt_time_t* time);
int efi_rt_set_wakeup_time(uint8_t enable, efi_rt_time_t* time);

void efi_rt_reset_system(uint32_t reset_type, int status, uint64_t data_size, uint16_t* data);

int efi_rt_query_variable_info(uint32_t attributes,
                                 uint64_t* max_storage,
                                 uint64_t* remaining_storage,
                                 uint64_t* max_size);

int efi_runtime_ready(void);

#endif
