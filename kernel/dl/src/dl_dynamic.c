#include "dl_dynamic.h"
#include <arch/memory.h>
#include <string.h>

uint32_t dl_elf_hash(const char* name)
{
    if (!name) return 0;
    uint32_t h = 0, g;
    while (*name) {
        h = (h << 4) + (uint8_t)(*name++);
        g = h & 0xF0000000;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

void dl_init(dl_context_t* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(dl_context_t));
    spin_init(&ctx->lock);
}

void dl_add_search_path(dl_context_t* ctx, const char* path)
{
    if (!ctx || !path) return;
    spinlock_acquire(&ctx->lock);
    if (ctx->search_path_count < 64) {
        ctx->search_paths[ctx->search_path_count++] = (char*)path;
    }
    spinlock_release(&ctx->lock);
}

static elf_sym_t* dl_sym_lookup_local(dl_module_t* mod, const char* name)
{
    if (!mod || !mod->symtab || !mod->strtab || !mod->hash || !name) return NULL;

    uint32_t h = dl_elf_hash(name);
    uint32_t nbuckets = mod->hash[0];
    uint32_t nchain = mod->hash[1];
    uint32_t* buckets = &mod->hash[2];
    uint32_t* chain = &mod->hash[2 + nbuckets];

    uint32_t idx = buckets[h % nbuckets];
    while (idx != 0 && idx < nchain) {
        const char* sym_name = mod->strtab + mod->symtab[idx].name;
        if (strcmp(sym_name, name) == 0) return &mod->symtab[idx];
        idx = chain[idx];
    }
    return NULL;
}

elf_sym_t* dl_lookup_symbol(dl_context_t* ctx, const char* name, dl_module_t** out_mod)
{
    if (!ctx || !name) return NULL;

    uint32_t h = dl_elf_hash(name);
    uint32_t bucket = h % DL_HASH_SIZE;

    spinlock_acquire(&ctx->lock);
    dl_module_t* mod = ctx->hash_table[bucket];
    while (mod) {
        spinlock_acquire(&mod->lock);
        elf_sym_t* sym = dl_sym_lookup_local(mod, name);
        if (sym && sym->shndx != 0) {
            if (out_mod) *out_mod = mod;
            spinlock_release(&mod->lock);
            spinlock_release(&ctx->lock);
            return sym;
        }
        spinlock_release(&mod->lock);
        mod = mod->hash_next;
    }
    spinlock_release(&ctx->lock);
    return NULL;
}

static int dl_process_dynamic(dl_module_t* mod, elf_dyn_t* dynamic)
{
    if (!mod || !dynamic) return -1;

    mod->dynamic = dynamic;
    for (elf_dyn_t* d = dynamic; d->tag != ELF_DT_NULL; d++) {
        switch (d->tag) {
        case ELF_DT_STRTAB:   mod->strtab = (const char*)(mod->base_addr + d->val); break;
        case ELF_DT_SYMTAB:   mod->symtab = (elf_sym_t*)(mod->base_addr + d->val); break;
        case ELF_DT_HASH:     mod->hash = (uint32_t*)(mod->base_addr + d->val); break;
        case ELF_DT_PLTGOT:   mod->got = (void*)(mod->base_addr + d->val); break;
        case ELF_DT_JMPREL:   mod->pltrel_offset = d->val; break;
        case ELF_DT_PLTRELSZ: mod->pltrel_size = d->val; break;
        case ELF_DT_PLTREL:   mod->pltrel_type = (int)d->val; break;
        case ELF_DT_RELA:     mod->rela_offset = d->val; break;
        case ELF_DT_RELASZ:   mod->rela_size = d->val; break;
        case ELF_DT_RELAENT:  break;
        case ELF_DT_REL:      mod->rel_offset = d->val; break;
        case ELF_DT_RELSZ:    mod->rel_size = d->val; break;
        case ELF_DT_RELENT:   break;
        case ELF_DT_INIT:     mod->init_addr = d->val; break;
        case ELF_DT_FINI:     mod->fini_addr = d->val; break;
        case ELF_DT_INIT_ARRAY:     mod->init_array = (uint64_t*)(mod->base_addr + d->val); break;
        case ELF_DT_INIT_ARRAYSZ:   mod->init_array_size = d->val; break;
        case ELF_DT_FINI_ARRAY:     mod->fini_array = (uint64_t*)(mod->base_addr + d->val); break;
        case ELF_DT_FINI_ARRAYSZ:   mod->fini_array_size = d->val; break;
        case ELF_DT_NEEDED: break;
        case ELF_DT_SONAME: break;
        default: break;
        }
    }
    return 0;
}

static int dl_apply_rela(dl_context_t* ctx, dl_module_t* mod, elf_rela_t* rela, uint64_t count)
{
    if (!ctx || !mod || !rela) return -1;

    for (uint64_t i = 0; i < count; i++) {
        uint32_t type = (uint32_t)(rela[i].info & 0xFFFFFFFF);
        uint32_t sym_idx = (uint32_t)(rela[i].info >> 32);
        uint64_t* target = (uint64_t*)(mod->base_addr + rela[i].offset);
        uint64_t S = 0;
        uint64_t A = (uint64_t)rela[i].addend;

        if (sym_idx != 0 && type != ELF_R_X86_64_RELATIVE) {
            if (!mod->symtab || !mod->strtab) return -2;
            const char* sym_name = mod->strtab + mod->symtab[sym_idx].name;
            dl_module_t* def_mod = NULL;
            elf_sym_t* sym = dl_lookup_symbol(ctx, sym_name, &def_mod);
            if (sym && def_mod) {
                S = (uint64_t)def_mod->base_addr + sym->value;
            } else if (mod->symtab[sym_idx].shndx != 0) {
                S = (uint64_t)mod->base_addr + mod->symtab[sym_idx].value;
            } else {
                continue;
            }
        }

        switch (type) {
        case ELF_R_X86_64_NONE:
            break;
        case ELF_R_X86_64_64:
            *target = S + A;
            break;
        case ELF_R_X86_64_PC32: {
            int32_t val = (int32_t)(S + A - (uint64_t)target);
            *(int32_t*)target = val;
            break;
        }
        case ELF_R_X86_64_RELATIVE:
            *target = (uint64_t)mod->base_addr + A;
            break;
        case ELF_R_X86_64_GLOB_DAT:
            *target = S;
            break;
        case ELF_R_X86_64_JMP_SLOT:
            *target = S;
            break;
        case ELF_R_X86_64_COPY:
            if (sym_idx != 0) {
                memcpy(target, (void*)S, mod->symtab[sym_idx].size);
            }
            break;
        case ELF_R_X86_64_32: {
            uint32_t val = (uint32_t)(S + A);
            *(uint32_t*)target = val;
            break;
        }
        case ELF_R_X86_64_32S: {
            int32_t val = (int32_t)(S + A);
            *(int32_t*)target = val;
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

static int dl_apply_rel(dl_context_t* ctx, dl_module_t* mod, elf_rel_t* rel, uint64_t count)
{
    if (!ctx || !mod || !rel) return -1;

    for (uint64_t i = 0; i < count; i++) {
        uint32_t type = (uint32_t)(rel[i].info & 0xFFFFFFFF);
        uint32_t sym_idx = (uint32_t)(rel[i].info >> 32);
        uint64_t* target = (uint64_t*)(mod->base_addr + rel[i].offset);

        if (type == ELF_R_X86_64_RELATIVE) {
            *target += (uint64_t)mod->base_addr;
        } else if (sym_idx != 0) {
            if (!mod->symtab || !mod->strtab) return -2;
            const char* sym_name = mod->strtab + mod->symtab[sym_idx].name;
            dl_module_t* def_mod = NULL;
            elf_sym_t* sym = dl_lookup_symbol(ctx, sym_name, &def_mod);
            if (sym && def_mod) {
                uint64_t S = (uint64_t)def_mod->base_addr + sym->value;
                if (type == ELF_R_X86_64_64) *target = S;
                else if (type == ELF_R_X86_64_GLOB_DAT) *target = S;
                else if (type == ELF_R_X86_64_JMP_SLOT) *target = S;
            }
        }
    }
    return 0;
}

int dl_relocate(dl_context_t* ctx, dl_module_t* mod)
{
    if (!ctx || !mod) return -1;

    if (mod->rela_size > 0 && mod->rela_offset > 0) {
        elf_rela_t* rela = (elf_rela_t*)(mod->base_addr + mod->rela_offset);
        uint64_t count = mod->rela_size / sizeof(elf_rela_t);
        int result = dl_apply_rela(ctx, mod, rela, count);
        if (result != 0) return result;
    }

    if (mod->rel_size > 0 && mod->rel_offset > 0) {
        elf_rel_t* rel = (elf_rel_t*)(mod->base_addr + mod->rel_offset);
        uint64_t count = mod->rel_size / sizeof(elf_rel_t);
        int result = dl_apply_rel(ctx, mod, rel, count);
        if (result != 0) return result;
    }

    if (mod->pltrel_size > 0 && mod->pltrel_offset > 0) {
        if (mod->pltrel_type == ELF_DT_RELA) {
            elf_rela_t* rela = (elf_rela_t*)(mod->base_addr + mod->pltrel_offset);
            uint64_t count = mod->pltrel_size / sizeof(elf_rela_t);
            int result = dl_apply_rela(ctx, mod, rela, count);
            if (result != 0) return result;
        } else {
            elf_rel_t* rel = (elf_rel_t*)(mod->base_addr + mod->pltrel_offset);
            uint64_t count = mod->pltrel_size / sizeof(elf_rel_t);
            int result = dl_apply_rel(ctx, mod, rel, count);
            if (result != 0) return result;
        }
    }

    return 0;
}

void dl_call_init(dl_module_t* mod)
{
    if (!mod) return;

    if (mod->init_array && mod->init_array_size > 0) {
        uint64_t count = mod->init_array_size / sizeof(uint64_t);
        for (uint64_t i = 0; i < count; i++) {
            void (*init_fn)(void) = (void(*)(void))(mod->base_addr + mod->init_array[i]);
            if (init_fn) init_fn();
        }
    }

    if (mod->init_addr != 0) {
        void (*init_fn)(void) = (void(*)(void))(mod->base_addr + mod->init_addr);
        if (init_fn) init_fn();
    }
}

void dl_call_fini(dl_module_t* mod)
{
    if (!mod) return;

    if (mod->fini_addr != 0) {
        void (*fini_fn)(void) = (void(*)(void))(mod->base_addr + mod->fini_addr);
        if (fini_fn) fini_fn();
    }

    if (mod->fini_array && mod->fini_array_size > 0) {
        uint64_t count = mod->fini_array_size / sizeof(uint64_t);
        for (uint64_t i = count; i > 0; i--) {
            void (*fini_fn)(void) = (void(*)(void))(mod->base_addr + mod->fini_array[i - 1]);
            if (fini_fn) fini_fn();
        }
    }
}

dl_module_t* dl_load(dl_context_t* ctx, const char* name, int flags)
{
    if (!ctx || !name) return NULL;

    spinlock_acquire(&ctx->lock);

    uint32_t h = dl_elf_hash(name);
    uint32_t bucket = h % DL_HASH_SIZE;
    dl_module_t* existing = ctx->hash_table[bucket];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            existing->ref_count++;
            spinlock_release(&ctx->lock);
            return existing;
        }
        existing = existing->hash_next;
    }

    if (ctx->module_count >= DL_MAX_LOADED) {
        spinlock_release(&ctx->lock);
        return NULL;
    }

    dl_module_t* mod = (dl_module_t*)memory_alloc(sizeof(dl_module_t));
    if (!mod) {
        spinlock_release(&ctx->lock);
        return NULL;
    }
    memset(mod, 0, sizeof(dl_module_t));
    strncpy(mod->name, name, sizeof(mod->name) - 1);
    mod->ref_count = 1;
    mod->loaded = 1;
    spin_init(&mod->lock);

    mod->hash_next = ctx->hash_table[bucket];
    ctx->hash_table[bucket] = mod;
    ctx->modules[ctx->module_count++] = mod;

    spinlock_release(&ctx->lock);
    (void)flags;
    return mod;
}

int dl_unload(dl_context_t* ctx, dl_module_t* mod)
{
    if (!ctx || !mod) return -1;

    spinlock_acquire(&ctx->lock);
    mod->ref_count--;
    if (mod->ref_count > 0) {
        spinlock_release(&ctx->lock);
        return 0;
    }

    dl_call_fini(mod);

    uint32_t h = dl_elf_hash(mod->name);
    uint32_t bucket = h % DL_HASH_SIZE;
    dl_module_t** pp = &ctx->hash_table[bucket];
    while (*pp) {
        if (*pp == mod) {
            *pp = mod->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    for (int i = 0; i < ctx->module_count; i++) {
        if (ctx->modules[i] == mod) {
            ctx->modules[i] = ctx->modules[ctx->module_count - 1];
            ctx->modules[ctx->module_count - 1] = NULL;
            ctx->module_count--;
            break;
        }
    }

    if (mod->base_addr) memory_free(mod->base_addr);
    memory_free(mod);
    spinlock_release(&ctx->lock);
    return 0;
}

void* dl_resolve(dl_context_t* ctx, dl_module_t* mod, const char* symbol)
{
    if (!ctx || !mod || !symbol) return NULL;

    spinlock_acquire(&mod->lock);
    elf_sym_t* sym = dl_sym_lookup_local(mod, symbol);
    if (sym && sym->shndx != 0) {
        void* addr = (void*)((uint64_t)mod->base_addr + sym->value);
        spinlock_release(&mod->lock);
        return addr;
    }
    spinlock_release(&mod->lock);

    dl_module_t* def_mod = NULL;
    sym = dl_lookup_symbol(ctx, symbol, &def_mod);
    if (sym && def_mod) {
        return (void*)((uint64_t)def_mod->base_addr + sym->value);
    }
    return NULL;
}

void* dl_dlopen(dl_context_t* ctx, const char* filename, int flags)
{
    if (!ctx || !filename) return NULL;
    dl_module_t* mod = dl_load(ctx, filename, flags);
    return (void*)mod;
}

int dl_dlclose(dl_context_t* ctx, void* handle)
{
    if (!ctx || !handle) return -1;
    return dl_unload(ctx, (dl_module_t*)handle);
}

void* dl_dlsym(dl_context_t* ctx, void* handle, const char* symbol)
{
    if (!ctx || !handle || !symbol) return NULL;
    return dl_resolve(ctx, (dl_module_t*)handle, symbol);
}