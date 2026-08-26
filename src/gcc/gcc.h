/*
 * Kenux OS - GCC Compiler Driver (Minimal)
 * Header file for gcc functionality
 */

#ifndef _GCC_H
#define _GCC_H

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
#define WIFSTOPPED(s)  (0)
#define WSTOPSIG(s)    (0)

static inline int kenux_fork(void) { return -1; }
static inline int kenux_wait(int *s) { if (s) *s = 0; return -1; }
static inline int kenux_waitpid(int p, int *s, int o) { (void)p; (void)o; if (s) *s = 0; return -1; }
static inline int kenux_execvp(const char *f, char *const a[]) { (void)f; (void)a; return -1; }

#define fork    kenux_fork
#define wait    kenux_wait
#define waitpid kenux_waitpid
#define execvp  kenux_execvp

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static inline char *kenux_libname(const char *path) {
    static char buf[PATH_MAX];
    const char *p = strrchr(path, '/');
    const char *b = p ? p + 1 : path;
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

#define MAX_PATH_LEN 1024
#define MAX_INPUT_FILES 64
#define MAX_FLAGS 128
#define MAX_DEFINE 64
#define MAX_INCLUDE_DIRS 32
#define MAX_LIBRARY_DIRS 32
#define MAX_LIBRARIES 32
#define MAX_LINKER_FLAGS 32

/* Optimization levels */
typedef enum {
    OPT_NONE = 0,      /* -O0 */
    OPT_1,             /* -O1 */
    OPT_2,             /* -O2 (default) */
    OPT_3,             /* -O3 */
    OPT_S,             /* -Os  optimize for size */
    OPT_FAST,          /* -Ofast */
    OPT_DEBUG,         /* -Og */
} OptLevel;

/* C standard versions */
typedef enum {
    STD_DEFAULT = 0,
    STD_C89,           /* -std=c89 */
    STD_C90,           /* -std=c90 */
    STD_C94,           /* -std=c94 */
    STD_C99,           /* -std=c99 */
    STD_C11,           /* -std=c11 */
    STD_C17,           /* -std=c17 */
    STD_C2X,           /* -std=c2x */
    STD_CXX98,
    STD_CXX03,
    STD_CXX11,
    STD_CXX14,
    STD_CXX17,
    STD_CXX20,
    STD_GNU89,
    STD_GNU99,
    STD_GNU11,
    STD_GNU17,
} StdVersion;

/* Language modes */
typedef enum {
    LANG_AUTO = 0,
    LANG_C,            /* -x c */
    LANG_CXX,          /* -x c++ */
    LANG_ASSEMBLER,    /* -x assembler */
    LANG_PREPROCESSOR, /* -x c-cpp-output */
    LANG_OBJECTIVE_C,
    LANG_OBJECTIVE_CXX,
} LanguageMode;

/* Target architecture */
typedef enum {
    ARCH_NATIVE = 0,
    ARCH_X86,          /* -m32 */
    ARCH_X86_64,       /* -m64 */
    ARCH_ARM,
    ARCH_AARCH64,
    ARCH_RISCV32,
    ARCH_RISCV64,
    ARCH_MIPS,
    ARCH_PPC,
    ARCH_PPC64,
} ArchTarget;

/* Output file kind */
typedef enum {
    OUTPUT_DEFAULT = 0,  /* .c -> .s -> .o -> exe */
    OUTPUT_PREPROCESS,   /* -E  only preprocess */
    OUTPUT_COMPILE,      /* -S  compile to assembly */
    OUTPUT_ASSEMBLE,     /* -c  compile to object, don't link */
    OUTPUT_LINK,         /* normal link to executable */
} OutputKind;

/* Floating point ABI */
typedef enum {
    FP_DEFAULT = 0,
    FP_SOFT,           /* -msoft-float */
    FP_HARD,           /* -mhard-float */
    FP_SSE,            /* -msse / -mfpmath=sse */
    FP_387,            /* -mfpmath=387 */
} FloatABI;

/* Debug info level */
typedef enum {
    DEBUG_NONE = 0,
    DEBUG_MINIMAL,     /* -g1 */
    DEBUG_DEFAULT,     /* -g  */
    DEBUG_EXTENDED,    /* -g3 */
    DEBUG_GDB,         /* -ggdb */
} DebugLevel;

/* PIE / PIC mode */
typedef enum {
    PIE_DEFAULT = 0,
    PIC_NON_SHARED,    /* -fno-pic */
    PIC_SHARED,        /* -fpic / -fPIC */
    PIE_EXECUTABLE,    /* -fpie / -fPIE */
    PIE_NO_EXECUTABLE, /* -fno-pie */
} PieMode;

/* Sanitizer types */
typedef enum {
    SAN_NONE = 0,
    SAN_ADDRESS = 1 << 0,    /* -fsanitize=address */
    SAN_UNDEFINED = 1 << 1,  /* -fsanitize=undefined */
    SAN_THREAD = 1 << 2,     /* -fsanitize=thread */
    SAN_MEMORY = 1 << 3,     /* -fsanitize=memory */
    SAN_LEAK = 1 << 4,       /* -fsanitize=leak */
} SanitizerFlags;

/* LTO mode */
typedef enum {
    LTO_NONE = 0,
    LTO_ENABLE,        /* -flto */
    LTO_THIN,          /* -flto=thin */
    LTO_FAT,           /* -flto=fat */
} LtoMode;

/* Color diagnostics */
typedef enum {
    DIAG_COLOR_AUTO = 0,
    DIAG_COLOR_NEVER,
    DIAG_COLOR_ALWAYS,
} DiagColor;

/* Warning group */
typedef struct {
    char name[64];
    int enabled;
    int is_error;      /* -Werror=xxx */
} WarningSetting;

/* Compiler driver state */
typedef struct {
    /* Input/output */
    char input_files[MAX_INPUT_FILES][MAX_PATH_LEN];
    int input_count;
    char output_file[MAX_PATH_LEN];

    /* Language and standards */
    LanguageMode language;
    StdVersion std_version;
    int gnu_extensions;

    /* Target */
    ArchTarget arch;
    FloatABI float_abi;
    char march[64];   /* -march= */
    char mtune[64];   /* -mtune= */
    int m32;          /* -m32 flag */
    int m64;          /* -m64 flag */

    /* Optimization */
    OptLevel opt_level;
    int fomit_frame_pointer;
    int funroll_loops;
    int finline_functions;
    int ftree_vectorize;
    int fstrict_aliasing;
    int fno_strict_aliasing;
    int ffast_math;
    int ffine_math;

    /* Debug */
    DebugLevel debug_level;
    int fno_omit_frame_pointer;
    int p;            /* -pg profiling */

    /* Code generation */
    PieMode pie_mode;
    int fpic;
    int fPIC;
    int fpie;
    int fPIE;
    int shared;       /* -shared */
    int static_link;  /* -static */
    int rdynamic;     /* -rdynamic */
    int nostdlib;     /* -nostdlib */
    int nostartfiles; /* -nostartfiles */
    int nodefaultlibs;/* -nodefaultlibs */
    int freestanding; /* -ffreestanding */
    int no_pie;       /* -no-pie */

    /* Preprocessor */
    char defines[MAX_DEFINE][MAX_PATH_LEN];
    int define_count;
    char undefines[MAX_DEFINE][MAX_PATH_LEN];
    int undefine_count;
    char include_dirs[MAX_INCLUDE_DIRS][MAX_PATH_LEN];
    int include_dir_count;
    char sysroot[MAX_PATH_LEN]; /* --sysroot= */
    int nostdinc;              /* -nostdinc */
    int trigraphs;             /* -trigraphs */
    int no_inline;             /* -fno-inline */

    /* Assembler */
    int save_temps;            /* -save-temps */
    char assembler_flags[MAX_FLAGS][MAX_PATH_LEN];
    int assembler_flag_count;

    /* Linker */
    char library_dirs[MAX_LIBRARY_DIRS][MAX_PATH_LEN];
    int library_dir_count;
    char libraries[MAX_LIBRARIES][MAX_PATH_LEN];
    int library_count;
    char linker_scripts[MAX_LIBRARY_DIRS][MAX_PATH_LEN];
    int linker_script_count;
    char linker_flags[MAX_LINKER_FLAGS][MAX_PATH_LEN];
    int linker_flag_count;
    char entry_point[MAX_PATH_LEN]; /* -e symbol */
    int build_shared;           /* -shared */
    int strip;                  /* -s strip symbols */
    int export_dynamic;         /* -Wl,-E */
    int gc_sections;            /* -Wl,--gc-sections */
    int print_gc_sections;
    char rpath[MAX_LINKER_FLAGS][MAX_PATH_LEN];
    int rpath_count;

    /* Sanitizers & analysis */
    SanitizerFlags sanitizers;
    int fstack_protector;       /* -fstack-protector */
    int fstack_protector_strong;
    int fstack_clash_protection;
    int fcf_protection;
    LtoMode lto_mode;

    /* Warnings */
    WarningSetting warnings[MAX_FLAGS];
    int warning_count;
    int wall;                   /* -Wall */
    int wextra;                 /* -Wextra */
    int werror;                 /* -Werror */
    int wpedantic;              /* -Wpedantic */
    int wshadow;
    int wconversion;
    int wsign_conversion;
    int wnull_dereference;
    int wformat_security;
    int wunused;
    int wimplicit;
    int wno_all;

    /* Diagnostics */
    DiagColor diag_color;
    int fshow_column;
    int fmessage_length;
    char fdiagnostics_format[64];

    /* Misc */
    int verbose;                /* -v */
    int quiet;
    int pipe;                   /* -pipe */
    int print_file_name;
    int print_search_dirs;      /* -print-search-dirs */
    int print_libgcc_file_name;
    int print_prog_name;
    int dumpversion;            /* --version */
    int dumpmachine;            /* -dumpmachine */
    int dumpspecs;              /* -dumpspecs */
    int help;                   /* --help */
    int target_help;            /* --target-help */

    /* Internal driver state */
    OutputKind output_kind;
    char temp_dir[MAX_PATH_LEN];
    char cc1_path[MAX_PATH_LEN];
    char as_path[MAX_PATH_LEN];
    char ld_path[MAX_PATH_LEN];
    char cpp_path[MAX_PATH_LEN];
    char collect2_path[MAX_PATH_LEN];
    int pid;
    int exit_code;
    int error_count;

    /* Timing / memory stats */
    int time_report;            /* -ftime-report */
    int mem_report;

    /* Profile */
    int fprofile_generate;
    int fprofile_use;
    int fauto_profile;

} GccState;

/* Driver phases */
typedef enum {
    PHASE_PREPROCESS,  /* cpp */
    PHASE_COMPILE,     /* cc1  -> .s */
    PHASE_ASSEMBLE,    /* as   -> .o */
    PHASE_LINK,        /* ld   -> exe */
} CompilerPhase;

/* Per-file compile job */
typedef struct {
    char input[MAX_PATH_LEN];
    char output[MAX_PATH_LEN];
    char depfile[MAX_PATH_LEN];
    CompilerPhase phase;
    LanguageMode lang;
} CompileJob;

/* Function prototypes */
void gcc_init(GccState *state);
int gcc_parse_arguments(GccState *state, int argc, char **argv);
int gcc_validate_options(GccState *state);
int gcc_infer_language(GccState *state, const char *filename, LanguageMode *out_lang);
int gcc_infer_output_kind(OutputKind kind, const char *input, LanguageMode lang, char *out_path);
int gcc_determine_output_file(GccState *state, const char *input, CompilerPhase phase, char *out_path);
int gcc_build_jobs(GccState *state, CompileJob *jobs, int *out_job_count);
int gcc_resolve_tool_paths(GccState *state);
const char *gcc_std_option(StdVersion std);
const char *gcc_arch_option(ArchTarget arch);
const char *gcc_opt_option(OptLevel opt);
const char *gcc_debug_option(DebugLevel dbg);
int gcc_build_cc1_args(const GccState *state, const CompileJob *job, char ***out_argv);
int gcc_build_as_args(const GccState *state, const CompileJob *job, char ***out_argv);
int gcc_build_ld_args(const GccState *state, const CompileJob *jobs, int njobs, char ***out_argv);
int gcc_run_command(const char *argv[]);
int gcc_execute_jobs(GccState *state, const CompileJob *jobs, int njobs);
int gcc_do_link(GccState *state, const CompileJob *obj_jobs, int njobs);
void gcc_report_version(void);
void gcc_report_machine(void);
void gcc_report_search_dirs(const GccState *state);
void gcc_print_usage(void);
void gcc_print_target_help(void);
const char *language_name(LanguageMode lang);
LanguageMode language_from_name(const char *name);
const char *input_extension_for_language(LanguageMode lang);
int file_extension_matches(const char *filename, const char *ext);

#endif /* _GCC_H */
