

#ifndef ARCH_ELF_H
#define ARCH_ELF_H

#include <arch/types.h>

#define ELFMAG          "\x7fELF"
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EV_CURRENT      1
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define EM_X86_64       62

#define PT_LOAD         1
#define PT_INTERP       3
#define PT_PHDR         6
#define PT_GNU_STACK    0x6474e551

#define PF_X            0x1
#define PF_W            0x2
#define PF_R            0x4

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

typedef struct {
    uint64_t entry;
    uint64_t stack_top;
    uint64_t phdr_addr;
    void*    pml4;
    uint16_t phnum;
    uint16_t phentsize;
    int      is_dynamic;
    char     interpreter[256];
} elf_load_info_t;

int elf_verify(const Elf64_Ehdr* ehdr);

int elf_load_from_memory(const void* data, uint64_t size, elf_load_info_t* info);

int elf_load_from_file(const char* path, elf_load_info_t* info);

uint64_t elf_setup_stack(uint64_t stack_top, const char* argv[], const char* envp[],
                          const elf_load_info_t* info);

#endif
