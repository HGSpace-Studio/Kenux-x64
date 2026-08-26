#include <arch/efi.h>
#include <arch/memory.h>
#include <string.h>

efi_system_table_t* efi_system_table = NULL;
static efi_boot_services_t* efi_boot_services = NULL;

efi_memory_descriptor_t* efi_memory_map = NULL;
uint64_t efi_memory_map_size = 0;
uint64_t efi_memory_map_key = 0;
uint64_t efi_descriptor_size = 0;
uint32_t efi_descriptor_version = 0;
efi_graphics_output_protocol_mode_t* efi_gop_mode = NULL;
int efi_boot_services_exited = 0;

void efi_init(void* system_table)
{
    efi_system_table = (efi_system_table_t*)system_table;
    efi_boot_services = (efi_boot_services_t*)efi_system_table->boot_services;
}

efi_status_t efi_get_memory_map(uint64_t* memory_map_size,
                                 efi_memory_descriptor_t* memory_map,
                                 uint64_t* map_key,
                                 uint64_t* descriptor_size,
                                 uint32_t* descriptor_version)
{
    return efi_boot_services->get_memory_map(memory_map_size, memory_map, map_key, descriptor_size, descriptor_version);
}

efi_status_t efi_allocate_pages(int type, int memory_type, uint64_t pages, uint64_t* physical_address)
{
    return efi_boot_services->allocate_pages(type, memory_type, pages, physical_address);
}

efi_status_t efi_free_pages(uint64_t physical_address, uint64_t pages)
{
    return efi_boot_services->free_pages(physical_address, pages);
}

efi_status_t efi_allocate_pool(int pool_type, uint64_t size, void** buffer)
{
    return efi_boot_services->allocate_pool(pool_type, size, buffer);
}

efi_status_t efi_free_pool(void* buffer)
{
    return efi_boot_services->free_pool(buffer);
}

efi_status_t efi_exit_boot_services(efi_handle_t image_handle, uint64_t map_key)
{
    return efi_boot_services->exit_boot_services(image_handle, map_key);
}

efi_status_t efi_set_virtual_address_map(uint64_t memory_map_size, uint64_t descriptor_size, uint32_t descriptor_version,
                                  efi_memory_descriptor_t* virtual_map)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->set_virtual_address_map(memory_map_size, descriptor_version, descriptor_size, virtual_map);
}

efi_status_t efi_load_image(efi_handle_t parent_image, efi_handle_t* loaded_image)
{
    return efi_boot_services->load_image(parent_image, NULL, NULL, NULL, 0, loaded_image);
}

efi_status_t efi_start_image(efi_handle_t image_handle, uint64_t* exit_data_size, char16_t** exit_data)
{
    return efi_boot_services->start_image(image_handle, exit_data_size, exit_data);
}

efi_status_t efi_exit(efi_handle_t image_handle, efi_status_t exit_status, uint64_t exit_data_size, char16_t* exit_data)
{
    return efi_boot_services->exit(image_handle, exit_status, exit_data_size, exit_data);
}

efi_status_t efi_open_protocol(efi_handle_t handle, efi_guid_t* protocol, void** interface, efi_handle_t agent_handle, efi_handle_t controller_handle, uint32_t attributes)
{
    return efi_boot_services->open_protocol(handle, protocol, interface, agent_handle, controller_handle, attributes);
}

efi_status_t efi_close_protocol(efi_handle_t handle, efi_guid_t* protocol, efi_handle_t agent_handle, efi_handle_t controller_handle)
{
    return efi_boot_services->close_protocol(handle, protocol, agent_handle, controller_handle);
}

efi_status_t efi_locate_protocol(efi_guid_t* protocol, void* registration, void** interface)
{
    return efi_boot_services->locate_protocol(protocol, registration, interface);
}

efi_status_t efi_locate_device_path(efi_guid_t* protocol, efi_device_path_protocol_t** device_path, efi_handle_t* device)
{
    return efi_boot_services->locate_device_path(protocol, device_path, device);
}

