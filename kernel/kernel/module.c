

#include "module.h"
#include <arch/elf.h>
#include <string.h>
#include <slab.h>
#include <spinlock.h>

static module_t module_table[MODULE_MAX];
static uint32_t module_count = 0;
static spinlock_t module_lock = SPINLOCK_INIT;

#define EXPORT_MAX 256
typedef struct {
    char    name[64];
    void*   addr;
} export_entry_t;

static export_entry_t exports[EXPORT_MAX];
static uint32_t export_count = 0;

void module_init(void)
{
    memset(module_table, 0, sizeof(module_table));
    module_count = 0;
    spin_init(&module_lock);
    export_count = 0;
}

void module_export_symbol(const char* name, void* addr)
{
    if (!name || export_count >= EXPORT_MAX) return;
    spin_lock(&module_lock);
    /* 检查重复 */
    for (uint32_t i = 0; i < export_count; i++) {
        if (strcmp(exports[i].name, name) == 0) {
            exports[i].addr = addr;
            spin_unlock(&module_lock);
            return;
        }
    }
    strncpy(exports[export_count].name, name, 63);
    exports[export_count].name[63] = '\0';
    exports[export_count].addr = addr;
    export_count++;
    spin_unlock(&module_lock);
}

void* module_lookup_symbol(const char* name)
{
    if (!name) return NULL;

    /* 先在导出符号表查找 */
    for (uint32_t i = 0; i < export_count; i++) {
        if (strcmp(exports[i].name, name) == 0) {
            return exports[i].addr;
        }
    }

    spin_lock(&module_lock);
    for (uint32_t i = 0; i < module_count; i++) {
        module_t* mod = &module_table[i];
        if (mod->name[0] == '\0') continue;

        if (mod->symtab && mod->strtab) {
            Elf64_Sym* syms = (Elf64_Sym*)mod->symtab;
            uint64_t sym_count = mod->symtab_size / sizeof(Elf64_Sym);
            const char* strtab = (const char*)mod->strtab;

            for (uint64_t j = 0; j < sym_count; j++) {
                if (syms[j].st_name == 0) continue;
                if (syms[j].st_shndx == 0) continue;  /* 未定义符号 */
                const char* sym_name = strtab + syms[j].st_name;
                if (strcmp(sym_name, name) == 0) {
                    void* addr = (void*)((uint64_t)mod->text_section + syms[j].st_value);
                    spin_unlock(&module_lock);
                    return addr;
                }
            }
        }
    }
    spin_unlock(&module_lock);
    return NULL;
}

/* 根据 section index 找到对应的加载地址 */
static void* section_base(module_t* mod, uint16_t shndx, const Elf64_Shdr* shdrs,
                          const char* shstr, const uint8_t* data)
{
    (void)data;
    /* .text ~ .text* -> text_section, .data -> data_section,
       .rodata -> rodata_section, .bss -> bss_section */
    if (shndx == 0) return NULL;
    const Elf64_Shdr* sh = &shdrs[shndx];
    const char* name = shstr + sh->sh_name;

    if (sh->sh_type == 8) return mod->bss_section;  /* SHT_NOBITS */
    if (strcmp(name, ".text") == 0 || strncmp(name, ".text", 5) == 0) {
        return mod->text_section;
    }
    if (strcmp(name, ".data") == 0 || strncmp(name, ".data", 5) == 0) {
        return mod->data_section;
    }
    if (strcmp(name, ".rodata") == 0 || strncmp(name, ".rodata", 7) == 0) {
        return mod->rodata_section;
    }
    /* 默认：根据 shndx 与 .text 起始的偏移决定 */
    return mod->text_section;
}

/* 解析符号地址：未定义符号通过 module_lookup_symbol 解析；
   已定义符号基于所属 section 加载基址 + st_value */
