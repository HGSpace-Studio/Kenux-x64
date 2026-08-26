#ifndef KEGD_EFI_H
#define KEGD_EFI_H

#include <stdint.h>

typedef uint64_t efi_status_t;
typedef uint64_t efi_uintn_t;
typedef uint64_t efi_physical_address_t;
typedef void* efi_handle_t;
typedef uint16_t char16_t;

#define EFI_SUCCESS 0
#define EFI_ERROR_BIT 0x8000000000000000ULL
#define EFI_LOAD_ERROR (EFI_ERROR_BIT | 1)
#define EFI_INVALID_PARAMETER (EFI_ERROR_BIT | 2)
#define EFI_BUFFER_TOO_SMALL (EFI_ERROR_BIT | 5)
#define EFI_NOT_FOUND (EFI_ERROR_BIT | 14)

#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249ULL
#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42ULL
#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001
#define EFI_FILE_MODE_READ 0x0000000000000001ULL
#define EFI_LOADER_DATA 2
#define EFI_ALLOCATE_ANY_PAGES 0
#define EFI_ALLOCATE_MAX_ADDRESS 1
#define EFI_ALLOCATE_ADDRESS 2

#define EFIAPI __attribute__((ms_abi))

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

typedef struct {
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
} efi_pixel_bitmask_t;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    int32_t pixel_format;
    efi_pixel_bitmask_t pixel_info;
    uint32_t pixels_per_scanline;
} efi_graphics_output_mode_info_t;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    efi_graphics_output_mode_info_t *info;
    efi_uintn_t info_size;
    uint64_t frame_buffer_base;
    uint64_t frame_buffer_size;
} efi_graphics_output_protocol_mode_t;

typedef struct {
    efi_status_t (EFIAPI *query_mode)(void *This, uint32_t mode, efi_uintn_t *size, efi_graphics_output_mode_info_t **info);
    efi_status_t (EFIAPI *set_mode)(void *This, uint32_t mode);
    void *blt;
    efi_graphics_output_protocol_mode_t *mode;
} efi_graphics_output_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct efi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    efi_physical_address_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

typedef struct efi_boot_services {
    efi_table_header_t hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    void *handle_protocol;
    void *pc_handle_protocol;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    void *locate_protocol;
    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32;
    void *copy_mem;
    void *set_mem;
    void *create_event_ex;
} efi_boot_services_t;

typedef struct efi_simple_text_output {
    void *reset;
    efi_status_t (EFIAPI *output_string)(struct efi_simple_text_output *This, char16_t *string);
    void *test_string;
    void *query_mode;
    void *set_mode;
    void *set_attribute;
    void *clear_screen;
    void *set_cursor_position;
    void *enable_cursor;
    void *mode;
} efi_simple_text_output_t;

typedef struct {
    efi_table_header_t hdr;
    char16_t *fw_vendor;
    uint32_t fw_revision;
    void *con_in_handle;
    void *con_in;
    void *con_out_handle;
    efi_simple_text_output_t *con_out;
    void *std_err_handle;
    efi_simple_text_output_t *std_err;
    void *runtime_services;
    efi_boot_services_t *boot_services;
    uint64_t number_of_table_entries;
    void *configuration_table;
} efi_system_table_t;

typedef struct efi_loaded_image_protocol {
    uint32_t revision;
    efi_handle_t parent_handle;
    efi_system_table_t *system_table;
    efi_handle_t device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    efi_status_t (EFIAPI *unload)(efi_handle_t image_handle);
} efi_loaded_image_protocol_t;

typedef struct efi_simple_file efi_simple_file_t;

struct efi_simple_file {
    uint64_t revision;
    efi_status_t (EFIAPI *open)(efi_simple_file_t *This, efi_simple_file_t **new_handle, char16_t *file_name, uint64_t open_mode, uint64_t attributes);
    efi_status_t (EFIAPI *close)(efi_simple_file_t *This);
    efi_status_t (EFIAPI *delete_file)(efi_simple_file_t *This);
    efi_status_t (EFIAPI *read)(efi_simple_file_t *This, efi_uintn_t *buffer_size, void *buffer);
    efi_status_t (EFIAPI *write)(efi_simple_file_t *This, efi_uintn_t *buffer_size, void *buffer);
    efi_status_t (EFIAPI *get_position)(efi_simple_file_t *This, uint64_t *position);
    efi_status_t (EFIAPI *set_position)(efi_simple_file_t *This, uint64_t position);
    efi_status_t (EFIAPI *get_info)(efi_simple_file_t *This, efi_guid_t *information_type, efi_uintn_t *buffer_size, void *buffer);
    efi_status_t (EFIAPI *set_info)(efi_simple_file_t *This, efi_guid_t *information_type, efi_uintn_t buffer_size, void *buffer);
    efi_status_t (EFIAPI *flush)(efi_simple_file_t *This);
};

typedef struct efi_simple_filesystem {
    uint64_t revision;
    efi_status_t (EFIAPI *open_volume)(struct efi_simple_filesystem *This, efi_simple_file_t **root);
} efi_simple_filesystem_t;

typedef efi_status_t (EFIAPI *efi_handle_protocol_t)(
    efi_handle_t handle,
    efi_guid_t *protocol,
    void **interface
);
typedef efi_status_t (EFIAPI *efi_locate_protocol_t)(
    efi_guid_t *protocol,
    void *registration,
    void **interface
);
typedef efi_status_t (EFIAPI *efi_allocate_pool_t)(
    uint32_t pool_type,
    efi_uintn_t size,
    void **buffer
);
typedef efi_status_t (EFIAPI *efi_free_pool_t)(void *buffer);
typedef efi_status_t (EFIAPI *efi_allocate_pages_t)(
    uint32_t type,
    uint32_t memory_type,
    efi_uintn_t pages,
    efi_physical_address_t *memory
);
typedef efi_status_t (EFIAPI *efi_get_memory_map_t)(
    efi_uintn_t *memory_map_size,
    efi_memory_descriptor_t *memory_map,
    efi_uintn_t *map_key,
    efi_uintn_t *descriptor_size,
    uint32_t *descriptor_version
);
typedef efi_status_t (EFIAPI *efi_exit_boot_services_t)(
    efi_handle_t image_handle,
    efi_uintn_t map_key
);

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} pixel_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    char *cmdline;
    uint32_t mods_count;
    uint64_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint64_t mmap_addr;
    uint32_t drives_length;
    uint64_t drives_addr;
    uint64_t config_table;
    uint64_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} multiboot_info_t;

typedef void (*kernel_entry_t)(uint32_t magic, multiboot_info_t *info);

#endif
