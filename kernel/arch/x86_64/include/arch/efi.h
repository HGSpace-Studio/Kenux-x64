#ifndef ARCH_X86_64_EFI_H
#define ARCH_X86_64_EFI_H

#include <arch/types.h>

#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249ULL
#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42ULL
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553545552ULL

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR 1
#define EFI_INVALID_PARAMETER 2
#define EFI_UNSUPPORTED 3
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_NOT_FOUND 14
#define EFI_NOT_READY 6

#define EFI_FILE_MODE_READ 0x0000000000000001
#define EFI_FILE_MODE_WRITE 0x0000000000000002
#define EFI_FILE_MODE_CREATE 0x8000000000000000

#define EFI_FILE_READ_ONLY 0x0000000000000001
#define EFI_FILE_HIDDEN 0x0000000000000002
#define EFI_FILE_SYSTEM 0x0000000000000004
#define EFI_FILE_RESERVED 0x0000000000000008
#define EFI_FILE_DIRECTORY 0x0000000000000010
#define EFI_FILE_ARCHIVE 0x0000000000000020

#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL 0x00000002
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL 0x00000004
#define EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER 0x00000008
#define EFI_OPEN_PROTOCOL_BY_DRIVER 0x00000010
#define EFI_OPEN_PROTOCOL_EXCLUSIVE 0x00000020

#define EFI_MEMORY_UC 0x0000000000000001
#define EFI_MEMORY_WC 0x0000000000000002
#define EFI_MEMORY_WT 0x0000000000000004
#define EFI_MEMORY_WB 0x0000000000000008
#define EFI_MEMORY_UCE 0x0000000000000010
#define EFI_MEMORY_WP 0x0000000000001000
#define EFI_MEMORY_RP 0x0000000000002000
#define EFI_MEMORY_XP 0x0000000000004000
#define EFI_MEMORY_NV 0x0000000000008000
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000
#define EFI_MEMORY_RO 0x0000000000020000
#define EFI_MEMORY_RUNTIME 0x8000000000000000

#define EFI_LOADER_CODE 1
#define EFI_LOADER_DATA 2
#define EFI_BOOT_SERVICES_CODE 3
#define EFI_BOOT_SERVICES_DATA 4
#define EFI_RUNTIME_SERVICES_CODE 5
#define EFI_RUNTIME_SERVICES_DATA 6
#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_UNUSABLE_MEMORY 8
#define EFI_ACPI_RECLAIM_MEMORY 9
#define EFI_ACPI_MEMORY_NVS 10
#define EFI_MEMORY_MAPPED_IO 11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE 13
#define EFI_PERSISTENT_MEMORY 14
#define EFI_MAX_MEMORY_TYPE 15

#define EFI_RESET_COLD 0
#define EFI_RESET_WARM 1
#define EFI_RESET_SHUTDOWN 2

#define EFI_PCI_IO_PROTOCOL_GUID \
    {0x4CF5B200, 0x8B8B, 0x42ca, {0x98, 0x3C, 0x8D, 0x56, 0x6F, 0xB7, 0x1F, 0xE0}}

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042A9DE, 0x23DC, 0x4A38, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}}

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    {0x387477C2, 0x69C7, 0x11d2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_UGA_DRAW_PROTOCOL_GUID \
    {0x982C298B, 0xF4FA, 0x41CB, {0xB8, 0x38, 0x77, 0xAA, 0x68, 0x8F, 0xB8, 0x39}}

#define EFI_MEMORY_DESCRIPTOR_VERSION 1

typedef uint64_t efi_status_t;
typedef void* efi_handle_t;
typedef uint64_t efi_physical_address_t;
typedef uint64_t efi_virtual_address_t;
typedef uint16_t char16_t;

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

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
} efi_time_t;

typedef struct {
    uint32_t resolution;
    uint32_t accuracy;
    uint8_t sets_to_zero;
} efi_time_cap_t;

typedef struct {
    uint32_t type;
    uint32_t num_entries;
} efi_virtual_descriptor_t;

typedef struct {
    uint8_t revision;
    uint8_t length;
    uint16_t type;
} efi_device_path_protocol_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct {
    efi_table_header_t hdr;
    uint64_t fw_vendor;
    uint32_t fw_revision;
    uint32_t reserved2;
    uint64_t con_in_handle;
    uint64_t con_in;
    uint64_t con_out_handle;
    uint64_t con_out;
    uint64_t std_err_handle;
    uint64_t std_err;
    uint64_t con_in_caps;
    uint64_t reserved3;
    uint64_t set_state;
    uint64_t parse_interface;
    uint64_t install_configuration_table;
    uint64_t runtime_services;
    uint64_t boot_services;
    uint64_t number_of_table_entries;
    uint64_t configuration_table;
} efi_system_table_t;

