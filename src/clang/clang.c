/*
 * Kenux OS - Clang/LLVM Compiler Driver (Minimal)
 * Main clang driver implementation
 *
 * This is a minimal front-end driver that parses clang-style command-line
 * options, then invokes an external clang/cc1/ld toolchain (if available on
 * the host). It mirrors the option surface of a real clang driver so that
 * KenuxK build scripts can call "clang" the same way they call upstream clang.
 */

#include "clang.h"

#include <stdarg.h>

#define CLANG_DRIVER_VERSION "KenuxK-clang 18.1.0 (minimal driver)"
#define CLANG_DEFAULT_TARGET  "x86_64-unknown-kenuxk"

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static const char *clang_basename(const char *path) {
    const char *b = path;
    if (!path) return "clang";
    const char *s1 = strrchr(path, '/');
    const char *s2 = strrchr(path, '\\');
    if (s1 && s1 >= b) b = s1 + 1;
    if (s2 && s2 >= b) b = s2 + 1;
    return b;
}

static int clang_ext_is(const char *file, const char *ext) {
    const char *dot = strrchr(file, '.');
    return dot && strcmp(dot + 1, ext) == 0;
}

static int clang_run_argv(const char *const *argv) {
    /* Run an external command synchronously. */
    if (!argv || !argv[0]) return -1;
#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) { perror("clang"); return -1; }
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        perror(argv[0]);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
    /* Windows: use _spawnvp synchronously. */
    int rc = _spawnvp(_P_WAIT, argv[0], (const char *const *)argv);
    return rc;
#endif
}

/* ---------------------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------------------- */

void clang_init(ClangState *state) {
    if (!state) return;
    memset(state, 0, sizeof(ClangState));
    state->opt_level = CLANG_OPT_2;
    state->std_version = CLANG_STD_GNU17;
    state->language = CLANG_LANG_AUTO;
    state->output_kind = CLANG_OUTPUT_LINK;
    state->debug_level = CLANG_DEBUG_NONE;
    state->lto_mode = CLANG_LTO_NONE;
    state->reloc_model = CLANG_RELOC_DEFAULT;
    state->code_model = CLANG_CM_DEFAULT;
    state->fpu_mode = CLANG_FPU_DEFAULT;

    /* Default tool paths (POSIX layout; overridden on Windows if absent). */
    strncpy(state->clang_cc1_path, "/usr/bin/clang-18", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->clang_cc1as_path, "/usr/lib/clang/18.1.0/include", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->as_path, "/usr/bin/as", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->ld_path, "/usr/bin/ld", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->llvm_link_path, "/usr/bin/llvm-link", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->opt_path, "/usr/bin/opt", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->llc_path, "/usr/bin/llc", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->entry_point, "", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->linker_script, "", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->dynamic_linker, "", CLANG_MAX_PATH_LEN - 1);
    strncpy(state->target_triple, CLANG_DEFAULT_TARGET, CLANG_MAX_PATH_LEN - 1);

    /* Default include dirs. */
    if (state->include_dir_count < CLANG_MAX_INCLUDE_DIRS) {
        strncpy(state->include_dirs[state->include_dir_count++],
                "/usr/local/include", CLANG_MAX_PATH_LEN - 1);
    }
    if (state->include_dir_count < CLANG_MAX_INCLUDE_DIRS) {
        strncpy(state->include_dirs[state->include_dir_count++],
                "/usr/lib/clang/18.1.0/include", CLANG_MAX_PATH_LEN - 1);
    }
    if (state->include_dir_count < CLANG_MAX_INCLUDE_DIRS) {
        strncpy(state->include_dirs[state->include_dir_count++],
                "/usr/include", CLANG_MAX_PATH_LEN - 1);
    }
    /* Default library dirs. */
    if (state->library_dir_count < CLANG_MAX_LIBRARY_DIRS) {
        strncpy(state->library_dirs[state->library_dir_count++],
                "/usr/lib", CLANG_MAX_PATH_LEN - 1);
    }
    if (state->library_dir_count < CLANG_MAX_LIBRARY_DIRS) {
        strncpy(state->library_dirs[state->library_dir_count++],
                "/lib", CLANG_MAX_PATH_LEN - 1);
    }
    state->gc_sections = 1;
    state->build_id = 0;
    state->fstack_protector = 1;
    state->integrate_as = 1;
}

/* ---------------------------------------------------------------------------
 * Option -> flag conversion helpers
 * ------------------------------------------------------------------------- */

const char *clang_std_flag(ClangStdVersion s) {
    switch (s) {
        case CLANG_STD_C89:    return "-std=c89";
        case CLANG_STD_C90:    return "-std=c90";
        case CLANG_STD_C94:    return "-std=c94";
        case CLANG_STD_C99:    return "-std=c99";
        case CLANG_STD_C11:    return "-std=c11";
        case CLANG_STD_C17:    return "-std=c17";
        case CLANG_STD_C23:    return "-std=c23";
        case CLANG_STD_GNU89:  return "-std=gnu89";
        case CLANG_STD_GNU99:  return "-std=gnu99";
        case CLANG_STD_GNU11:  return "-std=gnu11";
        case CLANG_STD_GNU17:  return "-std=gnu17";
        case CLANG_STD_GNU23:  return "-std=gnu23";
        case CLANG_STD_CXX98:  return "-std=c++98";
        case CLANG_STD_CXX03:  return "-std=c++03";
        case CLANG_STD_CXX11:  return "-std=c++11";
        case CLANG_STD_CXX14:  return "-std=c++14";
        case CLANG_STD_CXX17:  return "-std=c++17";
        case CLANG_STD_CXX20:  return "-std=c++20";
        case CLANG_STD_CXX23:  return "-std=c++23";
        case CLANG_STD_GNUXX11: return "-std=gnu++11";
        case CLANG_STD_GNUXX14: return "-std=gnu++14";
        case CLANG_STD_GNUXX17: return "-std=gnu++17";
        case CLANG_STD_GNUXX20: return "-std=gnu++20";
        default: return NULL;
    }
}

