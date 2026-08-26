/*
 * Kenux OS - Clang/LLVM Compiler Driver (Minimal)
 * Header file for clang functionality
 */

#ifndef _CLANG_H
#define _CLANG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#else
/* Windows MinGW compatibility shims */
#include <process.h>
#include <io.h>
#include <direct.h>

#define WIFEXITED(s)   (1)
#define WEXITSTATUS(s) (0)
#define WIFSIGNALED(s) (0)
#define WTERMSIG(s)    (0)
#define WIFSTOPPED(s)   (0)
#define WSTOPSIG(s)    (0)

static inline int kenux_fork(void) { return -1; }
static inline int kenux_wait(int *s) { if (s) *s = 0; return -1; }
static inline int kenux_execvp(const char *f, char *const a[]) { (void)f; (void)a; return -1; }

#define fork    kenux_fork
#define wait    kenux_wait
#define execvp  kenux_execvp

static inline char *kenux_libdir_dummy(const char *p) { (void)p; return NULL; }

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static inline char *kenux_libname(const char *path) {
    static char buf[PATH_MAX];
    const char *p = strrchr(path, '/');
    const char *b = p ? p + 1 : path;
    /* strip extension */
    size_t n = strlen(b);
    const char *dot = strrchr(b, '.');
    if (dot) n = (size_t)(dot - b);
    if (n >= PATH_MAX) n = PATH_MAX - 1;
    memcpy(buf, b, n);
    buf[n] = 0;
    return buf;
}
#define basename kenux_libname
#endif

#define CLANG_MAX_PATH_LEN 1024
#define CLANG_MAX_INPUT_FILES 64
#define CLANG_MAX_FLAGS 128
#define CLANG_MAX_DEFINE 64
#define CLANG_MAX_INCLUDE_DIRS 32
#define CLANG_MAX_LIBRARY_DIRS 32
#define CLANG_MAX_LIBRARIES 32
#define CLANG_MAX_LINKER_FLAGS 32

/* Clang optimization levels */
typedef enum {
    CLANG_OPT_NONE = 0,   /* -O0 */
    CLANG_OPT_1,          /* -O1 */
    CLANG_OPT_2,          /* -O2 default */
    CLANG_OPT_3,          /* -O3 */
    CLANG_OPT_S,          /* -Os size */
    CLANG_OPT_Z,          /* -Oz min size (clang extension) */
    CLANG_OPT_FAST,       /* -Ofast */
    CLANG_OPT_DEBUG,      /* -Og */
} ClangOptLevel;

/* C/C++ standard versions */
typedef enum {
    CLANG_STD_DEFAULT = 0,
    CLANG_STD_C89,
    CLANG_STD_C90,
    CLANG_STD_C94,
    CLANG_STD_C99,
    CLANG_STD_C11,
    CLANG_STD_C17,
    CLANG_STD_C23,
    CLANG_STD_GNU89,
    CLANG_STD_GNU99,
    CLANG_STD_GNU11,
    CLANG_STD_GNU17,
    CLANG_STD_GNU23,
    CLANG_STD_CXX98,
    CLANG_STD_CXX03,
    CLANG_STD_CXX11,
    CLANG_STD_CXX14,
    CLANG_STD_CXX17,
    CLANG_STD_CXX20,
    CLANG_STD_CXX23,
    CLANG_STD_GNUXX11,
    CLANG_STD_GNUXX14,
    CLANG_STD_GNUXX17,
    CLANG_STD_GNUXX20,
} ClangStdVersion;

/* Language modes */
typedef enum {
    CLANG_LANG_AUTO = 0,
    CLANG_LANG_C,
    CLANG_LANG_CXX,
    CLANG_LANG_ASSEMBLER,
    CLANG_LANG_ASSEMBLER_WITH_CPP,
    CLANG_LANG_OBJC,
    CLANG_LANG_OBJCXX,
    CLANG_LANG_OPENCL,
    CLANG_LANG_CUDA,
    CLANG_LANG_HIP,
    CLANG_LANG_RENDERSCRIPT,
    CLANG_LANG_AST,
    CLANG_LANG_LLVM_IR,
    CLANG_LANG_LLVM_BC,
} ClangLanguageMode;