typedef struct {
    uint32_t type;
    uint32_t pad;
    efi_physical_address_t physical_start;
    efi_virtual_address_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

typedef struct {
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
} efi_pixel_bitmask_t;

typedef enum {
    pixel_red_green_blue_reserved_8bit_per_color,
    pixel_blue_green_red_reserved_8bit_per_color,
    pixel_bit_mask,
    pixel_blt_only,
    pixel_format_max
} efi_graphics_pixel_format_t;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    efi_graphics_pixel_format_t pixel_format;
    efi_pixel_bitmask_t pixel_information;
    uint32_t pixels_per_scan_line;
} efi_graphics_output_mode_information_t;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    efi_graphics_output_mode_information_t* info;
    uint64_t size_of_info;
    efi_physical_address_t frame_buffer_base;
    uint64_t frame_buffer_size;
} efi_graphics_output_protocol_mode_t;

typedef struct {
    uint64_t reset;
    uint64_t output_string;
    uint64_t test_string;
    uint64_t query_mode;
    uint64_t set_mode;
    uint64_t set_attribute;
    uint64_t clear_screen;
    uint64_t set_cursor_position;
    uint64_t enable_cursor;
    efi_graphics_output_protocol_mode_t* mode;
} efi_graphics_output_protocol_t;

typedef struct {
    uint32_t revision;
    efi_handle_t parent_handle;
    efi_system_table_t* system_table;
    efi_handle_t device_handle;
    efi_device_path_protocol_t* file_path;
    void* reserved;
    uint32_t load_options_size;
    void* load_options;
    void* image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    uint64_t unload;
} efi_loaded_image_protocol_t;

typedef struct {
    efi_table_header_t hdr;
    efi_status_t (*raise_tpl)(uint64_t);
    efi_status_t (*restore_tpl)(uint64_t);
    efi_status_t (*allocate_pages)(int, int, uint64_t, uint64_t*);
    efi_status_t (*free_pages)(uint64_t, uint64_t);
    efi_status_t (*get_memory_map)(uint64_t*, efi_memory_descriptor_t*, uint64_t*, uint64_t*, uint32_t*);
    efi_status_t (*allocate_pool)(int, uint64_t, void**);
    efi_status_t (*free_pool)(void*);
    uint64_t create_event;
    uint64_t set_timer;
    uint64_t wait_for_event;
    uint64_t signal_event;
    uint64_t close_event;
    uint64_t check_event;
    uint64_t install_protocol_interface;
    uint64_t reinstall_protocol_interface;
    uint64_t uninstall_protocol_interface;
    uint64_t handle_protocol;
    uint64_t reserved;
    uint64_t register_protocol_notify;
    uint64_t locate_handle;
    efi_status_t (*locate_device_path)(efi_guid_t*, efi_device_path_protocol_t**, efi_handle_t*);
    uint64_t install_configuration_table;
    efi_status_t (*load_image)(efi_handle_t, efi_handle_t, efi_device_path_protocol_t*, void*, uint64_t, efi_handle_t*);
    efi_status_t (*start_image)(efi_handle_t, uint64_t*, char16_t**);
    efi_status_t (*exit)(efi_handle_t, efi_status_t, uint64_t, char16_t*);
    uint64_t unload_image;
    efi_status_t (*exit_boot_services)(efi_handle_t, uint64_t);
    uint64_t get_next_monotonic_count;
    uint64_t stall;
    uint64_t set_watchdog_timer;
    uint64_t connect_controller;
    uint64_t disconnect_controller;
    efi_status_t (*open_protocol)(efi_handle_t, efi_guid_t*, void**, efi_handle_t, efi_handle_t, uint32_t);
    efi_status_t (*close_protocol)(efi_handle_t, efi_guid_t*, efi_handle_t, efi_handle_t);
    uint64_t open_protocol_information;
    uint64_t protocols_per_handle;
    uint64_t locate_handle_buffer;
    efi_status_t (*locate_protocol)(efi_guid_t*, void*, void**);
    uint64_t install_multiple_protocol_interfaces;
    uint64_t uninstall_multiple_protocol_interfaces;
    efi_status_t (*calculate_crc32)(void*, uint64_t, uint32_t*);
    void (*copy_mem)(void*, void*, uint64_t);
    void (*set_mem)(void*, uint64_t, uint8_t);
    uint64_t create_event_ex;
} efi_boot_services_t;

typedef struct {
    efi_table_header_t hdr;
    efi_status_t (*get_time)(efi_time_t*, efi_time_cap_t*);
    efi_status_t (*set_time)(efi_time_t*);
    efi_status_t (*get_wakeup_time)(uint8_t*, uint8_t*, efi_time_t*);
    efi_status_t (*set_wakeup_time)(uint8_t, efi_time_t*);
    efi_status_t (*set_virtual_address_map)(uint64_t, uint32_t, uint64_t, efi_memory_descriptor_t*);
    uint64_t convert_pointer;
    efi_status_t (*get_variable)(char16_t*, efi_guid_t*, uint32_t*, uint64_t*, void*);
    uint64_t get_next_variable_name;
    efi_status_t (*set_variable)(char16_t*, efi_guid_t*, uint32_t, uint64_t, void*);
    uint64_t get_next_high_monotonic_count;
    efi_status_t (*reset_system)(int, efi_status_t, uint64_t, char16_t*);
} efi_runtime_services_t;