efi_status_t efi_get_variable(char16_t* variable_name, efi_guid_t* vendor_guid, uint32_t* attributes, uint64_t* data_size, void* data)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->get_variable(variable_name, vendor_guid, attributes, data_size, data);
}

efi_status_t efi_set_variable(char16_t* variable_name, efi_guid_t* vendor_guid, uint32_t attributes, uint64_t data_size, void* data)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->set_variable(variable_name, vendor_guid, attributes, data_size, data);
}

efi_status_t efi_get_time(efi_time_t* time, efi_time_cap_t* capabilities)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->get_time(time, capabilities);
}

efi_status_t efi_set_time(efi_time_t* time)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->set_time(time);
}

efi_status_t efi_get_wakeup_time(uint8_t* enabled, uint8_t* pending, efi_time_t* time)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->get_wakeup_time(enabled, pending, time);
}

efi_status_t efi_set_wakeup_time(uint8_t enabled, efi_time_t* time)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->set_wakeup_time(enabled, time);
}

efi_status_t efi_reset_system(int reset_type, efi_status_t reset_status, uint64_t data_size, char16_t* reset_data)
{
    return ((efi_runtime_services_t*)efi_system_table->runtime_services)->reset_system(reset_type, reset_status, data_size, reset_data);
}

efi_status_t efi_calculate_crc32(void* data, uint64_t length, uint32_t* crc32)
{
    return efi_boot_services->calculate_crc32(data, length, crc32);
}

void efi_copy_mem(void* destination, void* source, uint64_t length)
{
    efi_boot_services->copy_mem(destination, source, length);
}

void efi_set_mem(void* buffer, uint64_t size, uint8_t value)
{
    efi_boot_services->set_mem(buffer, size, value);
}

static efi_guid_t gop_guid = {0x9042A9DE, 0x23DC, 0x4A38, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}};