static uint64_t resolve_symbol_value(module_t* mod, const Elf64_Sym* sym,
                                      const Elf64_Shdr* shdrs,
                                      const char* shstr,
                                      const uint8_t* data)
{
    if (sym->st_shndx == 0) {
        /* 未定义符号：查找导出表 */
        const char* name = (const char*)mod->strtab + sym->st_name;
        void* addr = module_lookup_symbol(name);
        return (uint64_t)addr;
    }
    void* base = section_base(mod, sym->st_shndx, shdrs, shstr, data);
    if (!base) return 0;
    /* bss/data/rodata 的 st_value 是相对 section 起始的偏移 */
    return (uint64_t)base + sym->st_value;
}

static int __apply_relocations(module_t* mod, const uint8_t* data, uint64_t size)
{
    (void)size;
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;

    const Elf64_Shdr* shdrs = (const Elf64_Shdr*)(data + ehdr->e_shoff);
    const Elf64_Shdr* shstrtab = &shdrs[ehdr->e_shstrndx];
    const char* shstr = (const char*)(data + shstrtab->sh_offset);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type != 4) continue;  /* SHT_RELA */

        const Elf64_Rela* relas = (const Elf64_Rela*)(data + shdrs[i].sh_offset);
        uint64_t rela_count = shdrs[i].sh_size / sizeof(Elf64_Rela);
        /* 目标 section (被重定位的 section) */
        uint32_t target_idx = shdrs[i].sh_info;
        if (target_idx >= ehdr->e_shnum) continue;
        void* target_base = section_base(mod, target_idx, shdrs, shstr, data);
        if (!target_base) continue;

        /* 符号表 */
        uint32_t symtab_idx = shdrs[i].sh_link;
        const Elf64_Sym* syms = NULL;
        const char* strtab = (const char*)mod->strtab;
        if (symtab_idx < ehdr->e_shnum) {
            syms = (const Elf64_Sym*)(data + shdrs[symtab_idx].sh_offset);
        }

        for (uint64_t r = 0; r < rela_count; r++) {
            uint64_t offset = relas[r].r_offset;
            uint32_t type = (uint32_t)(relas[r].r_info >> 32);
            uint32_t sym_idx = (uint32_t)(relas[r].r_info & 0xFFFFFFFF);
            int64_t addend = relas[r].r_addend;

            /* target 地址 = 目标 section 基址 + offset */
            uint8_t* target = (uint8_t*)target_base + offset;

            /* 解析符号值 */
            uint64_t sym_value = 0;
            if (syms && sym_idx != 0) {
                const Elf64_Sym* sym = &syms[sym_idx];
                sym_value = resolve_symbol_value(mod, sym, shdrs, shstr, data);
            }

            switch (type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_64: {
                uint64_t* p = (uint64_t*)target;
                *p = sym_value + (uint64_t)addend;
                break;
            }
            case R_X86_64_PC32:
            case R_X86_64_PLT32: {
                /* PC-relative 32-bit: S + A - P */
                uint32_t* p = (uint32_t*)target;
                int64_t val = (int64_t)sym_value + addend - (int64_t)(uint64_t)target;
                *p = (uint32_t)val;
                break;
            }
            case R_X86_64_GOTPCREL: {
                /* 简化：直接当作 PC32 处理（无 GOT） */
                uint32_t* p = (uint32_t*)target;
                int64_t val = (int64_t)sym_value + addend - (int64_t)(uint64_t)target;
                *p = (uint32_t)val;
                break;
            }
            case R_X86_64_32: {
                uint32_t* p = (uint32_t*)target;
                *p = (uint32_t)(sym_value + (uint64_t)addend);
                break;
            }
            case R_X86_64_32S: {
                int32_t* p = (int32_t*)target;
                *p = (int32_t)(int64_t)(sym_value + (uint64_t)addend);
                break;
            }
            case R_X86_64_16: {
                uint16_t* p = (uint16_t*)target;
                *p = (uint16_t)(sym_value + (uint64_t)addend);
                break;
            }
            case R_X86_64_PC16: {
                uint16_t* p = (uint16_t*)target;
                int64_t val = (int64_t)sym_value + addend - (int64_t)(uint64_t)target;
                *p = (uint16_t)val;
                break;
            }
            case R_X86_64_8: {
                uint8_t* p = (uint8_t*)target;
                *p = (uint8_t)(sym_value + (uint64_t)addend);
                break;
            }
            case R_X86_64_PC8: {
                uint8_t* p = (uint8_t*)target;
                int64_t val = (int64_t)sym_value + addend - (int64_t)(uint64_t)target;
                *p = (uint8_t)val;
                break;
            }
            case R_X86_64_PC64: {
                uint64_t* p = (uint64_t*)target;
                int64_t val = (int64_t)sym_value + addend - (int64_t)(uint64_t)target;
                *p = (uint64_t)val;
                break;
            }
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT: {
                uint64_t* p = (uint64_t*)target;
                *p = sym_value + (uint64_t)addend;
                break;
            }
            case R_X86_64_RELATIVE: {
                uint64_t* p = (uint64_t*)target;
                *p = (uint64_t)addend;  /* B + A, B=0 */
                break;
            }
            default:
                /* 未知类型：忽略 */
                break;
            }
        }
    }

    return 0;
}