/* Sanitizers */
typedef enum {
    CLANG_SAN_NONE = 0,
    CLANG_SAN_ADDRESS       = 1 << 0,
    CLANG_SAN_HWADDRESS     = 1 << 1,
    CLANG_SAN_KASAN         = 1 << 2,
    CLANG_SAN_UNDEFINED     = 1 << 3,
    CLANG_SAN_SAFESTACK     = 1 << 4,
    CLANG_SAN_CFI           = 1 << 5,
    CLANG_SAN_THREAD        = 1 << 6,
    CLANG_SAN_KTSAN         = 1 << 7,
    CLANG_SAN_MEMORY        = 1 << 8,
    CLANG_SAN_KMSAN         = 1 << 9,
    CLANG_SAN_DATAFLOW      = 1 << 10,
    CLANG_SAN_LEAK          = 1 << 11,
    CLANG_SAN_SCUDO         = 1 << 12,
    CLANG_SAN_BOUND         = 1 << 13,
} ClangSanitizerFlags;

/* Output kind */
typedef enum {
    CLANG_OUTPUT_DEFAULT = 0,
    CLANG_OUTPUT_PREPROCESS,    /* -E */
    CLANG_OUTPUT_COMPILE,       /* -S */
    CLANG_OUTPUT_ASSEMBLE,      /* -c */
    CLANG_OUTPUT_LINK,          /* default link */
    CLANG_OUTPUT_EMIT_LLVM,     /* -emit-llvm  -> .ll / .bc */
    CLANG_OUTPUT_AST,           /* -ast-print / -ast-dump */
    CLANG_OUTPUT_FS_ONLY,       /* -fsyntax-only */
} ClangOutputKind;

/* LTO modes */
typedef enum {
    CLANG_LTO_NONE = 0,
    CLANG_LTO_FULL,            /* -flto=full  or -flto */
    CLANG_LTO_THIN,            /* -flto=thin */
} ClangLtoMode;

/* Target triple components */
typedef struct {
    char arch[64];    /* x86_64, aarch64, riscv64, ... */
    char vendor[64];  /* pc, unknown, w64, apple, ... */
    char os[64];      /* linux, windows, darwin, kenux, ... */
    char env[64];     /* gnu, musl, msvc, android, ... */
} ClangTargetTriple;

/* Debug level */
typedef enum {
    CLANG_DEBUG_NONE = 0,
    CLANG_DEBUG_LINE,         /* -gline-tables-only */
    CLANG_DEBUG_MINIMAL,      /* -g1 */
    CLANG_DEBUG_DEFAULT,      /* -g2 or -g */
    CLANG_DEBUG_EXTENDED,     /* -g3 */
    CLANG_DEBUG_GDB,          /* -ggdb */
    CLANG_DEBUG_DWARF2,       /* -gdwarf-2 */
    CLANG_DEBUG_DWARF3,
    CLANG_DEBUG_DWARF4,
    CLANG_DEBUG_DWARF5,
} ClangDebugLevel;

/* Code model */
typedef enum {
    CLANG_CM_DEFAULT = 0,
    CLANG_CM_SMALL,           /* -mcmodel=small */
    CLANG_CM_KERNEL,          /* -mcmodel=kernel */
    CLANG_CM_MEDIUM,          /* -mcmodel=medium */
    CLANG_CM_LARGE,           /* -mcmodel=large */
} ClangCodeModel;

/* Relocation model */
typedef enum {
    CLANG_RELOC_DEFAULT = 0,
    CLANG_RELOC_STATIC,       /* -mcmodel + -static */
    CLANG_RELOC_PIC,          /* -fpic */
    CLANG_RELOC_PIE,          /* -fpie */
    CLANG_RELOC_DYNAMIC_NO_PIC, /* -fno-pic for -shared... */
} ClangRelocModel;