const char *clang_opt_flag(ClangOptLevel o) {
    switch (o) {
        case CLANG_OPT_NONE:  return "-O0";
        case CLANG_OPT_1:     return "-O1";
        case CLANG_OPT_2:     return "-O2";
        case CLANG_OPT_3:     return "-O3";
        case CLANG_OPT_S:     return "-Os";
        case CLANG_OPT_Z:     return "-Oz";
        case CLANG_OPT_FAST:  return "-Ofast";
        case CLANG_OPT_DEBUG: return "-Og";
        default: return "-O2";
    }
}

const char *clang_debug_flag(ClangDebugLevel d) {
    switch (d) {
        case CLANG_DEBUG_NONE:     return NULL;
        case CLANG_DEBUG_LINE:     return "-gline-tables-only";
        case CLANG_DEBUG_MINIMAL:  return "-g1";
        case CLANG_DEBUG_DEFAULT:  return "-g";
        case CLANG_DEBUG_EXTENDED: return "-g3";
        case CLANG_DEBUG_GDB:      return "-ggdb";
        case CLANG_DEBUG_DWARF2:   return "-gdwarf-2";
        case CLANG_DEBUG_DWARF3:   return "-gdwarf-3";
        case CLANG_DEBUG_DWARF4:   return "-gdwarf-4";
        case CLANG_DEBUG_DWARF5:   return "-gdwarf-5";
        default: return "-g";
    }
}

const char *clang_lto_flag(ClangLtoMode l) {
    switch (l) {
        case CLANG_LTO_FULL: return "-flto=full";
        case CLANG_LTO_THIN: return "-flto=thin";
        default: return NULL;
    }
}

const char *clang_lang_name(ClangLanguageMode l) {
    switch (l) {
        case CLANG_LANG_C: return "c";
        case CLANG_LANG_CXX: return "c++";
        case CLANG_LANG_ASSEMBLER: return "assembler";
        case CLANG_LANG_ASSEMBLER_WITH_CPP: return "assembler-with-cpp";
        case CLANG_LANG_OBJC: return "objective-c";
        case CLANG_LANG_OBJCXX: return "objective-c++";
        case CLANG_LANG_OPENCL: return "opencl";
        case CLANG_LANG_CUDA: return "cuda";
        case CLANG_LANG_HIP: return "hip";
        case CLANG_LANG_RENDERSCRIPT: return "renderscript";
        case CLANG_LANG_AST: return "ast";
        case CLANG_LANG_LLVM_IR: return "llvm-ir";
        case CLANG_LANG_LLVM_BC: return "llvm-bc";
        default: return "auto";
    }
}

/* ---------------------------------------------------------------------------
 * Language inference / output extension
 * ------------------------------------------------------------------------- */

int clang_infer_language(const char *filename, ClangLanguageMode *out) {
    ClangLanguageMode m = CLANG_LANG_AUTO;
    if (!filename) { if (out) *out = m; return 0; }
    if      (clang_ext_is(filename, "c"))   m = CLANG_LANG_C;
    else if (clang_ext_is(filename, "h"))   m = CLANG_LANG_C;
    else if (clang_ext_is(filename, "i"))   m = CLANG_LANG_C;        /* preprocessed C */
    else if (clang_ext_is(filename, "cpp") || clang_ext_is(filename, "cxx") ||
             clang_ext_is(filename, "cc")  || clang_ext_is(filename, "C"))   m = CLANG_LANG_CXX;
    else if (clang_ext_is(filename, "ii"))  m = CLANG_LANG_CXX;
    else if (clang_ext_is(filename, "s"))   m = CLANG_LANG_ASSEMBLER;
    else if (clang_ext_is(filename, "S"))   m = CLANG_LANG_ASSEMBLER_WITH_CPP;
    else if (clang_ext_is(filename, "bc"))  m = CLANG_LANG_LLVM_BC;
    else if (clang_ext_is(filename, "ll"))  m = CLANG_LANG_LLVM_IR;
    else if (clang_ext_is(filename, "m"))   m = CLANG_LANG_OBJC;
    else if (clang_ext_is(filename, "mm"))  m = CLANG_LANG_OBJCXX;
    else if (clang_ext_is(filename, "cl"))  m = CLANG_LANG_OPENCL;
    else if (clang_ext_is(filename, "cu"))  m = CLANG_LANG_CUDA;
    else if (clang_ext_is(filename, "hip")) m = CLANG_LANG_HIP;
    if (out) *out = m;
    return 0;
}

int clang_output_extension(ClangOutputKind kind, ClangLanguageMode lang,
                           char *out, size_t size) {
    const char *ext = ".out";
    if (!out || size == 0) return -1;
    switch (kind) {
        case CLANG_OUTPUT_PREPROCESS:
            ext = (lang == CLANG_LANG_CXX) ? ".ii" : ".i";
            break;
        case CLANG_OUTPUT_COMPILE:
            ext = ".s";
            break;
        case CLANG_OUTPUT_ASSEMBLE:
            ext = ".o";
            break;
        case CLANG_OUTPUT_EMIT_LLVM:
            ext = ".bc";
            break;
        case CLANG_OUTPUT_AST:
            ext = ".ast";
            break;
        case CLANG_OUTPUT_FS_ONLY:
            ext = ".fs";
            break;
        default: /* LINK */
            ext = ".out";
            break;
    }
    return (snprintf(out, size, "%s", ext) < 0) ? -1 : 0;
}