int module_load(const void* data, uint64_t size, const char* name)
{
    if (!data || !name || size < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;
    if (ehdr->e_type != ET_REL) return -1;
    if (ehdr->e_machine != EM_X86_64) return -1;

    spin_lock(&module_lock);
    if (module_count >= MODULE_MAX) {
        spin_unlock(&module_lock);
        return -1;
    }

    module_t* mod = &module_table[module_count];
    memset(mod, 0, sizeof(module_t));
    strncpy(mod->name, name, MODULE_NAME_MAX - 1);
    mod->state = MODULE_STATE_COMING;

    const Elf64_Shdr* shdrs = (const Elf64_Shdr*)(data + ehdr->e_shoff);
    const Elf64_Shdr* shstrtab = &shdrs[ehdr->e_shstrndx];
    const char* shstr = (const char*)(data + shstrtab->sh_offset);

    /* 加载各 section */
    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        const char* sec_name = shstr + shdrs[i].sh_name;

        if (shdrs[i].sh_type == 1) {  /* SHT_PROGBITS */
            if (strcmp(sec_name, ".text") == 0 || strncmp(sec_name, ".text", 5) == 0) {
                /* 多个 .text* 段合并到 text_section（简化：仅第一个） */
                if (!mod->text_section) {
                    mod->text_size = shdrs[i].sh_size;
                    mod->text_section = kzalloc(shdrs[i].sh_size);
                    if (!mod->text_section) goto fail;
                    memcpy(mod->text_section, data + shdrs[i].sh_offset, shdrs[i].sh_size);
                }
            } else if (strcmp(sec_name, ".data") == 0 || strncmp(sec_name, ".data", 5) == 0) {
                if (!mod->data_section) {
                    mod->data_size = shdrs[i].sh_size;
                    mod->data_section = kzalloc(shdrs[i].sh_size);
                    if (!mod->data_section) goto fail;
                    memcpy(mod->data_section, data + shdrs[i].sh_offset, shdrs[i].sh_size);
                }
            } else if (strcmp(sec_name, ".rodata") == 0 || strncmp(sec_name, ".rodata", 7) == 0) {
                if (!mod->rodata_section) {
                    mod->rodata_size = shdrs[i].sh_size;
                    mod->rodata_section = kzalloc(shdrs[i].sh_size);
                    if (!mod->rodata_section) goto fail;
                    memcpy(mod->rodata_section, data + shdrs[i].sh_offset, shdrs[i].sh_size);
                }
            }
        } else if (shdrs[i].sh_type == 8) {  /* SHT_NOBITS (.bss) */
            if (!mod->bss_section) {
                mod->bss_size = shdrs[i].sh_size;
                mod->bss_section = kzalloc(shdrs[i].sh_size);
                if (!mod->bss_section) goto fail;
            }
        } else if (shdrs[i].sh_type == 2) {  /* SHT_SYMTAB */
            mod->symtab = kzalloc(shdrs[i].sh_size);
            if (mod->symtab) memcpy(mod->symtab, data + shdrs[i].sh_offset, shdrs[i].sh_size);
            mod->symtab_size = shdrs[i].sh_size;
        } else if (shdrs[i].sh_type == 3) {  /* SHT_STRTAB */
            /* 优先取 .strtab（链接符号字符串表），跳过 .shstrtab */
            if (strcmp(sec_name, ".shstrtab") != 0 && !mod->strtab) {
                mod->strtab = kzalloc(shdrs[i].sh_size);
                if (mod->strtab) memcpy(mod->strtab, data + shdrs[i].sh_offset, shdrs[i].sh_size);
                mod->strtab_size = shdrs[i].sh_size;
            }
        }
    }

    /* 应用重定位 */
    __apply_relocations(mod, data, size);

    /* 查找 init_module / cleanup_module 符号 */
    if (mod->symtab && mod->strtab) {
        Elf64_Sym* syms = (Elf64_Sym*)mod->symtab;
        uint64_t sym_count = mod->symtab_size / sizeof(Elf64_Sym);
        const char* strtab = (const char*)mod->strtab;

        for (uint64_t i = 0; i < sym_count; i++) {
            if (syms[i].st_name == 0) continue;
            const char* sym_name = strtab + syms[i].st_name;

            if (strcmp(sym_name, "init_module") == 0) {
                mod->init = (int (*)(void))((uint64_t)mod->text_section + syms[i].st_value);
            } else if (strcmp(sym_name, "cleanup_module") == 0) {
                mod->exit = (void (*)(void))((uint64_t)mod->text_section + syms[i].st_value);
            }
        }
    }

    mod->state = MODULE_STATE_LIVE;
    mod->ref_count = 1;
    module_count++;
    spin_unlock(&module_lock);

    /* 调用 init_module */
    if (mod->init) {
        int ret = mod->init();
        if (ret != 0) {
            /* init 失败：回滚 */
            spin_lock(&module_lock);
            mod->state = MODULE_STATE_GOING;
            if (mod->exit) {
                spin_unlock(&module_lock);
                mod->exit();
                spin_lock(&module_lock);
            }
            kfree(mod->text_section);
            kfree(mod->data_section);
            kfree(mod->bss_section);
            kfree(mod->rodata_section);
            kfree(mod->symtab);
            kfree(mod->strtab);
            memset(mod, 0, sizeof(module_t));
            module_count--;
            spin_unlock(&module_lock);
            return -1;
        }
    }

    return 0;

fail:
    if (mod->text_section) kfree(mod->text_section);
    if (mod->data_section) kfree(mod->data_section);
    if (mod->bss_section) kfree(mod->bss_section);
    if (mod->rodata_section) kfree(mod->rodata_section);
    if (mod->symtab) kfree(mod->symtab);
    if (mod->strtab) kfree(mod->strtab);
    memset(mod, 0, sizeof(module_t));
    spin_unlock(&module_lock);
    return -1;
}

