#include <stddef.h>
#include "kernel.h"
#include "serial.h"

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return 0;
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

extern efi_boot_services_t* get_boot_services(efi_system_table_t *st);
extern void *memset(void *s, uint32_t c, uint64_t n);
extern void *memcpy(void *dest, const void *src, uint64_t n);

// EFI 简单文件系统协议 GUID
#define SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define LOADED_IMAGE_PROTOCOL_GUID \
    {0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define FILE_INFO_GUID \
    {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define PAGE_PRESENT 0x001ULL
#define PAGE_WRITABLE 0x002ULL
#define PAGE_SIZE_2M 0x080ULL
#define KERNEL_HIGH_BASE 0xffffffff80000000ULL
#define LOW_IDENTITY_PDPT_COUNT 8

static uint64_t *kernel_pml4;
static uint64_t *kernel_pdpt_low;
static uint64_t *kernel_pd_low[LOW_IDENTITY_PDPT_COUNT];
static uint64_t *kernel_pdpt_high;
static uint64_t *kernel_pd_high;
static uint64_t *kernel_pt_high[2];

static char16_t ascii_to_efi_path_char(char c) {
    return c == '/' ? (char16_t)'\\' : (char16_t)c;
}

static efi_status_t alloc_page_table(efi_allocate_pages_t allocate_pages, uint64_t **table) {
    efi_physical_address_t address = 0xffffffff;
    efi_status_t status = allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, 1, &address);
    if (status != EFI_SUCCESS) {
        serial_write("load_kernel: handle loaded image failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    memset((void*)address, 0, 4096);
    *table = (uint64_t*)address;
    return EFI_SUCCESS;
}

static efi_status_t setup_kernel_page_tables(efi_allocate_pages_t allocate_pages, uint64_t kernel_base, uint64_t kernel_size, uint64_t framebuffer_base, uint64_t framebuffer_size) {
    efi_status_t status = alloc_page_table(allocate_pages, &kernel_pml4);
    if (status != EFI_SUCCESS) return status;
    status = alloc_page_table(allocate_pages, &kernel_pdpt_low);
    if (status != EFI_SUCCESS) return status;
    for (int i = 0; i < LOW_IDENTITY_PDPT_COUNT; i++) {
        status = alloc_page_table(allocate_pages, &kernel_pd_low[i]);
        if (status != EFI_SUCCESS) return status;
    }
    status = alloc_page_table(allocate_pages, &kernel_pdpt_high);
    if (status != EFI_SUCCESS) return status;
    status = alloc_page_table(allocate_pages, &kernel_pd_high);
    if (status != EFI_SUCCESS) return status;
    status = alloc_page_table(allocate_pages, &kernel_pt_high[0]);
    if (status != EFI_SUCCESS) return status;
    status = alloc_page_table(allocate_pages, &kernel_pt_high[1]);
    if (status != EFI_SUCCESS) return status;

    kernel_pml4[0] = (uint64_t)kernel_pdpt_low | PAGE_PRESENT | PAGE_WRITABLE;
    for (int table = 0; table < LOW_IDENTITY_PDPT_COUNT; table++) {
        kernel_pdpt_low[table] = (uint64_t)kernel_pd_low[table] | PAGE_PRESENT | PAGE_WRITABLE;
    }

    kernel_pml4[511] = (uint64_t)kernel_pdpt_high | PAGE_PRESENT | PAGE_WRITABLE;
    kernel_pdpt_high[510] = (uint64_t)kernel_pd_high | PAGE_PRESENT | PAGE_WRITABLE;

    for (uint64_t i = 0; i < LOW_IDENTITY_PDPT_COUNT * 512ULL; i++) {
        uint64_t phys = i * 0x200000ULL;
        kernel_pd_low[i / 512][i % 512] = phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2M;
    }

    if (framebuffer_base && framebuffer_size) {
        uint64_t start = framebuffer_base & ~0x1FFFFFULL;
        uint64_t end = (framebuffer_base + framebuffer_size + 0x1FFFFFULL) & ~0x1FFFFFULL;
        for (uint64_t phys = start; phys < end; phys += 0x200000ULL) {
            uint64_t pdpt_index = phys >> 30;
            uint64_t pd_index = (phys >> 21) & 0x1FF;
            if (pdpt_index < LOW_IDENTITY_PDPT_COUNT) {
                kernel_pd_low[pdpt_index][pd_index] = phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2M;
            } else {
                serial_write("load_kernel: framebuffer above identity map ");
                serial_write_hex64(phys);
                serial_write("\n");
            }
        }
    }

    uint64_t mapped_pages = (kernel_size + 0xFFF) >> 12;
    if (mapped_pages > 1024) {
        mapped_pages = 1024;
    }

    kernel_pd_high[0] = (uint64_t)kernel_pt_high[0] | PAGE_PRESENT | PAGE_WRITABLE;
    kernel_pd_high[1] = (uint64_t)kernel_pt_high[1] | PAGE_PRESENT | PAGE_WRITABLE;
    for (uint64_t i = 0; i < mapped_pages; i++) {
        uint64_t table = i / 512;
        uint64_t index = i % 512;
        kernel_pt_high[table][index] = (kernel_base + i * 0x1000ULL) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    return EFI_SUCCESS;
}

efi_status_t load_kernel(efi_handle_t image_handle, efi_system_table_t *st, const char *kernel_path, void **kernel_entry, void **mb_info, uint64_t framebuffer_base, uint64_t framebuffer_size) {
    efi_boot_services_t *bs = get_boot_services(st);
    efi_handle_protocol_t handle_protocol = (efi_handle_protocol_t)bs->handle_protocol;
    efi_allocate_pool_t allocate_pool = (efi_allocate_pool_t)bs->allocate_pool;
    efi_free_pool_t free_pool = (efi_free_pool_t)bs->free_pool;
    efi_allocate_pages_t allocate_pages = (efi_allocate_pages_t)bs->allocate_pages;
    
    *kernel_entry = NULL;
    *mb_info = NULL;
    
    // 创建 multiboot 信息
    multiboot_info_t *info = (multiboot_info_t*)0x5000;
    memset(info, 0, sizeof(multiboot_info_t));
    
    info->flags = 0x00000001 | 0x00000002 | 0x00000004;
    info->mem_lower = 640;
    info->mem_upper = 3072;
    info->boot_device = 0xE0;
    
    info->flags |= 0x00000040;
    info->vbe_mode = 0x118;
    
    *mb_info = (void*)info;
    
    efi_guid_t loaded_image_guid = LOADED_IMAGE_PROTOCOL_GUID;
    efi_loaded_image_protocol_t *loaded_image = NULL;
    efi_status_t status = handle_protocol(
        image_handle,
        &loaded_image_guid,
        (void **)&loaded_image
    );
    if (status != EFI_SUCCESS) {
        serial_write("load_kernel: handle filesystem failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }

    efi_guid_t fs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    efi_simple_filesystem_t *fs = NULL;
    status = handle_protocol(
        loaded_image->device_handle,
        &fs_guid,
        (void **)&fs
    );
    if (status != EFI_SUCCESS) {
        serial_write("load_kernel: open volume failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // 打开根目录
    efi_simple_file_t *root = NULL;
    status = fs->open_volume(fs, &root);
    if (status != EFI_SUCCESS) {
        serial_write("load_kernel: open kernel file failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // 将 ASCII 路径转换为 UTF-16
    char16_t u16_path[256];
    int i = 0;
    while (kernel_path[i] && i < 255) {
        u16_path[i] = ascii_to_efi_path_char(kernel_path[i]);
        i++;
    }
    u16_path[i] = 0;
    
    // 打开文件
    efi_simple_file_t *file = NULL;
    status = root->open(root, &file, u16_path, EFI_FILE_MODE_READ, 0);
    root->close(root);
    if (status != EFI_SUCCESS) {
        return status;
    }
    
    // 获取文件信息以确定大小
    efi_uintn_t info_size = 0;
    efi_guid_t file_info_guid = FILE_INFO_GUID;
    status = file->get_info(file, &file_info_guid, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        file->close(file);
        serial_write("load_kernel: get_info size failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // 分配内存用于文件信息
    void *file_info = NULL;
    status = allocate_pool(EFI_LOADER_DATA, info_size, &file_info);
    if (status != EFI_SUCCESS) {
        file->close(file);
        serial_write("load_kernel: allocate file info failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // 获取文件信息
    status = file->get_info(file, &file_info_guid, &info_size, file_info);
    if (status != EFI_SUCCESS) {
        free_pool(file_info);
        file->close(file);
        serial_write("load_kernel: get_info data failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // EFI_FILE_INFO layout: Size, FileSize, PhysicalSize, timestamps...
    efi_uintn_t file_size = *(uint64_t*)((uint8_t*)file_info + 8);
    free_pool(file_info);
    
    // 分配缓冲区用于内核
    void *kernel_buffer = NULL;
    status = allocate_pool(EFI_LOADER_DATA, file_size, &kernel_buffer);
    if (status != EFI_SUCCESS) {
        file->close(file);
        serial_write("load_kernel: allocate kernel buffer failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    
    // 读取文件
    efi_uintn_t bytes_read = file_size;
    status = file->read(file, &bytes_read, kernel_buffer);
    file->close(file);
    if (status != EFI_SUCCESS) {
        free_pool(kernel_buffer);
        serial_write("load_kernel: read kernel file failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }
    if (bytes_read < sizeof(elf_header_t)) {
        free_pool(kernel_buffer);
        serial_write("load_kernel: short kernel read\n");
        return status;
    }
    
    // 验证 ELF 魔数
    elf_header_t *elf = (elf_header_t*)kernel_buffer;
    
    if (elf->magic[0] != 0x7F || elf->magic[1] != 'E' || elf->magic[2] != 'L' || elf->magic[3] != 'F') {
        free_pool(kernel_buffer);
        serial_write("load_kernel: bad ELF magic\n");
        return EFI_LOAD_ERROR;
    }
    
    // 先为高半区内核保留一段低物理内存，再按 vaddr 偏移装载。
    elf_program_header_t *ph = (elf_program_header_t*)((uint8_t*)elf + elf->phoff);
    uint64_t max_kernel_offset = 0;
    for (int j = 0; j < elf->phnum; j++) {
        if (ph[j].type == 1) {  // PT_LOAD
            if (ph[j].vaddr < KERNEL_HIGH_BASE) {
                free_pool(kernel_buffer);
                serial_write("load_kernel: segment below high base\n");
                return EFI_LOAD_ERROR;
            }
            uint64_t end = (ph[j].vaddr - KERNEL_HIGH_BASE) + ph[j].memsz;
            if (end > max_kernel_offset) {
                max_kernel_offset = end;
            }
        }
    }

    efi_physical_address_t kernel_base = 0x3fffffff;
    uint64_t kernel_pages = (max_kernel_offset + 0xFFF) >> 12;
    status = allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, kernel_pages, &kernel_base);
    if (status != EFI_SUCCESS) {
        free_pool(kernel_buffer);
        serial_write("load_kernel: allocate kernel pages failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }

    for (int j = 0; j < elf->phnum; j++) {
        if (ph[j].type == 1) {  // PT_LOAD
            uint64_t dest_addr = kernel_base + (ph[j].vaddr - KERNEL_HIGH_BASE);
            memcpy((void*)dest_addr, (uint8_t*)elf + ph[j].offset, ph[j].filesz);
            if (ph[j].memsz > ph[j].filesz) {
                memset((uint8_t*)dest_addr + ph[j].filesz, 0, ph[j].memsz - ph[j].filesz);
            }
        }
    }
    
    status = setup_kernel_page_tables(allocate_pages, kernel_base, max_kernel_offset, framebuffer_base, framebuffer_size);
    if (status != EFI_SUCCESS) {
        free_pool(kernel_buffer);
        serial_write("load_kernel: setup page tables failed ");
        serial_write_hex64(status);
        serial_write("\n");
        return status;
    }

    *kernel_entry = (void*)(uintptr_t)elf->entry;
    free_pool(kernel_buffer);
    
    return EFI_SUCCESS;
}

void jump_to_kernel(kernel_entry_t entry, uint32_t magic, multiboot_info_t *mb_info) {
    __asm__ volatile (
        "cli\n"
        "mov %0, %%cr3\n"
        :
        : "r"(kernel_pml4)
        : "memory"
    );
    
    if (entry) {
        entry(magic, mb_info);
    }
    
    while(1) {
        __asm__ volatile ("hlt");
    }
}
