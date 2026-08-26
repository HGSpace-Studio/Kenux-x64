

#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define MODULE_NAME_MAX   64
#define MODULE_MAX        32
#define MODULE_PARAM_MAX  16
#define MODULE_DEPS_MAX   8
#define MODULE_PARAM_LEN  64

#define MODULE_STATE_LIVE    0
#define MODULE_STATE_COMING  1
#define MODULE_STATE_GOING   2

/* ELF 重定位类型 (x86_64) */
#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_PC32       2
#define R_X86_64_GOT32      3
#define R_X86_64_PLT32      4
#define R_X86_64_COPY       5
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8
#define R_X86_64_GOTPCREL   9
#define R_X86_64_32         10
#define R_X86_64_32S        11
#define R_X86_64_16         12
#define R_X86_64_PC16       13
#define R_X86_64_8          14
#define R_X86_64_PC8        15
#define R_X86_64_PC64       24

typedef enum {
    MODULE_PARAM_INT   = 1,
    MODULE_PARAM_STR   = 2,
    MODULE_PARAM_BOOL  = 3,
} module_param_type_t;

typedef struct {
    char               name[MODULE_PARAM_LEN];
    module_param_type_t type;
    void*              value;
    char               default_val[MODULE_PARAM_LEN];
} module_param_t;

typedef struct module {
    char     name[MODULE_NAME_MAX];
    void*    text_section;
    uint64_t text_size;
    void*    data_section;
    uint64_t data_size;
    void*    bss_section;
    uint64_t bss_size;
    void*    rodata_section;
    uint64_t rodata_size;
    void*    symtab;
    uint64_t symtab_size;
    void*    strtab;
    uint64_t strtab_size;
    void*    core_text;
    uint64_t core_text_size;
    int      (*init)(void);
    void     (*exit)(void);
    uint32_t state;
    uint32_t ref_count;
    /* 依赖管理 */
    char     deps[MODULE_DEPS_MAX][MODULE_NAME_MAX];
    int      deps_count;
    /* 参数 */
    module_param_t params[MODULE_PARAM_MAX];
    int      params_count;
    struct module* next;
} module_t;

void module_init(void);

int module_load(const void* data, uint64_t size, const char* name);

int module_unload(const char* name);

module_t* module_find(const char* name);

int module_get(module_t* mod);
int module_put(module_t* mod);

module_t* module_get_first(void);
module_t* module_get_next(module_t* mod);

void module_export_symbol(const char* name, void* addr);
void* module_lookup_symbol(const char* name);

/* 扩展接口 */
int  module_register_param(module_t* mod, const char* name,
                            module_param_type_t type,
                            void* value, const char* default_val);
int  module_set_param(module_t* mod, const char* name, const char* value);
int  module_add_dep(module_t* mod, const char* dep_name);
int  module_resolve_deps(module_t* mod);
int  module_info_count(void);
int  module_get_info(int idx, module_t* out);

#endif
