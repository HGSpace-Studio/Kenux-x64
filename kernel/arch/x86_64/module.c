#include <arch/memory.h>
#include <arch/elf.h>
#include <string.h>

#define MODULE_MAX          64
#define MODULE_MAX_SYMBOLS  512
#define MODULE_MAX_DEPS     8
#define MODULE_NAME_MAX     32
#define MODULE_SYMBOL_MAX   64

#define MODULE_STATE_LOADED    1
#define MODULE_STATE_RUNNING   2
#define MODULE_STATE_UNLOADING 3
#define MODULE_STATE_ERROR     4

typedef enum {
    MOD_LICENSE_GPL,
    MOD_LICENSE_GPL_V2,
    MOD_LICENSE_MIT,
    MOD_LICENSE_BSD,
    MOD_LICENSE_PROPRIETARY,
} mod_license_t;

typedef struct {
    char name[MODULE_SYMBOL_MAX];
    void* addr;
} mod_symbol_t;

typedef struct {
    char name[MODULE_NAME_MAX];
    char version[16];
    char author[32];
    char description[64];
    mod_license_t license;
    int state;
    void* base;
    uint32_t size;
    void* init_func;
    void* exit_func;
    mod_symbol_t exports[MODULE_MAX_SYMBOLS];
    uint32_t export_count;
    mod_symbol_t imports[MODULE_MAX_SYMBOLS];
    uint32_t import_count;
    char deps[MODULE_MAX_DEPS][MODULE_NAME_MAX];
    uint32_t dep_count;
    uint64_t load_time;
    uint32_t ref_count;
} module_t;

static module_t modules[MODULE_MAX];
static mod_symbol_t kernel_symbols[MODULE_MAX_SYMBOLS];
static uint32_t kernel_symbol_count = 0;
static spinlock_t module_lock = SPINLOCK_INIT;

void module_system_init(void)
{
    memset(modules, 0, sizeof(modules));
    memset(kernel_symbols, 0, sizeof(kernel_symbols));
    kernel_symbol_count = 0;
}

void module_export_symbol(const char* name, void* addr)
{
    if (!name || !addr || kernel_symbol_count >= MODULE_MAX_SYMBOLS) return;

    spinlock_acquire(&module_lock);
    mod_symbol_t* sym = &kernel_symbols[kernel_symbol_count];
    strncpy(sym->name, name, MODULE_SYMBOL_MAX - 1);
    sym->name[MODULE_SYMBOL_MAX - 1] = '\0';
    sym->addr = addr;
    kernel_symbol_count++;
    spinlock_release(&module_lock);
}

static void* module_lookup_symbol(const char* name)
{
    if (!name) return NULL;

    for (uint32_t i = 0; i < kernel_symbol_count; i++) {
        if (strcmp(kernel_symbols[i].name, name) == 0) return kernel_symbols[i].addr;
    }

    for (int m = 0; m < MODULE_MAX; m++) {
        if (modules[m].state == MODULE_STATE_RUNNING) {
            for (uint32_t i = 0; i < modules[m].export_count; i++) {
                if (strcmp(modules[m].exports[i].name, name) == 0)
                    return modules[m].exports[i].addr;
            }
        }
    }

    return NULL;
}

static int module_find_free(void)
{
    for (int i = 0; i < MODULE_MAX; i++) {
        if (modules[i].state == 0) return i;
    }
    return -1;
}

static int module_find_by_name(const char* name)
{
    for (int i = 0; i < MODULE_MAX; i++) {
        if (modules[i].state != 0 && strcmp(modules[i].name, name) == 0) return i;
    }
    return -1;
}

static int module_resolve_deps(int idx)
{
    module_t* mod = &modules[idx];
    for (uint32_t i = 0; i < mod->dep_count; i++) {
        int dep = module_find_by_name(mod->deps[i]);
        if (dep < 0) return -1;
        if (modules[dep].state != MODULE_STATE_RUNNING) return -2;
    }
    return 0;
}

static int module_resolve_symbols(int idx)
{
    module_t* mod = &modules[idx];
    for (uint32_t i = 0; i < mod->import_count; i++) {
        void* addr = module_lookup_symbol(mod->imports[i].name);
        if (!addr) return -1;
        mod->imports[i].addr = addr;
    }
    return 0;
}

static int module_relocate(int idx)
{
    (void)idx;
    return 0;
}