/* ---------------------------------------------------------------------------
 * Target triple parsing/formatting
 * ------------------------------------------------------------------------- */

int clang_parse_target_triple(const char *triple, ClangTargetTriple *out) {
    if (!triple || !out) return -1;
    memset(out, 0, sizeof(*out));
    /* very small parser: arch[-vendor]-os[-env] */
    char buf[256];
    strncpy(buf, triple, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *save = NULL;
    char *tok = strtok_r(buf, "-", &save);
    if (tok) strncpy(out->arch, tok, sizeof(out->arch) - 1);
    tok = strtok_r(NULL, "-", &save);
    if (tok) strncpy(out->vendor, tok, sizeof(out->vendor) - 1);
    tok = strtok_r(NULL, "-", &save);
    if (tok) strncpy(out->os, tok, sizeof(out->os) - 1);
    tok = strtok_r(NULL, "-", &save);
    if (tok) strncpy(out->env, tok, sizeof(out->env) - 1);
    return 0;
}

int clang_format_target_triple(const ClangTargetTriple *t, char *out, size_t size) {
    if (!t || !out || size == 0) return -1;
    int n;
    if (t->env[0])
        n = snprintf(out, size, "%s-%s-%s-%s", t->arch, t->vendor, t->os, t->env);
    else
        n = snprintf(out, size, "%s-%s-%s", t->arch, t->vendor, t->os);
    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

int clang_validate(ClangState *state) {
    if (!state) return -1;
    if (state->output_kind == CLANG_OUTPUT_LINK && state->input_count == 0)
        return -1;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Argument parsing
 * ------------------------------------------------------------------------- */

static const char *clang_take_value(const char *a, int *i, int argc, char **argv) {
    /* "-X val" or "-Xval" */
    size_t len = strlen(a);
    if (len > 2) return a + 2;          /* -Xval form */
    if (++(*i) < argc) return argv[*i]; /* -X val form */
    return NULL;
}

int clang_parse_arguments(ClangState *state, int argc, char **argv) {
    if (!state || argc < 1 || !argv) return -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;
        if (a[0] != '-') {
            /* Input file (must look like it has a path/extension). */
            if (state->input_count < CLANG_MAX_INPUT_FILES) {
                strncpy(state->input_files[state->input_count++], a,
                        CLANG_MAX_PATH_LEN - 1);
            }
            continue;
        }
        /* Output stage */
        if (strcmp(a, "-E") == 0)               state->output_kind = CLANG_OUTPUT_PREPROCESS;
        else if (strcmp(a, "-S") == 0)          state->output_kind = CLANG_OUTPUT_COMPILE;
        else if (strcmp(a, "-c") == 0)         state->output_kind = CLANG_OUTPUT_ASSEMBLE;
        else if (strcmp(a, "-emit-llvm") == 0)  state->output_kind = CLANG_OUTPUT_EMIT_LLVM;
        else if (strcmp(a, "-fsyntax-only") == 0) state->output_kind = CLANG_OUTPUT_FS_ONLY;
        else if (strcmp(a, "-o") == 0) {
            const char *v = (++i < argc) ? argv[i] : NULL;
            if (v) strncpy(state->output_file, v, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-o", 2) == 0) {
            strncpy(state->output_file, a + 2, CLANG_MAX_PATH_LEN - 1);
        }
        /* Verbosity/help/version */
        else if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) state->verbose = 1;
        else if (strcmp(a, "--version") == 0) state->version = 1;
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) state->help = 1;
        else if (strcmp(a, "--help-hidden") == 0) state->help_hidden = 1;
        else if (strcmp(a, "-print-search-dirs") == 0) state->print_search_dirs = 1;
        else if (strcmp(a, "-print-prog-name") == 0) state->print_prog_name = 1;
        else if (strcmp(a, "-print-file-name") == 0) state->print_file_name = 1;
        else if (strcmp(a, "-dumpversion") == 0) state->dumpversion = 1;
        else if (strcmp(a, "-dumpmachine") == 0) state->dumpmachine = 1;
        /* Optimization */
        else if (strcmp(a, "-O0") == 0) state->opt_level = CLANG_OPT_NONE;
        else if (strcmp(a, "-O1") == 0) state->opt_level = CLANG_OPT_1;
        else if (strcmp(a, "-O2") == 0 || strcmp(a, "-O") == 0) state->opt_level = CLANG_OPT_2;
        else if (strcmp(a, "-O3") == 0) state->opt_level = CLANG_OPT_3;
        else if (strcmp(a, "-Os") == 0) state->opt_level = CLANG_OPT_S;
        else if (strcmp(a, "-Oz") == 0) state->opt_level = CLANG_OPT_Z;
        else if (strcmp(a, "-Ofast") == 0) state->opt_level = CLANG_OPT_FAST;
        else if (strcmp(a, "-Og") == 0) state->opt_level = CLANG_OPT_DEBUG;
        /* Debug info */
        else if (strcmp(a, "-g") == 0)             state->debug_level = CLANG_DEBUG_DEFAULT;
        else if (strcmp(a, "-g1") == 0)            state->debug_level = CLANG_DEBUG_MINIMAL;
        else if (strcmp(a, "-g3") == 0)           state->debug_level = CLANG_DEBUG_EXTENDED;
        else if (strcmp(a, "-ggdb") == 0)          state->debug_level = CLANG_DEBUG_GDB;
        else if (strcmp(a, "-gline-tables-only") == 0) state->debug_level = CLANG_DEBUG_LINE;
        else if (strcmp(a, "-gdwarf-2") == 0)      state->debug_level = CLANG_DEBUG_DWARF2;
        else if (strcmp(a, "-gdwarf-3") == 0)      state->debug_level = CLANG_DEBUG_DWARF3;
        else if (strcmp(a, "-gdwarf-4") == 0)      state->debug_level = CLANG_DEBUG_DWARF4;
        else if (strcmp(a, "-gdwarf-5") == 0)      state->debug_level = CLANG_DEBUG_DWARF5;
        /* Standard */
        else if (strncmp(a, "-std=", 5) == 0) {
            const char *s = a + 5;
            if      (strcmp(s, "c89") == 0)  state->std_version = CLANG_STD_C89;
            else if (strcmp(s, "c99") == 0)  state->std_version = CLANG_STD_C99;
            else if (strcmp(s, "c11") == 0)  state->std_version = CLANG_STD_C11;
            else if (strcmp(s, "c17") == 0 || strcmp(s, "c18") == 0) state->std_version = CLANG_STD_C17;
            else if (strcmp(s, "c23") == 0 || strcmp(s, "c2x") == 0) state->std_version = CLANG_STD_C23;
            else if (strcmp(s, "gnu89") == 0) state->std_version = CLANG_STD_GNU89;
            else if (strcmp(s, "gnu99") == 0) state->std_version = CLANG_STD_GNU99;
            else if (strcmp(s, "gnu11") == 0) state->std_version = CLANG_STD_GNU11;
            else if (strcmp(s, "gnu17") == 0) state->std_version = CLANG_STD_GNU17;
            else if (strcmp(s, "c++98") == 0) state->std_version = CLANG_STD_CXX98;
            else if (strcmp(s, "c++11") == 0) state->std_version = CLANG_STD_CXX11;
            else if (strcmp(s, "c++14") == 0) state->std_version = CLANG_STD_CXX14;
            else if (strcmp(s, "c++17") == 0) state->std_version = CLANG_STD_CXX17;
            else if (strcmp(s, "c++20") == 0) state->std_version = CLANG_STD_CXX20;
            else if (strcmp(s, "c++23") == 0) state->std_version = CLANG_STD_CXX23;
        }
        else if (strncmp(a, "-x", 2) == 0) {
            const char *s = clang_take_value(a, &i, argc, argv);
            if (s) {
                if (strcmp(s, "c") == 0) state->language = CLANG_LANG_C;
                else if (strcmp(s, "c++") == 0) state->language = CLANG_LANG_CXX;
                else if (strcmp(s, "assembler") == 0) state->language = CLANG_LANG_ASSEMBLER;
                else if (strcmp(s, "assembler-with-cpp") == 0) state->language = CLANG_LANG_ASSEMBLER_WITH_CPP;
                else if (strcmp(s, "objective-c") == 0) state->language = CLANG_LANG_OBJC;
                else if (strcmp(s, "objective-c++") == 0) state->language = CLANG_LANG_OBJCXX;
                else if (strcmp(s, "cuda") == 0) state->language = CLANG_LANG_CUDA;
                else if (strcmp(s, "hip") == 0) state->language = CLANG_LANG_HIP;
                else if (strcmp(s, "opencl") == 0) state->language = CLANG_LANG_OPENCL;
            }
        }
        /* Target / arch */
        else if (strncmp(a, "-target=", 8) == 0 || strncmp(a, "--target=", 9) == 0) {
            const char *t = strchr(a, '=') + 1;
            strncpy(state->target_triple, t, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strcmp(a, "-m32") == 0) state->m32 = 1;
        else if (strcmp(a, "-m64") == 0) state->m64 = 1;
        else if (strncmp(a, "-march=", 7) == 0) strncpy(state->march, a + 7, sizeof(state->march) - 1);
        else if (strncmp(a, "-mtune=", 7) == 0) strncpy(state->mtune, a + 7, sizeof(state->mtune) - 1);
        else if (strncmp(a, "-mcpu=", 6) == 0) strncpy(state->mcpu, a + 6, sizeof(state->mcpu) - 1);
        else if (strncmp(a, "-mcmodel=", 9) == 0) {
            const char *s = a + 9;
            if      (strcmp(s, "small") == 0)  state->code_model = CLANG_CM_SMALL;
            else if (strcmp(s, "kernel") == 0) state->code_model = CLANG_CM_KERNEL;
            else if (strcmp(s, "medium") == 0) state->code_model = CLANG_CM_MEDIUM;
            else if (strcmp(s, "large") == 0)  state->code_model = CLANG_CM_LARGE;
        }
        /* Preprocessor */
        else if (strncmp(a, "-D", 2) == 0) {
            if (state->define_count < CLANG_MAX_DEFINE)
                strncpy(state->defines[state->define_count++], a + 2, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-U", 2) == 0) {
            if (state->undefine_count < CLANG_MAX_DEFINE)
                strncpy(state->undefines[state->undefine_count++], a + 2, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-I", 2) == 0) {
            const char *v = clang_take_value(a, &i, argc, argv);
            if (v && state->include_dir_count < CLANG_MAX_INCLUDE_DIRS)
                strncpy(state->include_dirs[state->include_dir_count++], v, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-isystem", 8) == 0) {
            const char *v = clang_take_value(a, &i, argc, argv);
            if (v && state->include_dir_count < CLANG_MAX_INCLUDE_DIRS)
                strncpy(state->include_dirs[state->include_dir_count++], v, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "--sysroot=", 10) == 0)
            strncpy(state->sysroot, a + 10, CLANG_MAX_PATH_LEN - 1);
        else if (strcmp(a, "-nostdinc") == 0) state->nostdinc = 1;
        else if (strcmp(a, "-nostdinc++") == 0) state->nostdincpp = 1;
        /* Linker */
        else if (strncmp(a, "-L", 2) == 0) {
            const char *v = clang_take_value(a, &i, argc, argv);
            if (v && state->library_dir_count < CLANG_MAX_LIBRARY_DIRS)
                strncpy(state->library_dirs[state->library_dir_count++], v, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-l", 2) == 0) {
            if (state->library_count < CLANG_MAX_LIBRARIES)
                strncpy(state->libraries[state->library_count++], a + 2, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strncmp(a, "-Wl,", 4) == 0) {
            if (state->linker_flag_count < CLANG_MAX_LINKER_FLAGS)
                strncpy(state->linker_flags[state->linker_flag_count++], a + 4, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strcmp(a, "-T") == 0 || strncmp(a, "-T", 2) == 0) {
            const char *v = clang_take_value(a, &i, argc, argv);
            if (v) strncpy(state->linker_script, v, CLANG_MAX_PATH_LEN - 1);
        }
        else if (strcmp(a, "-static") == 0) { state->static_link = 1; state->reloc_model = CLANG_RELOC_STATIC; }
        else if (strcmp(a, "-shared") == 0)  { state->shared = 1; state->reloc_model = CLANG_RELOC_DYNAMIC_NO_PIC; }
        else if (strcmp(a, "-nostdlib") == 0) state->nostdlib = 1;
        else if (strcmp(a, "-nostartfiles") == 0) state->nostartfiles = 1;
        else if (strcmp(a, "-nodefaultlibs") == 0) state->nodefaultlibs = 1;
        else if (strcmp(a, "-rdynamic") == 0) state->rdynamic = 1;
        else if (strcmp(a, "-ffreestanding") == 0) state->freestanding = 1;
        else if (strcmp(a, "-fpic") == 0)  { state->fpic = 1; state->reloc_model = CLANG_RELOC_PIC; }
        else if (strcmp(a, "-fPIC") == 0)  { state->fPIC = 1; state->reloc_model = CLANG_RELOC_PIC; }
        else if (strcmp(a, "-fpie") == 0)  { state->fpie = 1; state->reloc_model = CLANG_RELOC_PIE; }
        else if (strcmp(a, "-fPIE") == 0)  { state->fPIE = 1; state->reloc_model = CLANG_RELOC_PIE; }
        else if (strcmp(a, "-fno-pic") == 0) state->fno_pic = 1;
        else if (strcmp(a, "-fno-pie") == 0) state->fno_pie = 1;
        else if (strcmp(a, "-no-pie") == 0) state->no_pie = 1;
        else if (strcmp(a, "-pipe") == 0) state->pipe = 1;
        /* Codegen flags */
        else if (strcmp(a, "-ffunction-sections") == 0) {} /* accepted */
        else if (strcmp(a, "-fdata-sections") == 0)    state->gc_sections = 1;
        else if (strcmp(a, "-fno-stack-protector") == 0) state->fstack_protector = 0;
        else if (strcmp(a, "-fstack-protector") == 0) state->fstack_protector = 1;
        else if (strcmp(a, "-fstack-protector-strong") == 0) state->fstack_protector_strong = 1;
        else if (strcmp(a, "-fstack-protector-all") == 0) state->fstack_protector_all = 1;
        else if (strcmp(a, "-fomit-frame-pointer") == 0) state->fomit_frame_pointer = 1;
        else if (strcmp(a, "-fno-omit-frame-pointer") == 0) state->fno_frame_pointer = 1;
        else if (strcmp(a, "-ffp-contract=on") == 0)  state->fffp_contract = 1;
        else if (strcmp(a, "-fno-math-errno") == 0)  state->fno_math_errno = 1;
        else if (strcmp(a, "-ffast-math") == 0)       state->ffast_math = 1;
        else if (strcmp(a, "-fvectorize") == 0)       state->fvectorize = 1;
        else if (strcmp(a, "-fslp-vectorize") == 0)   state->fslp_vectorize = 1;
        else if (strcmp(a, "-fno-exceptions") == 0)  { /* accepted, no state field */ }
        else if (strcmp(a, "-fno-rtti") == 0)         { /* accepted, no state field */ }
        else if (strcmp(a, "-fblocks") == 0)          state->fblocks = 1;
        else if (strcmp(a, "-fobjc-arc") == 0)        state->fobjc_arc = 1;
        else if (strcmp(a, "-fopenmp") == 0)          state->fopenmp = 1;
        else if (strcmp(a, "-ftrigraphs") == 0)       state->ftrigraphs = 1;
        else if (strcmp(a, "-fintegrated-as") == 0)   state->integrate_as = 1;
        else if (strcmp(a, "-fno-integrated-as") == 0) state->no_integrate_as = 1;
        /* LTO */
        else if (strcmp(a, "-flto") == 0)         state->lto_mode = CLANG_LTO_FULL;
        else if (strcmp(a, "-flto=full") == 0)     state->lto_mode = CLANG_LTO_FULL;
        else if (strcmp(a, "-flto=thin") == 0)    state->lto_mode = CLANG_LTO_THIN;
        /* Sanitizers */
        else if (strncmp(a, "-fsanitize=", 11) == 0) {
            const char *s = a + 11;
            if (strstr(s, "address"))   state->sanitizers |= CLANG_SAN_ADDRESS;
            if (strstr(s, "leak"))      state->sanitizers |= CLANG_SAN_LEAK;
            if (strstr(s, "undefined")) state->sanitizers |= CLANG_SAN_UNDEFINED;
            if (strstr(s, "thread"))    state->sanitizers |= CLANG_SAN_THREAD;
            if (strstr(s, "memory"))    state->sanitizers |= CLANG_SAN_MEMORY;
            if (strstr(s, "hwaddress")) state->sanitizers |= CLANG_SAN_HWADDRESS;
            if (strstr(s, "safe"))      state->sanitizers |= CLANG_SAN_SAFESTACK;
            if (strstr(s, "cfi"))       state->sanitizers |= CLANG_SAN_CFI;
            if (strstr(s, "scudo"))     state->sanitizers |= CLANG_SAN_SCUDO;
        }
        /* Diagnostics */
        else if (strcmp(a, "-Wall") == 0)        state->wall = 1;
        else if (strcmp(a, "-Wextra") == 0)       state->wextra = 1;
        else if (strcmp(a, "-Werror") == 0)       state->werror = 1;
        else if (strcmp(a, "-Weverything") == 0) state->weverything = 1;
        else if (strcmp(a, "-Wpedantic") == 0)   state->wpedantic = 1;
        else if (strcmp(a, "-fcolor-diagnostics") == 0) state->fcolor_diagnostics = 1;
        else if (strcmp(a, "-fno-color-diagnostics") == 0) state->fno_color_diagnostics = 1;
        else if (strcmp(a, "-pthread") == 0)      { /* accepted, links libpthread on host */ }
        else if (strcmp(a, "-save-temps") == 0)  state->save_temps = 1;
        /* Unknown: warn but keep going. */
        else {
            if (state->verbose)
                fprintf(stderr, "clang: warning: unknown option '%s' ignored\n", a);
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Job construction / tool resolution (stubs that satisfy the header)
 * ------------------------------------------------------------------------- */

int clang_resolve_tools(ClangState *state) {
    /* Tools are resolved to their default paths in clang_init. */
    (void)state;
    return 0;
}

int clang_build_jobs(ClangState *state, ClangJob *jobs, int *out_n) {
    int n = 0;
    if (!state || !jobs || !out_n) return -1;
    for (int i = 0; i < state->input_count && n < CLANG_MAX_INPUT_FILES; i++) {
        ClangJob *j = &jobs[n];
        memset(j, 0, sizeof(*j));
        strncpy(j->input, state->input_files[i], CLANG_MAX_PATH_LEN - 1);
        if (state->language == CLANG_LANG_AUTO)
            clang_infer_language(j->input, &j->lang);
        else
            j->lang = state->language;
        j->phase = state->output_kind;
        /* Determine output name. */
        char ext[16];
        clang_output_extension(state->output_kind, j->lang, ext, sizeof(ext));
        const char *slash = strrchr(j->input, '/');
        const char *bslash = strrchr(j->input, '\\');
        const char *base = (slash && (!bslash || slash > bslash)) ? slash + 1
                        : (bslash ? bslash + 1 : j->input);
        char stem[CLANG_MAX_PATH_LEN];
        strncpy(stem, base, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = 0;
        char *dot = strrchr(stem, '.');
        if (dot) *dot = 0;
        snprintf(j->output, CLANG_MAX_PATH_LEN, "%s%s", stem, ext);
        n++;
    }
    *out_n = n;
    return 0;
}

static void clang_push(char ***pargv, int *pargc, int *pcap, const char *s) {
    if (!s) return;
    if (*pargc + 1 >= *pcap) {
        int ncap = *pcap ? *pcap * 2 : 16;
        char **na = (char **)realloc(*pargv, sizeof(char *) * ncap);
        if (!na) return;
        *pargv = na;
        *pcap = ncap;
    }
    size_t len = strlen(s) + 1;
    char *dup = (char *)malloc(len);
    if (!dup) return;
    memcpy(dup, s, len);
    (*pargv)[(*pargc)++] = dup;
    (*pargv)[*pargc] = NULL;
}

int clang_build_cc1_args(const ClangState *state, const ClangJob *job, char ***out_argv) {
    if (!state || !job || !out_argv) return -1;
    char **argv = NULL; int argc = 0, cap = 0;
    clang_push(&argv, &argc, &cap, state->clang_cc1_path[0] ? state->clang_cc1_path : "clang");
    clang_push(&argv, &argc, &cap, job->input);
    clang_push(&argv, &argc, &cap, "-o");
    clang_push(&argv, &argc, &cap, job->output);
    ClangOutputKind k = job->phase;
    if (k == CLANG_OUTPUT_PREPROCESS)   clang_push(&argv, &argc, &cap, "-E");
    else if (k == CLANG_OUTPUT_COMPILE) clang_push(&argv, &argc, &cap, "-S");
    else if (k == CLANG_OUTPUT_ASSEMBLE) clang_push(&argv, &argc, &cap, "-c");
    else if (k == CLANG_OUTPUT_EMIT_LLVM) clang_push(&argv, &argc, &cap, "-emit-llvm");
    else if (k == CLANG_OUTPUT_FS_ONLY) clang_push(&argv, &argc, &cap, "-fsyntax-only");
    else clang_push(&argv, &argc, &cap, "-c");
    clang_push(&argv, &argc, &cap, clang_opt_flag(state->opt_level));
    const char *df = clang_debug_flag(state->debug_level);
    if (df) clang_push(&argv, &argc, &cap, df);
    const char *sf = clang_std_flag(state->std_version);
    if (sf) clang_push(&argv, &argc, &cap, sf);
    for (int i = 0; i < state->define_count; i++) {
        char buf[CLANG_MAX_PATH_LEN + 4];
        snprintf(buf, sizeof(buf), "-D%s", state->defines[i]);
        clang_push(&argv, &argc, &cap, buf);
    }
    for (int i = 0; i < state->undefine_count; i++) {
        char buf[CLANG_MAX_PATH_LEN + 4];
        snprintf(buf, sizeof(buf), "-U%s", state->undefines[i]);
        clang_push(&argv, &argc, &cap, buf);
    }
    for (int i = 0; i < state->include_dir_count; i++) {
        char buf[CLANG_MAX_PATH_LEN + 4];
        snprintf(buf, sizeof(buf), "-I%s", state->include_dirs[i]);
        clang_push(&argv, &argc, &cap, buf);
    }
    if (state->target_triple[0]) {
        char buf[CLANG_MAX_PATH_LEN + 16];
        snprintf(buf, sizeof(buf), "-target=%s", state->target_triple);
        clang_push(&argv, &argc, &cap, buf);
    }
    if (state->sysroot[0]) {
        char buf[CLANG_MAX_PATH_LEN + 16];
        snprintf(buf, sizeof(buf), "--sysroot=%s", state->sysroot);
        clang_push(&argv, &argc, &cap, buf);
    }
    if (state->freestanding) clang_push(&argv, &argc, &cap, "-ffreestanding");
    if (state->nostdinc) clang_push(&argv, &argc, &cap, "-nostdinc");
    if (state->fstack_protector == 0) clang_push(&argv, &argc, &cap, "-fno-stack-protector");
    if (state->fomit_frame_pointer) clang_push(&argv, &argc, &cap, "-fomit-frame-pointer");
    if (state->wall) clang_push(&argv, &argc, &cap, "-Wall");
    if (state->wextra) clang_push(&argv, &argc, &cap, "-Wextra");
    if (state->werror) clang_push(&argv, &argc, &cap, "-Werror");
    *out_argv = argv;
    return argc;
}

int clang_build_cc1as_args(const ClangState *state, const ClangJob *job, char ***out_argv) {
    if (!state || !job || !out_argv) return -1;
    char **argv = NULL; int argc = 0, cap = 0;
    clang_push(&argv, &argc, &cap, "clang");
    clang_push(&argv, &argc, &cap, "-c");
    clang_push(&argv, &argc, &cap, job->input);
    clang_push(&argv, &argc, &cap, "-o");
    clang_push(&argv, &argc, &cap, job->output);
    *out_argv = argv;
    return argc;
}

int clang_build_external_as_args(const ClangState *state, const ClangJob *job, char ***out_argv) {
    if (!state || !job || !out_argv) return -1;
    char **argv = NULL; int argc = 0, cap = 0;
    clang_push(&argv, &argc, &cap, state->as_path[0] ? state->as_path : "as");
    clang_push(&argv, &argc, &cap, job->input);
    clang_push(&argv, &argc, &cap, "-o");
    clang_push(&argv, &argc, &cap, job->output);
    *out_argv = argv;
    return argc;
}

int clang_build_ld_args(const ClangState *state, const ClangJob *jobs, int njobs,
                        char ***out_argv) {
    if (!state || !jobs || !out_argv) return -1;
    char **argv = NULL; int argc = 0, cap = 0;
    clang_push(&argv, &argc, &cap, state->ld_path[0] ? state->ld_path : "ld");
    clang_push(&argv, &argc, &cap, "-o");
    clang_push(&argv, &argc, &cap, state->output_file[0] ? state->output_file : "a.out");
    if (state->nostdlib) clang_push(&argv, &argc, &cap, "-nostdlib");
    if (state->static_link) clang_push(&argv, &argc, &cap, "-static");
    if (state->shared) clang_push(&argv, &argc, &cap, "-shared");
    if (state->gc_sections) clang_push(&argv, &argc, &cap, "--gc-sections");
    if (state->linker_script[0]) {
        clang_push(&argv, &argc, &cap, "-T");
        clang_push(&argv, &argc, &cap, state->linker_script);
    }
    for (int i = 0; i < state->library_dir_count; i++) {
        char buf[CLANG_MAX_PATH_LEN + 4];
        snprintf(buf, sizeof(buf), "-L%s", state->library_dirs[i]);
        clang_push(&argv, &argc, &cap, buf);
    }
    for (int j = 0; j < njobs; j++) {
        if (jobs[j].output[0])
            clang_push(&argv, &argc, &cap, jobs[j].output);
    }
    if (!state->nostdlib) {
        clang_push(&argv, &argc, &cap, "-lc");
        clang_push(&argv, &argc, &cap, "-lkapi");
    }
    for (int i = 0; i < state->library_count; i++) {
        char buf[CLANG_MAX_PATH_LEN + 4];
        snprintf(buf, sizeof(buf), "-l%s", state->libraries[i]);
        clang_push(&argv, &argc, &cap, buf);
    }
    for (int i = 0; i < state->linker_flag_count; i++)
        clang_push(&argv, &argc, &cap, state->linker_flags[i]);
    *out_argv = argv;
    return argc;
}

/* ---------------------------------------------------------------------------
 * Execution
 * ------------------------------------------------------------------------- */

int clang_run(const char *argv[]) {
    return clang_run_argv(argv);
}

int clang_execute_jobs(ClangState *state, const ClangJob *jobs, int njobs) {
    if (!state || !jobs) return -1;
    for (int j = 0; j < njobs; j++) {
        if (state->output_kind == CLANG_OUTPUT_LINK) {
            /* In link mode each input is compiled to .o first. */
            ClangJob sub = jobs[j];
            sub.phase = CLANG_OUTPUT_ASSEMBLE;
            char ext[8] = {0};
            clang_output_extension(CLANG_OUTPUT_ASSEMBLE, sub.lang, ext, sizeof(ext));
            const char *slash = strrchr(sub.input, '/');
            const char *base = slash ? slash + 1 : sub.input;
            char stem[CLANG_MAX_PATH_LEN];
            strncpy(stem, base, sizeof(stem) - 1); stem[sizeof(stem) - 1] = 0;
            char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
            snprintf(sub.output, CLANG_MAX_PATH_LEN, "%s%s", stem, ext);
            char **argv = NULL;
            int n = clang_build_cc1_args(state, &sub, &argv);
            if (argv && n > 0) {
                int rc = clang_run((const char **)argv);
                for (int k = 0; k < n; k++) free(argv[k]);
                free(argv);
                if (rc != 0) { state->exit_code = rc; return rc; }
            }
        } else {
            char **argv = NULL;
            int n = clang_build_cc1_args(state, &jobs[j], &argv);
            if (argv && n > 0) {
                int rc = clang_run((const char **)argv);
                for (int k = 0; k < n; k++) free(argv[k]);
                free(argv);
                if (rc != 0) { state->exit_code = rc; return rc; }
            }
        }
    }
    return 0;
}

int clang_do_link(ClangState *state, const ClangJob *jobs, int njobs) {
    if (!state) return -1;
    if (state->output_kind != CLANG_OUTPUT_LINK) return 0;
    if (state->verbose)
        fprintf(stderr, "clang: linking -> %s\n",
                state->output_file[0] ? state->output_file : "a.out");
    char **argv = NULL;
    int n = clang_build_ld_args(state, jobs, njobs, &argv);
    if (!argv || n <= 0) return -1;
    int rc = clang_run((const char **)argv);
    for (int k = 0; k < n; k++) free(argv[k]);
    free(argv);
    if (rc != 0) state->exit_code = rc;
    return rc;
}

/* ---------------------------------------------------------------------------
 * Reporting
 * ------------------------------------------------------------------------- */

void clang_show_version(void) {
    printf("KenuxK clang version %s\n", CLANG_DRIVER_VERSION);
    printf("Target: %s\n", CLANG_DEFAULT_TARGET);
    printf("Thread model: posix\n");
    printf("InstalledDir: /usr/bin\n");
}

void clang_print_usage(void) {
    printf("OVERVIEW: KenuxK clang LLVM compiler driver (minimal)\n\n");
    printf("USAGE: clang [options] file...\n\n");
    printf("OPTIONS:\n");
    printf("  -c, -S, -E             Stop after assemble/compile/preprocess\n");
    printf("  -o <file>              Output file\n");
    printf("  -O0/-O1/-O2/-O3/-Os/-Oz/-Ofast/-Og  Optimization\n");
    printf("  -std=<standard>        Language standard\n");
    printf("  -g / -g3 / -ggdb       Debug info\n");
    printf("  -Wall -Wextra -Werror  Warnings\n");
    printf("  -fsanitize=<list>      address,leak,undefined,thread,memory,hwaddress\n");
    printf("  -flto / -flto=thin     Link-time optimization\n");
    printf("  -target=<triple>       Target triple\n");
    printf("  -mcmodel=kernel        Kernel code model\n");
    printf("  -ffreestanding         Freestanding compilation\n");
    printf("  -fno-stack-protector   Disable stack protector\n");
    printf("  -nostdlib              No standard libraries\n");
    printf("  -static/-shared        Static/shared link\n");
    printf("  --version              Version\n");
    printf("  --help                 This help\n");
}

void clang_print_cc1_help(void) {
    printf("clang cc1 options (minimal):\n");
    printf("  -emit-obj -emit-llvm -fsyntax-only -E -S\n");
    printf("  -std= -O0..3 -g -I -D -U\n");
}

void clang_print_search_dirs(const ClangState *state) {
    if (!state) return;
    printf("programs: =%s\n", state->clang_cc1_path);
    printf("libraries: ");
    for (int i = 0; i < state->library_dir_count; i++)
        printf("%s%s", state->library_dirs[i],
               (i + 1 < state->library_dir_count) ? ":" : "\n");
}

/* ---------------------------------------------------------------------------
 * Main entry
 * ------------------------------------------------------------------------- */

#ifndef KENUXK_NO_MAIN_CLANG
int main(int argc, char **argv) {
    static ClangState s;
    clang_init(&s);
    clang_parse_arguments(&s, argc, argv);
    if (s.help) { clang_print_usage(); return 0; }
    if (s.help_hidden) { clang_print_cc1_help(); return 0; }
    if (s.version) { clang_show_version(); return 0; }
    if (s.print_search_dirs) { clang_print_search_dirs(&s); return 0; }
    if (s.dumpversion) { printf("18.1.0\n"); return 0; }
    if (s.dumpmachine) { printf("%s\n", CLANG_DEFAULT_TARGET); return 0; }
    if (s.print_prog_name) { printf("%s\n", s.clang_cc1_path); return 0; }
    if (s.print_file_name) { printf("%s\n", s.ld_path); return 0; }

    if (s.input_count == 0) {
        fprintf(stderr, "clang: error: no input files\n");
        return 1;
    }
    static ClangJob jobs[CLANG_MAX_INPUT_FILES];
    int njobs = 0;
    clang_build_jobs(&s, jobs, &njobs);
    if (clang_execute_jobs(&s, jobs, njobs) != 0)
        return s.exit_code;
    if (s.output_kind == CLANG_OUTPUT_LINK)
        clang_do_link(&s, jobs, njobs);
    return s.exit_code;
}
#endif
