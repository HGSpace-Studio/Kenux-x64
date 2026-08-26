

#include <arch/ldso.h>
#include <arch/elf.h>
#include <arch/memory.h>
#include <arch/fs.h>
#include <arch/spinlock.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

static int ldso_read_elf_header(int fd, Elf64_Ehdr* ehdr)
{

    if (vfs_lseek(fd, 0, 0) != 0)
        return -1;
    if (vfs_read(fd, ehdr, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr))
        return -1;
    return 0;
}

static int ldso_read_at(int fd, uint64_t offset, void* buf, uint64_t size)
{
    if (vfs_lseek(fd, offset, 0) != (int64_t)offset)
        return -1;
    if (vfs_read(fd, buf, size) != (int)size)
        return -1;
    return 0;
}

static uint32_t ldso_elf_hash(const char* name)
{
    uint32_t h = 0;
    uint8_t  high;

    while (*name) {
        h = (h << 4) + (uint8_t)(*name);
        high = h & 0xF0000000U;
        if (high)
            h ^= high >> 24;
        h &= ~high;
        name++;
    }
    return h;
}

static int ldso_verify_elf(const Elf64_Ehdr* ehdr)
{

    if (ehdr->e_ident[0] != 0x7f ||
        ehdr->e_ident[1] != 'E'  ||
        ehdr->e_ident[2] != 'L'  ||
        ehdr->e_ident[3] != 'F')
        return -1;

    if (ehdr->e_ident[4] != ELFCLASS64)
        return -1;
    if (ehdr->e_ident[5] != ELFDATA2LSB)
        return -1;
    if (ehdr->e_machine != EM_X86_64)
        return -1;

    if (ehdr->e_type != ET_DYN)
        return -1;

    return 0;
}

static klib_t* ldso_find_loaded(const char* name, ldso_state_t* state)
{

    const char* p = name;
    const char* fname = p;
    while (*p) {
        if (*p == '/')
            fname = p + 1;
        p++;
    }

    for (uint32_t i = 0; i < state->lib_count; i++) {
        const char* loaded_fname = state->libs[i].name;
        const char* lf = loaded_fname;
        while (*lf) {
            if (*lf == '/')
                loaded_fname = lf + 1;
            lf++;
        }
        if (strcmp(loaded_fname, fname) == 0)
            return &state->libs[i];
    }
    return NULL;
}

