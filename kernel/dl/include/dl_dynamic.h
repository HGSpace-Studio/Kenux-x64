#ifndef KERNEL_DL_DYNAMIC_H
#define KERNEL_DL_DYNAMIC_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define ELF_MAGIC       0x464C457F
#define ELF_CLASS_64    2
#define ELF_DATA_LSB    1
#define ELF_TYPE_EXEC   2
#define ELF_TYPE_DYN    3
#define ELF_MACHINE_X86_64 62

#define ELF_PT_NULL     0
#define ELF_PT_LOAD     1
#define ELF_PT_DYNAMIC  2
#define ELF_PT_INTERP   3
#define ELF_PT_NOTE     4
#define ELF_PT_SHLIB    5
#define ELF_PT_PHDR     6
#define ELF_PT_TLS      7

#define ELF_DT_NULL     0
#define ELF_DT_NEEDED   1
#define ELF_DT_PLTRELSZ 2
#define ELF_DT_PLTGOT   3
#define ELF_DT_HASH     4
#define ELF_DT_STRTAB   5
#define ELF_DT_SYMTAB   6
#define ELF_DT_RELA     7
#define ELF_DT_RELASZ   8
#define ELF_DT_RELAENT  9
#define ELF_DT_STRSZ    10
#define ELF_DT_SYMENT   11
#define ELF_DT_INIT     12
#define ELF_DT_FINI     13
#define ELF_DT_SONAME   14
#define ELF_DT_RPATH    15
#define ELF_DT_SYMBOLIC 16
#define ELF_DT_REL      17
#define ELF_DT_RELSZ    18
#define ELF_DT_RELENT   19
#define ELF_DT_PLTREL   20
#define ELF_DT_DEBUG    21
#define ELF_DT_TEXTREL  22
#define ELF_DT_JMPREL   23
#define ELF_DT_BIND_NOW 24
#define ELF_DT_INIT_ARRAY 25
#define ELF_DT_FINI_ARRAY 26
#define ELF_DT_INIT_ARRAYSZ 27
#define ELF_DT_FINI_ARRAYSZ 28
#define ELF_DT_VERSYM   35
#define ELF_DT_VERNEED  36
#define ELF_DT_VERNEEDNUM 37

#define ELF_R_X86_64_NONE     0
#define ELF_R_X86_64_64       1
#define ELF_R_X86_64_PC32     2
#define ELF_R_X86_64_GOT32    3
#define ELF_R_X86_64_PLT32    4
#define ELF_R_X86_64_COPY     5
#define ELF_R_X86_64_GLOB_DAT 6
#define ELF_R_X86_64_JMP_SLOT 7
#define ELF_R_X86_64_RELATIVE 8
#define ELF_R_X86_64_GOTPCREL 9
#define ELF_R_X86_64_32       10
#define ELF_R_X86_64_32S      11
#define ELF_R_X86_64_16       12
#define ELF_R_X86_64_PC16     13
#define ELF_R_X86_64_8        14
#define ELF_R_X86_64_PC8      15
#define ELF_R_X86_64_TLS_GD   18
#define ELF_R_X86_64_TLS_LD   19
#define ELF_R_X86_64_TLS_GOTCALL  33

#define ELF_STB_LOCAL   0
#define ELF_STB_GLOBAL  1
#define ELF_STB_WEAK    2

#define ELF_STT_NOTYPE  0
#define ELF_STT_OBJECT  1
#define ELF_STT_FUNC    2
#define ELF_STT_SECTION 3
#define ELF_STT_FILE    4
#define ELF_STT_COMMON  5
#define ELF_STT_TLS     6

#define DL_MAX_DEPS     32
#define DL_MAX_LOADED   256
#define DL_HASH_SIZE    1024

typedef struct {
    uint32_t name;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
} elf_sym_t;

typedef struct {
    uint64_t tag;
    uint64_t val;
} elf_dyn_t;

typedef struct {
    uint64_t offset;
    uint64_t info;
    int64_t  addend;
} elf_rela_t;

typedef struct {
    uint64_t offset;
    uint64_t info;
} elf_rel_t;

typedef struct dl_module dl_module_t;

struct dl_module {
    int          loaded;
    char         name[256];
    char         pathname[512];
    uint8_t*     base_addr;
    uint64_t     mem_size;
    elf_dyn_t*   dynamic;
    elf_sym_t*   symtab;
    const char*  strtab;
    uint32_t*    hash;
    void*        got;
    void*        plt;
    uint64_t     rela_offset;
    uint64_t     rela_size;
    uint64_t     rel_offset;
    uint64_t     rel_size;
    uint64_t     pltrel_offset;
    uint64_t     pltrel_size;
    int          pltrel_type;
    uint64_t     init_addr;
    uint64_t     fini_addr;
    uint64_t*    init_array;
    uint64_t     init_array_size;
    uint64_t*    fini_array;
    uint64_t     fini_array_size;
    int          ref_count;
    dl_module_t* deps[DL_MAX_DEPS];
    int          dep_count;
    dl_module_t* hash_next;
    spinlock_t   lock;
};

typedef struct {
    dl_module_t* modules[DL_MAX_LOADED];
    int          module_count;
    dl_module_t* hash_table[DL_HASH_SIZE];
    char**       search_paths;
    int          search_path_count;
    spinlock_t   lock;
} dl_context_t;

void     dl_init(dl_context_t* ctx);
void     dl_add_search_path(dl_context_t* ctx, const char* path);
dl_module_t* dl_load(dl_context_t* ctx, const char* name, int flags);
int      dl_unload(dl_context_t* ctx, dl_module_t* mod);
void*    dl_resolve(dl_context_t* ctx, dl_module_t* mod, const char* symbol);
int      dl_relocate(dl_context_t* ctx, dl_module_t* mod);
void     dl_call_init(dl_module_t* mod);
void     dl_call_fini(dl_module_t* mod);
elf_sym_t* dl_lookup_symbol(dl_context_t* ctx, const char* name, dl_module_t** out_mod);
void*    dl_dlopen(dl_context_t* ctx, const char* filename, int flags);
int      dl_dlclose(dl_context_t* ctx, void* handle);
void*    dl_dlsym(dl_context_t* ctx, void* handle, const char* symbol);
uint32_t dl_elf_hash(const char* name);

#define DL_RTLD_LAZY     1
#define DL_RTLD_NOW      2
#define DL_RTLD_GLOBAL   4
#define DL_RTLD_LOCAL    8
#define DL_RTLD_NODELETE 16

#endif