typedef struct {
    uint64_t data_size;
    efi_guid_t guid;
    uint8_t data[1];
} efi_configuration_table_t;

typedef struct {
    uint64_t signature;
    uint64_t revision;
    uint64_t parent_handle;
    efi_system_table_t* system_table;
    efi_handle_t device_handle;
    void* device_path;
    void* device_node;
} efi_loaded_image_t;

efi_status_t efi_get_memory_map(uint64_t* memory_map_size,
                                 efi_memory_descriptor_t* memory_map,
                                 uint64_t* map_key,
                                 uint64_t* descriptor_size,
                                 uint32_t* descriptor_version);

efi_status_t efi_allocate_pages(int type, int memory_type, uint64_t pages, uint64_t* physical_address);
efi_status_t efi_free_pages(uint64_t physical_address, uint64_t pages);
efi_status_t efi_allocate_pool(int pool_type, uint64_t size, void** buffer);
efi_status_t efi_free_pool(void* buffer);

efi_status_t efi_exit_boot_services(efi_handle_t image_handle, uint64_t map_key);
efi_status_t efi_set_virtual_address_map(uint64_t memory_map_size, uint64_t descriptor_size, uint32_t descriptor_version,
                                  efi_memory_descriptor_t* virtual_map);

void efi_init(void* system_table);

efi_status_t efi_open_protocol(efi_handle_t handle, efi_guid_t* protocol, void** interface, efi_handle_t agent_handle, efi_handle_t controller_handle, uint32_t attributes);
efi_status_t efi_close_protocol(efi_handle_t handle, efi_guid_t* protocol, efi_handle_t agent_handle, efi_handle_t controller_handle);
efi_status_t efi_locate_protocol(efi_guid_t* protocol, void* registration, void** interface);
efi_status_t efi_locate_device_path(efi_guid_t* protocol, efi_device_path_protocol_t** device_path, efi_handle_t* device);

efi_status_t efi_load_image(efi_handle_t parent_image, efi_handle_t* loaded_image);
efi_status_t efi_start_image(efi_handle_t image_handle, uint64_t* exit_data_size, char16_t** exit_data);
efi_status_t efi_exit(efi_handle_t image_handle, efi_status_t exit_status, uint64_t exit_data_size, char16_t* exit_data);

efi_status_t efi_get_variable(char16_t* variable_name, efi_guid_t* vendor_guid, uint32_t* attributes, uint64_t* data_size, void* data);
efi_status_t efi_set_variable(char16_t* variable_name, efi_guid_t* vendor_guid, uint32_t attributes, uint64_t data_size, void* data);
efi_status_t efi_get_time(efi_time_t* time, efi_time_cap_t* capabilities);
efi_status_t efi_set_time(efi_time_t* time);
efi_status_t efi_get_wakeup_time(uint8_t* enabled, uint8_t* pending, efi_time_t* time);
efi_status_t efi_set_wakeup_time(uint8_t enabled, efi_time_t* time);
efi_status_t efi_reset_system(int reset_type, efi_status_t reset_status, uint64_t data_size, char16_t* reset_data);
efi_status_t efi_calculate_crc32(void* data, uint64_t length, uint32_t* crc32);
void efi_copy_mem(void* destination, void* source, uint64_t length);
void efi_set_mem(void* buffer, uint64_t size, uint8_t value);

efi_status_t efi_gop_get_framebuffer(efi_handle_t image_handle, efi_graphics_output_protocol_t** gop);
efi_status_t efi_gop_set_mode(efi_graphics_output_protocol_t* gop, uint32_t mode);

efi_status_t efi_get_memory_map_and_exit(efi_handle_t image_handle, efi_memory_descriptor_t** map, uint64_t* map_size, uint64_t* map_key, uint64_t* desc_size, uint32_t* desc_version);

extern efi_memory_descriptor_t* efi_memory_map;
extern uint64_t efi_memory_map_size;
extern uint64_t efi_memory_map_key;
extern uint64_t efi_descriptor_size;
extern uint32_t efi_descriptor_version;
extern efi_graphics_output_protocol_mode_t* efi_gop_mode;
extern int efi_boot_services_exited;
extern efi_system_table_t* efi_system_table;

const char* efi_memory_type_to_string(uint32_t type);
uint64_t efi_get_total_memory(void);
uint64_t efi_get_usable_memory(void);

#endif