static int ldso_parse_dynamic(klib_t* lib, const uint8_t* dyn_data, uint64_t dyn_size)
{
    uint64_t count = dyn_size / sizeof(kelf_dyn_t);

    for (uint64_t i = 0; i < count; i++) {
        const kelf_dyn_t* dyn = (const kelf_dyn_t*)(dyn_data + i * sizeof(kelf_dyn_t));

        if (dyn->d_tag == DT_NULL)
            break;

        switch (dyn->d_tag) {
        case DT_SYMTAB:

            lib->symtab = (kelf_sym_t*)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_STRTAB:

            lib->strtab = (const char*)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_STRSZ:
            lib->strtab_size = dyn->d_un.d_val;
            break;

        case DT_HASH: {

            uint64_t hash_addr = lib->base_addr + dyn->d_un.d_ptr;
            lib->hash = (kelf_hash_t*)hash_addr;
            lib->hash_nbucket = lib->hash->nbucket;
            lib->hash_nchain = lib->hash->nchain;

            lib->sym_count = lib->hash_nchain;
            break;
        }

        case DT_JMPREL:

            lib->plt_rels = (kelf_rela_t*)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_PLTRELSZ:
            lib->plt_relsz = dyn->d_un.d_val;
            break;

        case DT_REL:

            lib->rels = (kelf_rela_t*)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_RELSZ:
            lib->relsz = dyn->d_un.d_val;
            break;

        case DT_INIT:
            lib->init_func = (void(*)(void))(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_FINI:
            lib->fini_func = (void(*)(void))(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_INIT_ARRAY:
            lib->init_array = (void**)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_INIT_ARRAYSZ:
            lib->init_array_sz = (uint32_t)(dyn->d_un.d_val / sizeof(void*));
            break;

        case DT_FINI_ARRAY:
            lib->fini_array = (void**)(lib->base_addr + dyn->d_un.d_ptr);
            break;

        case DT_FINI_ARRAYSZ:
            lib->fini_array_sz = (uint32_t)(dyn->d_un.d_val / sizeof(void*));
            break;

        default:
            break;
        }
    }

    return 0;
}

static int ldso_load_dependencies(klib_t* lib, ldso_state_t* state,
                                   const uint8_t* dyn_data, uint64_t dyn_size)
{
    uint64_t count = dyn_size / sizeof(kelf_dyn_t);

    for (uint64_t i = 0; i < count; i++) {
        const kelf_dyn_t* dyn = (const kelf_dyn_t*)(dyn_data + i * sizeof(kelf_dyn_t));

        if (dyn->d_tag == DT_NULL)
            break;

        if (dyn->d_tag == DT_NEEDED) {

            const char* dep_name = lib->strtab + dyn->d_un.d_val;
            if (dep_name[0] == '\0')
                continue;

            char dep_path[512];

            if (dep_name[0] == '/') {
                if (strlen(dep_name) >= sizeof(dep_path))
                    continue;
                memcpy(dep_path, dep_name, strlen(dep_name) + 1);
            } else {

                int n = snprintf(dep_path, sizeof(dep_path), "/lib/%s", dep_name);
                if (n < 0 || (size_t)n >= sizeof(dep_path))
                    continue;
            }

            spin_lock(&state->lock);
            klib_t* existing = ldso_find_loaded(dep_path, state);
            if (existing) {
                existing->ref_count++;
                spin_unlock(&state->lock);
                continue;
            }
            spin_unlock(&state->lock);

            klib_t* dep = ldso_load_library(dep_path, state);
            if (!dep) {

                continue;
            }
        }
    }

    return 0;
}

static ldso_state_t g_ldso_state;

void ldso_init_state(ldso_state_t* state)
{
    if (!state)
        return;

    memset(state, 0, sizeof(ldso_state_t));
    spin_init(&state->lock);
    state->lib_count = 0;
    state->total_relocs = 0;
    state->total_lookups = 0;
    state->cache_hits = 0;

    for (uint32_t i = 0; i < LDSO_SYM_CACHE_SIZE; i++) {
        state->sym_cache[i].name = NULL;
        state->sym_cache[i].addr = 0;
        state->sym_cache[i].owner = NULL;
        state->sym_cache[i].hits = 0;
    }
}

ldso_state_t* ldso_get_state(void)
{
    return &g_ldso_state;
}

klib_t* ldso_load_library(const char* path, ldso_state_t* state)
{
    if (!path || !state)
        return NULL;

    Elf64_Ehdr ehdr;

    int fd = vfs_open(path, FS_O_RDONLY, 0);
    if (fd < 0)
        return NULL;

    if (ldso_read_elf_header(fd, &ehdr) < 0) {
        vfs_close(fd);
        return NULL;
    }

    if (ldso_verify_elf(&ehdr) < 0) {
        vfs_close(fd);
        return NULL;
    }

    spin_lock(&state->lock);
    if (state->lib_count >= LDSO_MAX_LIBS) {
        spin_unlock(&state->lock);
        vfs_close(fd);
        return NULL;
    }

    klib_t* existing = ldso_find_loaded(path, state);
    if (existing) {
        existing->ref_count++;
        spin_unlock(&state->lock);
        vfs_close(fd);
        return existing;
    }

    klib_t* lib = &state->libs[state->lib_count];
    state->lib_count++;
    spin_unlock(&state->lock);

    memset(lib, 0, sizeof(klib_t));
    strncpy(lib->name, path, LDSO_LIB_NAME_MAX - 1);
    lib->fd = fd;
    lib->ref_count = 1;
    lib->resolved = 0;
    lib->initialized = 0;

    uint64_t phdr_size = (uint64_t)ehdr.e_phnum * ehdr.e_phentsize;
    Elf64_Phdr* phdrs = (Elf64_Phdr*)memory_zalloc(phdr_size);
    if (!phdrs) {
        vfs_close(fd);

        spin_lock(&state->lock);
        state->lib_count--;
        spin_unlock(&state->lock);
        return NULL;
    }

    if (ldso_read_at(fd, ehdr.e_phoff, phdrs, phdr_size) < 0) {
        memory_free(phdrs);
        vfs_close(fd);
        spin_lock(&state->lock);
        state->lib_count--;
        spin_unlock(&state->lock);
        return NULL;
    }

    uint64_t vaddr_low = UINT64_MAX;
    uint64_t vaddr_high = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t seg_start = phdrs[i].p_vaddr;
            uint64_t seg_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;

            seg_start &= ~(uint64_t)(PAGE_SIZE - 1);
            seg_end = (seg_end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

            if (seg_start < vaddr_low)
                vaddr_low = seg_start;
            if (seg_end > vaddr_high)
                vaddr_high = seg_end;
        }
    }

    if (vaddr_low == UINT64_MAX || vaddr_high <= vaddr_low) {
        memory_free(phdrs);
        vfs_close(fd);
        spin_lock(&state->lock);
        state->lib_count--;
        spin_unlock(&state->lock);
        return NULL;
    }

    lib->load_size = vaddr_high - vaddr_low;

    void* pml4 = pmap_get();
    vma_t process_vma;
    vma_init(&process_vma);

    uint64_t map_base = vma_alloc_range(&process_vma, lib->load_size, PAGE_SIZE);
    if (map_base == 0) {

        map_base = (uint64_t)memory_alloc_aligned(lib->load_size, PAGE_SIZE);
        if (map_base == 0) {
            memory_free(phdrs);
            vfs_close(fd);
            spin_lock(&state->lock);
            state->lib_count--;
            spin_unlock(&state->lock);
            return NULL;
        }
    }

    lib->base_addr = map_base;
    lib->map_start = map_base;
    lib->map_end = map_base + lib->load_size;

    memset((void*)map_base, 0, lib->load_size);

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t seg_vaddr = phdrs[i].p_vaddr;
            uint64_t seg_offset = phdrs[i].p_offset;
            uint64_t seg_filesz = phdrs[i].p_filesz;
            uint64_t seg_memsz = phdrs[i].p_memsz;

            uint64_t page_aligned_vaddr = seg_vaddr & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t page_offset = seg_vaddr - page_aligned_vaddr;
            uint64_t dest_addr = map_base + page_aligned_vaddr + page_offset;

            if (seg_filesz > 0) {
                if (ldso_read_at(fd, seg_offset, (void*)dest_addr, seg_filesz) < 0) {

                    memory_free(phdrs);
                    vfs_close(fd);
                    spin_lock(&state->lock);
                    state->lib_count--;
                    spin_unlock(&state->lock);
                    return NULL;
                }
            }

            uint64_t map_flags = PAGE_PRESENT | PAGE_USER | PAGE_GLOBAL;
            if (phdrs[i].p_flags & PF_W)
                map_flags |= PAGE_WRITABLE;
            if (!(phdrs[i].p_flags & PF_X))
                map_flags |= PAGE_NO_EXEC;

            uint64_t start_pfn = (dest_addr) >> PAGE_SHIFT;
            uint64_t end_pfn = (dest_addr + seg_memsz + PAGE_SIZE - 1) >> PAGE_SHIFT;

            for (uint64_t pfn = start_pfn; pfn < end_pfn; pfn++) {
                uint64_t vaddr = pfn << PAGE_SHIFT;
                uint64_t paddr = vaddr;
                pmap_map_page(pml4, vaddr, paddr, map_flags);
            }
        }
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == 0x2  ) {
            uint64_t dyn_offset = phdrs[i].p_offset;
            uint64_t dyn_filesz = phdrs[i].p_filesz;

            uint8_t* dyn_buf = (uint8_t*)memory_zalloc(dyn_filesz);
            if (!dyn_buf) {
                memory_free(phdrs);
                vfs_close(fd);
                spin_lock(&state->lock);
                state->lib_count--;
                spin_unlock(&state->lock);
                return NULL;
            }

            if (ldso_read_at(fd, dyn_offset, dyn_buf, dyn_filesz) < 0) {
                memory_free(dyn_buf);
                memory_free(phdrs);
                vfs_close(fd);
                spin_lock(&state->lock);
                state->lib_count--;
                spin_unlock(&state->lock);
                return NULL;
            }

            ldso_parse_dynamic(lib, dyn_buf, dyn_filesz);

            ldso_load_dependencies(lib, state, dyn_buf, dyn_filesz);

            memory_free(dyn_buf);
            break;
        }
    }

    memory_free(phdrs);
    vfs_close(fd);
    lib->fd = -1;

    ldso_relocate(lib, state);

    return lib;
}