int module_unload(const char* name)
{
    if (!name) return -1;

    spin_lock(&module_lock);
    for (uint32_t i = 0; i < module_count; i++) {
        if (strcmp(module_table[i].name, name) == 0) {
            module_t* mod = &module_table[i];

            if (mod->ref_count > 1) {
                spin_unlock(&module_lock);
                return -1;
            }

            mod->state = MODULE_STATE_GOING;

            if (mod->exit) {
                spin_unlock(&module_lock);
                mod->exit();
                spin_lock(&module_lock);
            }

            kfree(mod->text_section);
            kfree(mod->data_section);
            kfree(mod->bss_section);
            kfree(mod->rodata_section);
            kfree(mod->symtab);
            kfree(mod->strtab);

            for (uint32_t j = i; j < module_count - 1; j++) {
                module_table[j] = module_table[j + 1];
            }
            module_count--;
            memset(&module_table[module_count], 0, sizeof(module_t));

            spin_unlock(&module_lock);
            return 0;
        }
    }
    spin_unlock(&module_lock);
    return -1;
}

module_t* module_find(const char* name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < module_count; i++) {
        if (strcmp(module_table[i].name, name) == 0) {
            return &module_table[i];
        }
    }
    return NULL;
}

int module_get(module_t* mod)
{
    if (!mod) return -1;
    spin_lock(&module_lock);
    mod->ref_count++;
    spin_unlock(&module_lock);
    return 0;
}

