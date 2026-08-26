

#ifndef ARCH_LDSO_H
#define ARCH_LDSO_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define LDSO_PATH              "/lib/ld-kenux.so"
#define LDSO_MAX_LIBS          64
#define LDSO_MAX_SYMS          4096
#define LDSO_MAX_RELS          8192
#define LDSO_LIB_NAME_MAX      256

#define R_KENUX_64             1
#define R_KENUX_PC32           2
#define R_KENUX_GLOB_DAT       3
#define R_KENUX_JUMP_SLOT      4
#define R_KENUX_RELATIVE       5

#define DT_NULL                0
#define DT_NEEDED              1
#define DT_HASH                4
#define DT_STRTAB              5
#define DT_SYMTAB              6
#define DT_STRSZ               10
#define DT_SYMENT              11
#define DT_INIT                12
#define DT_FINI                13
#define DT_PLTREL              20
#define DT_JMPREL              23
#define DT_BIND_NOW            24
#define DT_INIT_ARRAY          25
#define DT_FINI_ARRAY          26
#define DT_INIT_ARRAYSZ        27
#define DT_FINI_ARRAYSZ        28
#define DT_REL                 17
#define DT_RELSZ               18
#define DT_RELENT              19
#define DT_PLTGOT              3
#define DT_PLTRELSZ            2
#define DT_FLAGS               30
#define DT_FLAGS_1             0x6ffffffb

#define DF_BIND_NOW            0x8

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} kelf_sym_t;

#define KELF_STB_GLOBAL        1
#define KELF_STB_WEAK          2

#define KELF_STT_NOTYPE        0
#define KELF_STT_FUNC          2
#define KELF_STT_OBJECT        1

#define KELF_ST_BIND(info)     ((info) >> 4)
#define KELF_ST_TYPE(info)     ((info) & 0xf)

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} kelf_rela_t;

#define KELF_R_SYM(info)       ((uint32_t)((info) >> 32))
#define KELF_R_TYPE(info)      ((uint32_t)((info) & 0xffffffffUL))
#define KELF_R_INFO(sym, type) (((uint64_t)(sym) << 32) | ((uint64_t)(type)))

typedef struct {
    uint32_t nbucket;
    uint32_t nchain;

} kelf_hash_t;

typedef struct {
    int64_t  d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} kelf_dyn_t;

typedef struct klib {
    uint64_t     base_addr;
    uint64_t     load_size;
    char         name[LDSO_LIB_NAME_MAX];
    int          fd;

    kelf_sym_t*  symtab;
    uint32_t     sym_count;
    const char*  strtab;
    uint64_t     strtab_size;
    kelf_hash_t* hash;
    uint32_t     hash_nbucket;
    uint32_t     hash_nchain;

    kelf_rela_t* plt_rels;
    uint64_t     plt_relsz;
    kelf_rela_t* rels;
    uint64_t     relsz;

    void        (*init_func)(void);
    void        (*fini_func)(void);
    void**       init_array;
    uint32_t     init_array_sz;
    void**       fini_array;
    uint32_t     fini_array_sz;

    int          ref_count;
    int          resolved;
    int          initialized;

    uint64_t     map_start;
    uint64_t     map_end;
} klib_t;

typedef struct {
    const char*  name;
    uint64_t     addr;
    klib_t*      owner;
    uint32_t     hits;
} sym_cache_entry_t;

#define LDSO_SYM_CACHE_SIZE      256

typedef struct {
    klib_t              libs[LDSO_MAX_LIBS];
    sym_cache_entry_t   sym_cache[LDSO_SYM_CACHE_SIZE];
    spinlock_t          lock;
    uint32_t            lib_count;
    uint64_t            total_relocs;
    uint64_t            total_lookups;
    uint64_t            cache_hits;
} ldso_state_t;

void ldso_init_state(ldso_state_t* state);

ldso_state_t* ldso_get_state(void);

klib_t* ldso_load_library(const char* path, ldso_state_t* state);

uint64_t ldso_lookup_symbol(const char* name, ldso_state_t* state);

int ldso_relocate(klib_t* lib, ldso_state_t* state);

void ldso_call_init(ldso_state_t* state);

void ldso_call_fini(ldso_state_t* state);

int ldso_unload_library(klib_t* lib, ldso_state_t* state);

#endif