uint64_t ldso_lookup_symbol(const char* name, ldso_state_t* state)
{
    if (!name || !state || name[0] == '\0')
        return 0;

    state->total_lookups++;

    uint32_t cache_idx = ldso_elf_hash(name) % LDSO_SYM_CACHE_SIZE;
    sym_cache_entry_t* cached = &state->sym_cache[cache_idx];

    if (cached->name && cached->addr != 0 && strcmp(cached->name, name) == 0) {

        cached->hits++;
        state->cache_hits++;
        return cached->addr;
    }

    spin_lock(&state->lock);

    uint64_t found_addr = 0;
    klib_t* found_owner = NULL;

    for (uint32_t i = 0; i < state->lib_count; i++) {
        klib_t* lib = &state->libs[i];

        if (!lib->symtab || !lib->hash || lib->hash_nbucket == 0)
            continue;

        uint32_t hash = ldso_elf_hash(name);
        uint32_t bucket_idx = hash % lib->hash_nbucket;

        uint32_t* buckets = (uint32_t*)((uint8_t*)lib->hash + sizeof(kelf_hash_t));
        uint32_t* chains  = (uint32_t*)((uint8_t*)buckets + lib->hash_nbucket * sizeof(uint32_t));

        uint32_t sym_idx = buckets[bucket_idx];
        while (sym_idx != 0) {
            if (sym_idx >= lib->sym_count)
                break;

            const kelf_sym_t* sym = &lib->symtab[sym_idx];

            if (sym->st_shndx != 0 && sym->st_name != 0) {
                const char* sym_name = lib->strtab + sym->st_name;
                if (strcmp(sym_name, name) == 0) {

                    uint8_t bind = KELF_ST_BIND(sym->st_info);
                    uint64_t sym_addr = lib->base_addr + sym->st_value;

                    if (bind == KELF_STB_GLOBAL) {

                        found_addr = sym_addr;
                        found_owner = lib;
                        goto found;
                    } else if (bind == KELF_STB_WEAK && found_addr == 0) {

                        found_addr = sym_addr;
                        found_owner = lib;
                    }
                }
            }

            sym_idx = chains[sym_idx];
        }
    }

found:
    spin_unlock(&state->lock);

    if (found_addr != 0) {
        cached->name = name;
        cached->addr = found_addr;
        cached->owner = found_owner;
        cached->hits = 1;
    }

    return found_addr;
}