int module_load(const void* data, uint64_t size, const char* name)
{
    if (!data || size == 0 || !name) return -1;

    spinlock_acquire(&module_lock);

    if (module_find_by_name(name) >= 0) {
        spinlock_release(&module_lock);
        return -2;
    }

    int idx = module_find_free();
    if (idx < 0) {
        spinlock_release(&module_lock);
        return -3;
    }

    module_t* mod = &modules[idx];
    memset(mod, 0, sizeof(module_t));

    mod->base = memory_alloc(size);
    if (!mod->base) {
        spinlock_release(&module_lock);
        return -4;
    }
    memcpy(mod->base, data, size);
    mod->size = (uint32_t)size;
    strncpy(mod->name, name, MODULE_NAME_MAX - 1);

    const uint8_t* base = (const uint8_t*)data;

    if (size >= 4 && base[0] == 0x7F && base[1] == 'E' && base[2] == 'L' && base[3] == 'F') {
        const elf_header_t* ehdr = (const elf_header_t*)data;
        mod->init_func = (void*)((uint64_t)mod->base + ehdr->entry);
    } else if (size >= 4) {
        uint32_t magic = *(const uint32_t*)data;
        if (magic == 0x05A4D) {
            mod->init_func = mod->base;
        } else {
            mod->init_func = mod->base;
        }
    }

    mod->state = MODULE_STATE_LOADED;

    if (module_resolve_deps(idx) != 0) {
        memory_free(mod->base);
        memset(mod, 0, sizeof(module_t));
        spinlock_release(&module_lock);
        return -5;
    }

    if (module_resolve_symbols(idx) != 0) {
        memory_free(mod->base);
        memset(mod, 0, sizeof(module_t));
        spinlock_release(&module_lock);
        return -6;
    }

    if (module_relocate(idx) != 0) {
        memory_free(mod->base);
        memset(mod, 0, sizeof(module_t));
        spinlock_release(&module_lock);
        return -7;
    }

    if (mod->init_func) {
        int (*init)(void) = (int (*)(void))mod->init_func;
        int ret = init();
        if (ret != 0) {
            memory_free(mod->base);
            memset(mod, 0, sizeof(module_t));
            spinlock_release(&module_lock);
            return -8;
        }
    }

    mod->state = MODULE_STATE_RUNNING;
    mod->ref_count = 1;

    uint64_t tsc;
    __asm__ volatile ("rdtsc" : "=A"(tsc));
    mod->load_time = tsc;

    spinlock_release(&module_lock);
    return 0;
}

int module_unload(const char* name)
{
    if (!name) return -1;

    spinlock_acquire(&module_lock);
    int idx = module_find_by_name(name);
    if (idx < 0) {
        spinlock_release(&module_lock);
        return -2;
    }

    module_t* mod = &modules[idx];
    if (mod->ref_count > 1) {
        spinlock_release(&module_lock);
        return -3;
    }

    mod->state = MODULE_STATE_UNLOADING;

    if (mod->exit_func) {
        void (*exit)(void) = (void (*)(void))mod->exit_func;
        exit();
    }

    if (mod->base) memory_free(mod->base);
    memset(mod, 0, sizeof(module_t));

    spinlock_release(&module_lock);
    return 0;
}

int module_get_info(const char* name, char* buf, int buf_len)
{
    if (!name || !buf) return -1;

    spinlock_acquire(&module_lock);
    int idx = module_find_by_name(name);
    if (idx < 0) {
        spinlock_release(&module_lock);
        return -2;
    }

    module_t* mod = &modules[idx];
    int len = 0;
    len += snprintf(buf + len, buf_len - len, "Name: %s\n", mod->name);
    len += snprintf(buf + len, buf_len - len, "Version: %s\n", mod->version[0] ? mod->version : "unknown");
    len += snprintf(buf + len, buf_len - len, "Author: %s\n", mod->author[0] ? mod->author : "unknown");
    len += snprintf(buf + len, buf_len - len, "Description: %s\n", mod->description[0] ? mod->description : "none");
    len += snprintf(buf + len, buf_len - len, "Size: %u bytes\n", mod->size);
    len += snprintf(buf + len, buf_len - len, "State: %s\n",
                   mod->state == MODULE_STATE_RUNNING ? "running" : "loaded");
    len += snprintf(buf + len, buf_len - len, "Exports: %u\n", mod->export_count);
    len += snprintf(buf + len, buf_len - len, "Imports: %u\n", mod->import_count);
    len += snprintf(buf + len, buf_len - len, "Refs: %u\n", mod->ref_count);

    spinlock_release(&module_lock);
    return len;
}

int module_list(char names[][MODULE_NAME_MAX], int max)
{
    if (!names) return -1;

    spinlock_acquire(&module_lock);
    int count = 0;
    for (int i = 0; i < MODULE_MAX && count < max; i++) {
        if (modules[i].state != 0) {
            strncpy(names[count], modules[i].name, MODULE_NAME_MAX - 1);
            names[count][MODULE_NAME_MAX - 1] = '\0';
            count++;
        }
    }
    spinlock_release(&module_lock);
    return count;
}