/* FPU mode */
typedef enum {
    CLANG_FPU_DEFAULT = 0,
    CLANG_FPU_SSE,
    CLANG_FPU_SSE2,
    CLANG_FPU_AVX,
    CLANG_FPU_AVX2,
    CLANG_FPU_AVX512F,
    CLANG_FPU_SOFT,
    CLANG_FPU_NEON,
    CLANG_FPU_NONE,
} ClangFPUMode;

/* Clang driver state */
typedef struct {
    /* I/O */
    char input_files[CLANG_MAX_INPUT_FILES][CLANG_MAX_PATH_LEN];
    int input_count;
    char output_file[CLANG_MAX_PATH_LEN];

    /* Language / standard */
    ClangLanguageMode language;
    ClangStdVersion std_version;

    /* Target */
    char target_triple[CLANG_MAX_PATH_LEN]; /* -target triple */
    ClangTargetTriple parsed_target;
    char march[64];
    char mtune[64];
    char mcpu[64];            /* -mcpu= */
    ClangCodeModel code_model;
    int m32;
    int m64;
    int mthumb;               /* ARM */
    int marm;
    ClangFPUMode fpu_mode;
    int mno_sse;
    int mno_avx;
    int msoft_float;

    /* Optimization */
    ClangOptLevel opt_level;
    int fomit_frame_pointer;
    int fno_frame_pointer;
    int funroll_loops;
    int finline;
    int fvectorize;
    int fslp_vectorize;
    int fstrict_aliasing;
    int fno_strict_aliasing;
    int ffast_math;
    int fffp_contract;
    int fno_math_errno;
    int finstrument_functions;

    /* Debug */
    ClangDebugLevel debug_level;
    int gline_tables_only;
    int fdebug_info_for_profiling;
    int p;                   /* -pg */

    /* Output */
    ClangOutputKind output_kind;
    int save_temps;
    int integrate_as;        /* -fintegrated-as (default) */
    int no_integrate_as;
    int integrate_cc1as;
    int time_passes;         /* -ftime-report */

    /* LTO */
    ClangLtoMode lto_mode;
    int lto_jobs;            /* -flto-jobs=N */
    int thinlto_cache_dir[CLANG_MAX_PATH_LEN];

    /* Preprocessor */
    char defines[CLANG_MAX_DEFINE][CLANG_MAX_PATH_LEN];
    int define_count;
    char undefines[CLANG_MAX_DEFINE][CLANG_MAX_PATH_LEN];
    int undefine_count;
    char include_dirs[CLANG_MAX_INCLUDE_DIRS][CLANG_MAX_PATH_LEN];
    int include_dir_count;
    char sysroot[CLANG_MAX_PATH_LEN];
    int nostdinc;
    int nostdincpp;
    char idirafter_dirs[CLANG_MAX_INCLUDE_DIRS][CLANG_MAX_PATH_LEN];
    int idirafter_count;
    char iprefix[CLANG_MAX_PATH_LEN];

    /* Code gen / linking */
    ClangRelocModel reloc_model;
    int fpic;
    int fPIC;
    int fpie;
    int fPIE;
    int fno_pic;
    int fno_pie;
    int shared;
    int static_link;
    int nostdlib;
    int nostartfiles;
    int nodefaultlibs;
    int freestanding;
    int rdynamic;
    int no_pie;
    int no_stdlib_inc;

    /* Sanitizers */
    ClangSanitizerFlags sanitizers;
    int fstack_protector;
    int fstack_protector_strong;
    int fstack_protector_all;
    int fstack_clash_protection;
    int fcf_protection;
    int fbti;                 /* aarch64 */
    int fsafe_stack;

    /* Warnings */
    int weverything;          /* clang -Weverything */
    int wall;
    int wextra;
    int werror;
    int wpedantic;
    int wno_error;
    char w_flags[CLANG_MAX_FLAGS][128];
    int w_flag_count;

    /* Linker */
    char library_dirs[CLANG_MAX_LIBRARY_DIRS][CLANG_MAX_PATH_LEN];
    int library_dir_count;
    char libraries[CLANG_MAX_LIBRARIES][CLANG_MAX_PATH_LEN];
    int library_count;
    char linker_flags[CLANG_MAX_LINKER_FLAGS][CLANG_MAX_PATH_LEN];
    int linker_flag_count;
    char entry_point[CLANG_MAX_PATH_LEN];
    char linker_script[CLANG_MAX_PATH_LEN];
    int strip_all;
    int strip_debug;
    int gc_sections;
    int no_gc_sections;
    int build_id;              /* -Wl,--build-id */
    char rpath[CLANG_MAX_LINKER_FLAGS][CLANG_MAX_PATH_LEN];
    int rpath_count;
    char dynamic_linker[CLANG_MAX_PATH_LEN]; /* -Wl,-dynamic-linker=... */

    /* Clang-specific */
    int cc1_print_help;
    int cc1_print_help_hidden;
    int print_llvm_options;
    char plugin[32][CLANG_MAX_PATH_LEN];
    int plugin_count;
    int fms_compatibility;     /* -fms-compatibility */
    int fms_extensions;
    int fborland_extensions;
    int fblocks;               /* -fblocks */
    int fobjc_arc;             /* -fobjc-arc */
    int fopenmp;
    int fopenacc;
    int fcuda_device;
    int fhip_device;
    int fsycl;
    int ftrigraphs;
    int fdollars_in_identifiers;

    /* Diagnostics color */
    int fno_color_diagnostics;
    int fcolor_diagnostics;

    /* Misc */
    int verbose;
    int pipe;
    int version;
    int help;
    int help_hidden;
    int print_search_dirs;
    int print_prog_name;
    int print_file_name;
    int dumpversion;
    int dumpmachine;

    /* Internal state */
    char clang_cc1_path[CLANG_MAX_PATH_LEN];
    char clang_cc1as_path[CLANG_MAX_PATH_LEN];
    char as_path[CLANG_MAX_PATH_LEN];
    char ld_path[CLANG_MAX_PATH_LEN];
    char llvm_link_path[CLANG_MAX_PATH_LEN];
    char opt_path[CLANG_MAX_PATH_LEN];
    char llc_path[CLANG_MAX_PATH_LEN];
    int exit_code;
    int error_count;
    int warning_count;

} ClangState;