int module_put(module_t* mod)
{
    if (!mod || mod->ref_count == 0) return -1;
    spin_lock(&module_lock);
    mod->ref_count--;
    spin_unlock(&module_lock);
    return 0;
}

module_t* module_get_first(void)
{
    return module_count > 0 ? &module_table[0] : NULL;
}

module_t* module_get_next(module_t* mod)
{
    if (!mod) return NULL;
    uint32_t idx = mod - module_table;
    return (idx + 1 < module_count) ? &module_table[idx + 1] : NULL;
}

/* ===== 扩展接口 ===== */

int module_register_param(module_t* mod, const char* name,
                           module_param_type_t type,
                           void* value, const char* default_val)
{
    if (!mod || !name || mod->params_count >= MODULE_PARAM_MAX) return -1;
    module_param_t* p = &mod->params[mod->params_count++];
    strncpy(p->name, name, MODULE_PARAM_LEN - 1);
    p->name[MODULE_PARAM_LEN - 1] = '\0';
    p->type = type;
    p->value = value;
    if (default_val) {
        strncpy(p->default_val, default_val, MODULE_PARAM_LEN - 1);
        p->default_val[MODULE_PARAM_LEN - 1] = '\0';
    } else {
        p->default_val[0] = '\0';
    }
    /* 应用默认值 */
    if (value && default_val) {
        module_set_param(mod, name, default_val);
    }
    return 0;
}

int module_set_param(module_t* mod, const char* name, const char* value)
{
    if (!mod || !name || !value) return -1;
    for (int i = 0; i < mod->params_count; i++) {
        if (strcmp(mod->params[i].name, name) == 0) {
            module_param_t* p = &mod->params[i];
            switch (p->type) {
            case MODULE_PARAM_INT: {
                int v = 0;
                int sign = 1;
                const char* s = value;
                if (*s == '-') { sign = -1; s++; }
                while (*s >= '0' && *s <= '9') {
                    v = v * 10 + (*s - '0');
                    s++;
                }
                *(int*)p->value = v * sign;
                break;
            }
            case MODULE_PARAM_BOOL: {
                int v = 0;
                if (value[0] == '1' || value[0] == 'y' || value[0] == 'Y' ||
                    value[0] == 't' || value[0] == 'T' || value[0] == 'T') {
                    v = 1;
                }
                *(int*)p->value = v;
                break;
            }
            case MODULE_PARAM_STR: {
                strncpy((char*)p->value, value, MODULE_PARAM_LEN - 1);
                ((char*)p->value)[MODULE_PARAM_LEN - 1] = '\0';
                break;
            }
            }
            return 0;
        }
    }
    return -1;
}

int module_add_dep(module_t* mod, const char* dep_name)
{
    if (!mod || !dep_name || mod->deps_count >= MODULE_DEPS_MAX) return -1;
    /* 避免重复 */
    for (int i = 0; i < mod->deps_count; i++) {
        if (strcmp(mod->deps[i], dep_name) == 0) return 0;
    }
    strncpy(mod->deps[mod->deps_count], dep_name, MODULE_NAME_MAX - 1);
    mod->deps[mod->deps_count][MODULE_NAME_MAX - 1] = '\0';
    mod->deps_count++;
    return 0;
}

int module_resolve_deps(module_t* mod)
{
    if (!mod) return -1;
    for (int i = 0; i < mod->deps_count; i++) {
        module_t* dep = module_find(mod->deps[i]);
        if (!dep) {
            /* 依赖未加载 */
            return -1;
        }
        module_get(dep);
    }
    return 0;
}

int module_info_count(void)
{
    return (int)module_count;
}

int module_get_info(int idx, module_t* out)
{
    if (idx < 0 || (uint32_t)idx >= module_count || !out) return -1;
    memcpy(out, &module_table[idx], sizeof(module_t));
    return 0;
}