int ldso_relocate(klib_t* lib, ldso_state_t* state)
{
    if (!lib || !state || lib->resolved)
        return 0;

    uint64_t base = lib->base_addr;

    if (lib->plt_rels && lib->plt_relsz > 0) {
        uint64_t plt_count = lib->plt_relsz / sizeof(kelf_rela_t);

        for (uint64_t i = 0; i < plt_count && i < LDSO_MAX_RELS; i++) {
            const kelf_rela_t* rel = &lib->plt_rels[i];
            uint32_t sym_idx = KELF_R_SYM(rel->r_info);
            uint32_t rel_type = KELF_R_TYPE(rel->r_info);
            uint64_t* target = (uint64_t*)(base + rel->r_offset);

            switch (rel_type) {
            case R_KENUX_GLOB_DAT: {

                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                if (sym_addr == 0) {

                    sym_addr = 0;
                }
                *target = sym_addr;
                state->total_relocs++;
                break;
            }

            case R_KENUX_JUMP_SLOT: {

                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                if (sym_addr == 0) {
                    sym_addr = 0;
                }
                *target = sym_addr;
                state->total_relocs++;
                break;
            }

            case R_KENUX_RELATIVE: {

                *target = base + rel->r_addend;
                state->total_relocs++;
                break;
            }

            case R_KENUX_64: {

                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                *target = sym_addr + rel->r_addend;
                state->total_relocs++;
                break;
            }

            case R_KENUX_PC32: {

                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                uint32_t* target32 = (uint32_t*)target;
                *target32 = (uint32_t)(sym_addr + rel->r_addend - (uint64_t)target);
                state->total_relocs++;
                break;
            }

            default:

                break;
            }
        }
    }

    if (lib->rels && lib->relsz > 0) {
        uint64_t rel_count = lib->relsz / sizeof(kelf_rela_t);

        for (uint64_t i = 0; i < rel_count && i < LDSO_MAX_RELS; i++) {
            const kelf_rela_t* rel = &lib->rels[i];
            uint32_t sym_idx = KELF_R_SYM(rel->r_info);
            uint32_t rel_type = KELF_R_TYPE(rel->r_info);
            uint64_t* target = (uint64_t*)(base + rel->r_offset);

            switch (rel_type) {
            case R_KENUX_GLOB_DAT: {
                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                *target = sym_addr;
                state->total_relocs++;
                break;
            }

            case R_KENUX_RELATIVE: {
                *target = base + rel->r_addend;
                state->total_relocs++;
                break;
            }

            case R_KENUX_64: {
                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                *target = sym_addr + rel->r_addend;
                state->total_relocs++;
                break;
            }

            case R_KENUX_PC32: {
                const char* sym_name = lib->strtab + lib->symtab[sym_idx].st_name;
                uint64_t sym_addr = ldso_lookup_symbol(sym_name, state);
                uint32_t* target32 = (uint32_t*)target;
                *target32 = (uint32_t)(sym_addr + rel->r_addend - (uint64_t)target);
                state->total_relocs++;
                break;
            }

            case R_KENUX_JUMP_SLOT:

            default:
                break;
            }
        }
    }

    lib->resolved = 1;
    return 0;
}