efi_status_t efi_gop_get_framebuffer(efi_handle_t image_handle, efi_graphics_output_protocol_t** gop)
{
    efi_status_t status;

    status = efi_locate_protocol(&gop_guid, NULL, (void**)gop);
    if (status != EFI_SUCCESS) {
        efi_handle_t handle = NULL;
        efi_device_path_protocol_t* path = NULL;
        status = efi_locate_device_path(&gop_guid, &path, &handle);
        if (status == EFI_SUCCESS && handle) {
            status = efi_open_protocol(handle, &gop_guid, (void**)gop, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        }
    }

    return status;
}

efi_status_t efi_gop_set_mode(efi_graphics_output_protocol_t* gop, uint32_t mode)
{
    efi_status_t status;
    uint64_t (*set_mode_fn)(efi_graphics_output_protocol_t*, uint32_t);

    if (!gop) return EFI_INVALID_PARAMETER;
    if (mode >= gop->mode->max_mode) return EFI_INVALID_PARAMETER;

    set_mode_fn = (uint64_t (*)(efi_graphics_output_protocol_t*, uint32_t))gop->set_mode;
    status = set_mode_fn(gop, mode);

    return status;
}

efi_status_t efi_get_memory_map_and_exit(efi_handle_t image_handle, efi_memory_descriptor_t** map, uint64_t* map_size, uint64_t* map_key, uint64_t* desc_size, uint32_t* desc_version)
{
    efi_status_t status;
    uint64_t local_map_size = 0;
    uint64_t local_map_key = 0;
    uint64_t local_desc_size = 0;
    uint32_t local_desc_version = 0;
    efi_memory_descriptor_t* local_map = NULL;
    int retries = 3;

    status = efi_get_memory_map(&local_map_size, NULL, &local_map_key, &local_desc_size, &local_desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    local_map_size += local_desc_size * 8;

    status = efi_allocate_pool(EFI_LOADER_DATA, local_map_size, (void**)&local_map);
    if (status != EFI_SUCCESS) {
        return status;
    }

    memset(local_map, 0, local_map_size);

    while (retries > 0) {
        status = efi_get_memory_map(&local_map_size, local_map, &local_map_key, &local_desc_size, &local_desc_version);
        if (status != EFI_SUCCESS) {
            efi_free_pool(local_map);
            return status;
        }

        status = efi_exit_boot_services(image_handle, local_map_key);
        if (status == EFI_SUCCESS) {
            break;
        }

        if (status == EFI_INVALID_PARAMETER) {
            retries--;
            local_map_size += local_desc_size * 4;
            efi_memory_descriptor_t* new_map;
            status = efi_allocate_pool(EFI_LOADER_DATA, local_map_size, (void**)&new_map);
            if (status != EFI_SUCCESS) {
                efi_free_pool(local_map);
                return status;
            }
            efi_free_pool(local_map);
            local_map = new_map;
            memset(local_map, 0, local_map_size);
            continue;
        }

        efi_free_pool(local_map);
        return status;
    }

    if (status != EFI_SUCCESS) {
        if (local_map) efi_free_pool(local_map);
        return status;
    }

    efi_boot_services_exited = 1;

    if (map) *map = local_map;
    if (map_size) *map_size = local_map_size;
    if (map_key) *map_key = local_map_key;
    if (desc_size) *desc_size = local_desc_size;
    if (desc_version) *desc_version = local_desc_version;

    efi_memory_map = local_map;
    efi_memory_map_size = local_map_size;
    efi_memory_map_key = local_map_key;
    efi_descriptor_size = local_desc_size;
    efi_descriptor_version = local_desc_version;

    return EFI_SUCCESS;
}

const char* efi_memory_type_to_string(uint32_t type)
{
    switch (type) {
        case EFI_LOADER_CODE: return "LoaderCode";
        case EFI_LOADER_DATA: return "LoaderData";
        case EFI_BOOT_SERVICES_CODE: return "BootServicesCode";
        case EFI_BOOT_SERVICES_DATA: return "BootServicesData";
        case EFI_RUNTIME_SERVICES_CODE: return "RuntimeServicesCode";
        case EFI_RUNTIME_SERVICES_DATA: return "RuntimeServicesData";
        case EFI_CONVENTIONAL_MEMORY: return "ConventionalMemory";
        case EFI_UNUSABLE_MEMORY: return "UnusableMemory";
        case EFI_ACPI_RECLAIM_MEMORY: return "AcpiReclaimMemory";
        case EFI_ACPI_MEMORY_NVS: return "AcpiMemoryNvs";
        case EFI_MEMORY_MAPPED_IO: return "MemoryMappedIO";
        case EFI_MEMORY_MAPPED_IO_PORT_SPACE: return "MemoryMappedIOPortSpace";
        case EFI_PAL_CODE: return "PalCode";
        case EFI_PERSISTENT_MEMORY: return "PersistentMemory";
        default: return "Unknown";
    }
}

uint64_t efi_get_total_memory(void)
{
    uint64_t total = 0;
    if (!efi_memory_map || efi_descriptor_size == 0) return 0;

    uint64_t count = efi_memory_map_size / efi_descriptor_size;
    for (uint64_t i = 0; i < count; i++) {
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint8_t*)efi_memory_map + i * efi_descriptor_size);
        total += desc->number_of_pages * 4096;
    }
    return total;
}

uint64_t efi_get_usable_memory(void)
{
    uint64_t usable = 0;
    if (!efi_memory_map || efi_descriptor_size == 0) return 0;

    uint64_t count = efi_memory_map_size / efi_descriptor_size;
    for (uint64_t i = 0; i < count; i++) {
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint8_t*)efi_memory_map + i * efi_descriptor_size);
        if (desc->type == EFI_CONVENTIONAL_MEMORY) {
            usable += desc->number_of_pages * 4096;
        }
    }
    return usable;
}