/* Per-file job */
typedef struct {
    char input[CLANG_MAX_PATH_LEN];
    char output[CLANG_MAX_PATH_LEN];
    char depfile[CLANG_MAX_PATH_LEN];
    ClangOutputKind phase;
    ClangLanguageMode lang;
} ClangJob;

/* Prototypes - core driver */
void clang_init(ClangState *state);
int clang_parse_arguments(ClangState *state, int argc, char **argv);
int clang_validate(ClangState *state);
int clang_parse_target_triple(const char *triple, ClangTargetTriple *out);
int clang_format_target_triple(const ClangTargetTriple *t, char *out, size_t size);
int clang_infer_language(const char *filename, ClangLanguageMode *out);
int clang_output_extension(ClangOutputKind kind, ClangLanguageMode lang, char *out, size_t size);
int clang_build_jobs(ClangState *state, ClangJob *jobs, int *out_n);
int clang_resolve_tools(ClangState *state);
const char *clang_std_flag(ClangStdVersion s);
const char *clang_opt_flag(ClangOptLevel o);
const char *clang_debug_flag(ClangDebugLevel d);
const char *clang_lto_flag(ClangLtoMode l);
int clang_build_cc1_args(const ClangState *state, const ClangJob *job, char ***out_argv);
int clang_build_cc1as_args(const ClangState *state, const ClangJob *job, char ***out_argv);
int clang_build_external_as_args(const ClangState *state, const ClangJob *job, char ***out_argv);
int clang_build_ld_args(const ClangState *state, const ClangJob *jobs, int njobs, char ***out_argv);
int clang_run(const char *argv[]);
int clang_execute_jobs(ClangState *state, const ClangJob *jobs, int njobs);
int clang_do_link(ClangState *state, const ClangJob *jobs, int njobs);
void clang_show_version(void);
void clang_print_usage(void);
void clang_print_cc1_help(void);
void clang_print_search_dirs(const ClangState *state);
const char *clang_lang_name(ClangLanguageMode l);

#endif /* _CLANG_H */