void ldso_call_init(ldso_state_t* state)
{
    if (!state)
        return;

    spin_lock(&state->lock);

    for (uint32_t i = 0; i < state->lib_count; i++) {
        klib_t* lib = &state->libs[i];

        if (lib->initialized)
            continue;

        lib->initialized = 1;

        if (lib->init_func) {
            void (*init_fn)(void) = lib->init_func;
            spin_unlock(&state->lock);
            init_fn();
            spin_lock(&state->lock);
        }

        if (lib->init_array && lib->init_array_sz > 0) {
            for (uint32_t j = 0; j < lib->init_array_sz; j++) {
                void (*fn)(void) = (void(*)(void))lib->init_array[j];
                if (fn) {
                    spin_unlock(&state->lock);
                    fn();
                    spin_lock(&state->lock);
                }
            }
        }
    }

    spin_unlock(&state->lock);
}

void ldso_call_fini(ldso_state_t* state)
{
    if (!state)
        return;

    spin_lock(&state->lock);

    for (int32_t i = (int32_t)state->lib_count - 1; i >= 0; i--) {
        klib_t* lib = &state->libs[i];

        if (lib->fini_array && lib->fini_array_sz > 0) {
            for (uint32_t j = 0; j < lib->fini_array_sz; j++) {
                void (*fn)(void) = (void(*)(void))lib->fini_array[j];
                if (fn) {
                    spin_unlock(&state->lock);
                    fn();
                    spin_lock(&state->lock);
                }
            }
        }

        if (lib->fini_func) {
            void (*fini_fn)(void) = lib->fini_func;
            spin_unlock(&state->lock);
            fini_fn();
            spin_lock(&state->lock);
        }
    }

    spin_unlock(&state->lock);
}

int ldso_unload_library(klib_t* lib, ldso_state_t* state)
{
    if (!lib || !state)
        return -1;

    spin_lock(&state->lock);

    lib->ref_count--;

    if (lib->ref_count > 0) {

        spin_unlock(&state->lock);
        return 0;
    }

    if (lib->initialized) {
        if (lib->fini_func) {
            void (*fini_fn)(void) = lib->fini_func;
            spin_unlock(&state->lock);
            fini_fn();
            spin_lock(&state->lock);
        }

        if (lib->fini_array && lib->fini_array_sz > 0) {
            for (uint32_t j = 0; j < lib->fini_array_sz; j++) {
                void (*fn)(void) = (void(*)(void))lib->fini_array[j];
                if (fn) {
                    spin_unlock(&state->lock);
                    fn();
                    spin_lock(&state->lock);
                }
            }
        }
    }

    if (lib->map_start != 0 && lib->map_end > lib->map_start) {
        void* pml4 = pmap_get();

        uint64_t start_pfn = lib->map_start >> PAGE_SHIFT;
        uint64_t end_pfn = lib->map_end >> PAGE_SHIFT;

        for (uint64_t pfn = start_pfn; pfn < end_pfn; pfn++) {
            uint64_t vaddr = pfn << PAGE_SHIFT;
            pmap_unmap_page(pml4, vaddr);
        }

        memory_free((void*)lib->map_start);
    }

    uint32_t idx = (uint32_t)(lib - state->libs);
    if (idx < state->lib_count - 1) {

        memcpy(&state->libs[idx], &state->libs[state->lib_count - 1],
               sizeof(klib_t));
    }
    state->lib_count--;

    for (uint32_t i = 0; i < LDSO_SYM_CACHE_SIZE; i++) {
        if (state->sym_cache[i].owner == lib) {
            state->sym_cache[i].name = NULL;
            state->sym_cache[i].addr = 0;
            state->sym_cache[i].owner = NULL;
            state->sym_cache[i].hits = 0;
        }
    }

    spin_unlock(&state->lock);

    __asm__ volatile ("invlpg (%0)" : : "r"(lib->map_start) : "memory");

    return 0;
}
