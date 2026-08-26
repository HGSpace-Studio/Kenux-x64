

#include <arch/elf.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <arch/fs.h>
#include <leonos_compat.h>
#include <string.h>

extern int pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);
extern void* pmap_create(void);

#define USER_STACK_SIZE     (2 * 1024 * 1024)
#define USER_STACK_TOP      0x00007FFFFFFFFFFFULL

int elf_verify(const Elf64_Ehdr* ehdr)
{
    if (!ehdr) return -1;
    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        return -1;
    }
    if (ehdr->e_ident[4] != ELFCLASS64) return -1;
    if (ehdr->e_ident[5] != ELFDATA2LSB) return -1;
    if (ehdr->e_ident[6] != EV_CURRENT) return -1;
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) return -1;
    if (ehdr->e_machine != EM_X86_64) return -1;
    return 0;
}

int elf_load_from_memory(const void* data, uint64_t size, elf_load_info_t* info)
{
    if (!data || !info || size < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;
    if (elf_verify(ehdr) != 0) return -1;

    memset(info, 0, sizeof(elf_load_info_t));
    info->entry = ehdr->e_entry;
    info->phnum = ehdr->e_phnum;
    info->phentsize = ehdr->e_phentsize;
    info->is_dynamic = (ehdr->e_type == ET_DYN);
    leonos_compat_info_t compat;
    if (leonos_compat_detect_elf(data, size, &compat) &&
        compat.type == LEONOS_COMPAT_APP_LEONOS_ELF) {
        info->is_dynamic = 1;
        if (compat.interpreter[0]) {
            memcpy(info->interpreter, compat.interpreter, sizeof(info->interpreter) - 1u);
            info->interpreter[sizeof(info->interpreter) - 1u] = '\0';
        } else {
            memcpy(info->interpreter, "kenux-leonos-compat",
                   sizeof("kenux-leonos-compat"));
        }
    }

    const Elf64_Phdr* phdrs = (const Elf64_Phdr*)((const uint8_t*)data + ehdr->e_phoff);
    uint64_t min_vaddr = (uint64_t)-1;
    uint64_t max_vaddr = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph = &phdrs[i];

        if (ph->p_type == PT_LOAD) {
            if (ph->p_vaddr < min_vaddr) min_vaddr = ph->p_vaddr;
            uint64_t end = ph->p_vaddr + ph->p_memsz;
            if (end > max_vaddr) max_vaddr = end;
        } else if (ph->p_type == PT_INTERP) {
            info->is_dynamic = 1;
            uint64_t len = ph->p_filesz;
            if (len > 255) len = 255;
            memcpy(info->interpreter,
                   (const uint8_t*)data + ph->p_offset, len);
            info->interpreter[len] = '\0';
        } else if (ph->p_type == PT_PHDR) {
            info->phdr_addr = ph->p_vaddr;
        }
    }

    void* pml4 = pmap_create();
    if (!pml4) return -1;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if (ph->p_flags & PF_W) flags |= PAGE_WRITABLE;

        uint64_t vaddr = ph->p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t offset = ph->p_offset & ~(PAGE_SIZE - 1);
        uint64_t file_end = ph->p_offset + ph->p_filesz;
        uint64_t mem_end = ph->p_vaddr + ph->p_memsz;
        uint64_t page_end = (mem_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uint64_t va = vaddr, off = offset; va < page_end; va += PAGE_SIZE, off += PAGE_SIZE) {

            uint64_t paddr = memory_alloc_physical(1);
            if (!paddr) return -1;

            if (off < file_end) {
                uint64_t copy_start = (off < ph->p_offset) ? (ph->p_offset - off) : 0;
                uint64_t copy_size = PAGE_SIZE - copy_start;
                if (off + copy_size > file_end) {
                    copy_size = file_end - off - copy_start;
                }
                if (copy_size > 0) {
                    memcpy((void*)(paddr + copy_start),
                           (const uint8_t*)data + off + copy_start,
                           copy_size);
                }
            }

            pmap_map_page(pml4, va, paddr, flags);
        }
    }

    uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    for (uint64_t va = stack_bottom; va < USER_STACK_TOP; va += PAGE_SIZE) {
        uint64_t paddr = memory_alloc_physical(1);
        if (!paddr) return -1;
        pmap_map_page(pml4, va, paddr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    info->stack_top = USER_STACK_TOP;
    info->pml4 = pml4;

    if (info->is_dynamic && info->interpreter[0]) {

    }

    (void)pml4;
    return 0;
}

int elf_load_from_file(const char* path, elf_load_info_t* info)
{
    if (!path || !info) return -1;

    int fd = vfs_open(path, FS_O_RDONLY, 0);
    if (fd < 0) return -1;

    uint8_t* buf = (uint8_t*)memory_alloc(1024 * 1024);
    if (!buf) {
        vfs_close(fd);
        return -1;
    }

    int total = 0;
    int n;
    while ((n = vfs_read(fd, buf + total, 4096)) > 0) {
        total += n;
        if (total >= 1024 * 1024 - 4096) break;
    }
    vfs_close(fd);

    int ret = elf_load_from_memory(buf, total, info);
    memory_free(buf);
    return ret;
}

uint64_t elf_setup_stack(uint64_t stack_top, const char* argv[], const char* envp[],
                          const elf_load_info_t* info)
{
    uint64_t old_pml4 = (uint64_t)pmap_get();
    if (info && info->pml4) pmap_switch(info->pml4);

    int argc = 0;
    int envc = 0;
    uint64_t argv_ptrs[32];
    uint64_t env_ptrs[32];
    if (argv) while (argv[argc] && argc < 31) argc++;
    if (envp) while (envp[envc] && envc < 31) envc++;

    uint64_t sp = stack_top & ~0xFULL;
    for (int i = envc - 1; i >= 0; i--) {
        uint64_t len = strlen(envp[i]) + 1;
        sp -= len;
        memcpy((void*)sp, envp[i], len);
        env_ptrs[i] = sp;
    }
    for (int i = argc - 1; i >= 0; i--) {
        uint64_t len = strlen(argv[i]) + 1;
        sp -= len;
        memcpy((void*)sp, argv[i], len);
        argv_ptrs[i] = sp;
    }

    sp &= ~0xFULL;
    uint64_t* out = (uint64_t*)sp;
    *(--out) = 0;
    for (int i = envc - 1; i >= 0; i--) *(--out) = env_ptrs[i];
    uint64_t env_base = (uint64_t)out;
    *(--out) = 0;
    for (int i = argc - 1; i >= 0; i--) *(--out) = argv_ptrs[i];
    uint64_t argv_base = (uint64_t)out;
    *(--out) = env_base;
    *(--out) = argv_base;
    *(--out) = (uint64_t)argc;

    if (old_pml4) pmap_switch((void*)old_pml4);
    return (uint64_t)out;
}
