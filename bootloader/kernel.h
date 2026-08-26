#ifndef KEGD_KERNEL_H
#define KEGD_KERNEL_H

#include "efi.h"

typedef struct {
    uint8_t magic[4];
    uint8_t elf_class;
    uint8_t endian;
    uint8_t version;
    uint8_t os_abi;
    uint8_t pad[8];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} elf_program_header_t;

efi_status_t load_kernel(efi_handle_t image_handle, efi_system_table_t *st, const char *kernel_path, void **kernel_entry, void **mb_info, uint64_t framebuffer_base, uint64_t framebuffer_size);

void jump_to_kernel(kernel_entry_t entry, uint32_t magic, multiboot_info_t *mb_info);

#endif
