/*
 * Kenux OS - GCC Compiler Driver (Minimal)
 * Main gcc driver implementation
 */

#include "gcc.h"

#define GCC_VERSION_STR "KenuxK-GCC 13.2.0 (Minimal Driver)"
#define GCC_MACHINE_STR "x86_64-kenuxk"
#define GCC_SPEC_STR \
  "*asm:\n" \
  "%{v:-V}\n" \
  "\n*link:\n" \
  "%{!nostdlib: -lc -lkapi}\n" \
  "\n*lib:\n" \
  "-lc -lkapi\n"

void gcc_init(GccState *state) {
    memset(state, 0, sizeof(GccState));
    state->opt_level = OPT_2;
    state->std_version = STD_GNU17;
    state->arch = ARCH_X86_64;
    state->language = LANG_AUTO;
    state->output_kind = OUTPUT_LINK;
    state->float_abi = FP_SSE;
    state->diag_color = DIAG_COLOR_AUTO;
    state->fstack_protector = 1;
    state->pipe = 1;
    state->exit_code = 0;
    state->error_count = 0;
    state->warning_count = 0;
    /* Default search paths */
    strcpy(state->cc1_path, "/usr/libexec/gcc/x86_64-kenuxk/13/cc1");
    strcpy(state->as_path, "/usr/bin/as");
    strcpy(state->ld_path, "/usr/bin/ld");
    strcpy(state->cpp_path, "/usr/bin/cpp");
    strcpy(state->collect2_path, "/usr/libexec/gcc/x86_64-kenuxk/13/collect2");
    strcpy(state->temp_dir, "/tmp");
    /* Default include paths for KenuxK */
    strcpy(state->include_dirs[0], "/usr/include");
    state->include_dir_count = 1;
    strcpy(state->library_dirs[0], "/usr/lib");
    state->library_dir_count = 1;
}

const char *gcc_std_option(StdVersion std) {
    switch (std) {
        case STD_C89: return "-std=c89";
        case STD_C90: return "-std=c90";
        case STD_C94: return "-std=c94";
        case STD_C99: return "-std=c99";
        case STD_C11: return "-std=c11";
        case STD_C17: return "-std=c17";
        case STD_C2X: return "-std=c2x";
        case STD_CXX98: return "-std=c++98";
        case STD_CXX03: return "-std=c++03";
        case STD_CXX11: return "-std=c++11";
        case STD_CXX14: return "-std=c++14";
        case STD_CXX17: return "-std=c++17";
        case STD_CXX20: return "-std=c++20";
        case STD_GNU89: return "-std=gnu89";
        case STD_GNU99: return "-std=gnu99";
        case STD_GNU11: return "-std=gnu11";
        case STD_GNU17: return "-std=gnu17";
        default: return NULL;
    }
}

const char *gcc_arch_option(ArchTarget arch) {
    switch (arch) {
        case ARCH_X86: return "-m32";
        case ARCH_X86_64: return "-m64";
        case ARCH_ARM: return "-march=armv7-a";
        case ARCH_AARCH64: return "-march=armv8-a";
        case ARCH_RISCV32: return "-march=rv32gc";
        case ARCH_RISCV64: return "-march=rv64gc";
        case ARCH_MIPS: return "-march=mips32";
        case ARCH_PPC: return "-march=powerpc";
        case ARCH_PPC64: return "-march=powerpc64";
        default: return NULL;
    }
}

const char *gcc_opt_option(OptLevel opt) {
    switch (opt) {
        case OPT_NONE: return "-O0";
        case OPT_1: return "-O1";
        case OPT_2: return "-O2";
        case OPT_3: return "-O3";
        case OPT_S: return "-Os";
        case OPT_FAST: return "-Ofast";
        case OPT_DEBUG: return "-Og";
        default: return "-O2";
    }
}

const char *gcc_debug_option(DebugLevel dbg) {
    switch (dbg) {
        case DEBUG_NONE: return NULL;
        case DEBUG_MINIMAL: return "-g1";
        case DEBUG_DEFAULT: return "-g";
        case DEBUG_EXTENDED: return "-g3";
        case DEBUG_GDB: return "-ggdb";
        default: return NULL;
    }
}

const char *language_name(LanguageMode lang) {
    switch (lang) {
        case LANG_C: return "c";
        case LANG_CXX: return "c++";
        case LANG_ASSEMBLER: return "assembler";
        case LANG_PREPROCESSOR: return "cpp-output";
        case LANG_OBJECTIVE_C: return "objective-c";
        case LANG_OBJECTIVE_CXX: return "objective-c++";
        default: return "none";
    }
}

LanguageMode language_from_name(const char *name) {
    if (!name) return LANG_AUTO;
    if (strcmp(name, "c") == 0) return LANG_C;
    if (strcmp(name, "c++") == 0) return LANG_CXX;
    if (strcmp(name, "assembler") == 0) return LANG_ASSEMBLER;
    if (strcmp(name, "cpp-output") == 0) return LANG_PREPROCESSOR;
    if (strcmp(name, "objective-c") == 0) return LANG_OBJECTIVE_C;
    if (strcmp(name, "objective-c++") == 0) return LANG_OBJECTIVE_CXX;
    return LANG_AUTO;
}

int file_extension_matches(const char *filename, const char *ext) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return strcmp(dot + 1, ext) == 0;
}

const char *input_extension_for_language(LanguageMode lang) {
    switch (lang) {
        case LANG_C: return "c";
        case LANG_CXX: return "cpp";
        case LANG_ASSEMBLER: return "s";
        case LANG_PREPROCESSOR: return "i";
        default: return NULL;
    }
}

int gcc_infer_language(GccState *state, const char *filename, LanguageMode *out_lang) {
    (void)state;
    if (!filename || !out_lang) return -1;
    LanguageMode lang = LANG_AUTO;
    if (file_extension_matches(filename, "c") ||
        file_extension_matches(filename, "C")) {
        lang = LANG_C;
    } else if (file_extension_matches(filename, "i")) {
        lang = LANG_PREPROCESSOR;
    } else if (file_extension_matches(filename, "cpp") ||
               file_extension_matches(filename, "cxx") ||
               file_extension_matches(filename, "cc") ||
               file_extension_matches(filename, "C++") ||
               file_extension_matches(filename, "CPP")) {
        lang = LANG_CXX;
    } else if (file_extension_matches(filename, "s") ||
               file_extension_matches(filename, "S")) {
        lang = LANG_ASSEMBLER;
    } else if (file_extension_matches(filename, "m")) {
        lang = LANG_OBJECTIVE_C;
    } else if (file_extension_matches(filename, "mm") ||
               file_extension_matches(filename, "M")) {
        lang = LANG_OBJECTIVE_CXX;
    }
    *out_lang = lang;
    return 0;
}

int gcc_parse_arguments(GccState *state, int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!arg) continue;
        if (arg[0] != '-' && !(arg[0] == '.' && arg[1] == '/') &&
            strchr(arg, '.') != NULL) {
            /* Input file */
            if (state->input_count < MAX_INPUT_FILES) {
                strncpy(state->input_files[state->input_count], arg, MAX_PATH_LEN - 1);
                state->input_files[state->input_count][MAX_PATH_LEN - 1] = '\0';
                state->input_count++;
            }
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            state->verbose = 1;
        } else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
            state->quiet = 1;
        } else if (strcmp(arg, "-pipe") == 0) {
            state->pipe = 1;
        } else if (strncmp(arg, "-o", 2) == 0) {
            const char *val = (arg[2] != '\0') ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (val) strncpy(state->output_file, val, MAX_PATH_LEN - 1);
        } else if (strcmp(arg, "-c") == 0) {
            state->output_kind = OUTPUT_ASSEMBLE;
        } else if (strcmp(arg, "-S") == 0) {
            state->output_kind = OUTPUT_COMPILE;
        } else if (strcmp(arg, "-E") == 0) {
            state->output_kind = OUTPUT_PREPROCESS;
        } else if (strcmp(arg, "-s") == 0) {
            state->strip = 1;
        } else if (strcmp(arg, "-static") == 0) {
            state->static_link = 1;
        } else if (strcmp(arg, "-shared") == 0) {
            state->shared = 1;
            state->build_shared = 1;
        } else if (strcmp(arg, "-nostdlib") == 0) {
            state->nostdlib = 1;
        } else if (strcmp(arg, "-nostartfiles") == 0) {
            state->nostartfiles = 1;
        } else if (strcmp(arg, "-nodefaultlibs") == 0) {
            state->nodefaultlibs = 1;
        } else if (strcmp(arg, "-ffreestanding") == 0) {
            state->freestanding = 1;
        } else if (strcmp(arg, "-fno-pie") == 0 || strcmp(arg, "-no-pie") == 0) {
            state->no_pie = 1;
        } else if (strcmp(arg, "-fpie") == 0) { state->pie_mode = PIE_EXECUTABLE; state->fpie = 1; }
        else if (strcmp(arg, "-fPIE") == 0) { state->pie_mode = PIE_EXECUTABLE; state->fPIE = 1; }
        else if (strcmp(arg, "-fpic") == 0) { state->pie_mode = PIC_SHARED; state->fpic = 1; }
        else if (strcmp(arg, "-fPIC") == 0) { state->pie_mode = PIC_SHARED; state->fPIC = 1; }
        else if (strcmp(arg, "-rdynamic") == 0) { state->rdynamic = 1; }
        else if (strcmp(arg, "-O0") == 0) state->opt_level = OPT_NONE;
        else if (strcmp(arg, "-O1") == 0) state->opt_level = OPT_1;
        else if (strcmp(arg, "-O2") == 0 || strcmp(arg, "-O") == 0) state->opt_level = OPT_2;
        else if (strcmp(arg, "-O3") == 0) state->opt_level = OPT_3;
        else if (strcmp(arg, "-Os") == 0) state->opt_level = OPT_S;
        else if (strcmp(arg, "-Ofast") == 0) state->opt_level = OPT_FAST;
        else if (strcmp(arg, "-Og") == 0) state->opt_level = OPT_DEBUG;
        else if (strcmp(arg, "-g") == 0) state->debug_level = DEBUG_DEFAULT;
        else if (strcmp(arg, "-g1") == 0) state->debug_level = DEBUG_MINIMAL;
        else if (strcmp(arg, "-g3") == 0) state->debug_level = DEBUG_EXTENDED;
        else if (strcmp(arg, "-ggdb") == 0) state->debug_level = DEBUG_GDB;
        else if (strncmp(arg, "-std=", 5) == 0) {
            const char *s = arg + 5;
            if (strcmp(s, "c89") == 0) state->std_version = STD_C89;
            else if (strcmp(s, "c90") == 0) state->std_version = STD_C90;
            else if (strcmp(s, "c99") == 0) state->std_version = STD_C99;
            else if (strcmp(s, "c11") == 0) state->std_version = STD_C11;
            else if (strcmp(s, "c17") == 0) state->std_version = STD_C17;
            else if (strcmp(s, "c2x") == 0 || strcmp(s, "c23") == 0) state->std_version = STD_C2X;
            else if (strcmp(s, "gnu89") == 0) state->std_version = STD_GNU89;
            else if (strcmp(s, "gnu99") == 0) state->std_version = STD_GNU99;
            else if (strcmp(s, "gnu11") == 0) state->std_version = STD_GNU11;
            else if (strcmp(s, "gnu17") == 0) state->std_version = STD_GNU17;
            else if (strcmp(s, "c++98") == 0) state->std_version = STD_CXX98;
            else if (strcmp(s, "c++11") == 0) state->std_version = STD_CXX11;
            else if (strcmp(s, "c++14") == 0) state->std_version = STD_CXX14;
            else if (strcmp(s, "c++17") == 0) state->std_version = STD_CXX17;
            else if (strcmp(s, "c++20") == 0) state->std_version = STD_CXX20;
        } else if (strncmp(arg, "-x", 2) == 0) {
            const char *lang = (arg[2] != '\0') ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (lang) state->language = language_from_name(lang);
        } else if (strcmp(arg, "-m32") == 0) {
            state->arch = ARCH_X86; state->m32 = 1; state->m64 = 0;
        } else if (strcmp(arg, "-m64") == 0) {
            state->arch = ARCH_X86_64; state->m64 = 1; state->m32 = 0;
        } else if (strncmp(arg, "-march=", 7) == 0) {
            strncpy(state->march, arg + 7, sizeof(state->march) - 1);
        } else if (strncmp(arg, "-mtune=", 7) == 0) {
            strncpy(state->mtune, arg + 7, sizeof(state->mtune) - 1);
        } else if (strncmp(arg, "-D", 2) == 0) {
            const char *d = arg + 2;
            if (state->define_count < MAX_DEFINE) {
                strncpy(state->defines[state->define_count++], d, MAX_PATH_LEN - 1);
            }
        } else if (strncmp(arg, "-U", 2) == 0) {
            const char *d = arg + 2;
            if (state->undefine_count < MAX_DEFINE) {
                strncpy(state->undefines[state->undefine_count++], d, MAX_PATH_LEN - 1);
            }
        } else if (strncmp(arg, "-I", 2) == 0) {
            const char *d = arg + 2;
            if (state->include_dir_count < MAX_INCLUDE_DIRS) {
                strncpy(state->include_dirs[state->include_dir_count++], d, MAX_PATH_LEN - 1);
            }
        } else if (strncmp(arg, "-L", 2) == 0) {
            const char *d = arg + 2;
            if (state->library_dir_count < MAX_LIBRARY_DIRS) {
                strncpy(state->library_dirs[state->library_dir_count++], d, MAX_PATH_LEN - 1);
            }
        } else if (strncmp(arg, "-l", 2) == 0) {
            const char *d = arg + 2;
            if (state->library_count < MAX_LIBRARIES) {
                strncpy(state->libraries[state->library_count++], d, MAX_PATH_LEN - 1);
            }
        } else if (strcmp(arg, "-Wall") == 0) state->wall = 1;
        else if (strcmp(arg, "-Wextra") == 0) state->wextra = 1;
        else if (strcmp(arg, "-Werror") == 0) state->werror = 1;
        else if (strcmp(arg, "-Wpedantic") == 0) state->wpedantic = 1;
        else if (strcmp(arg, "-Wshadow") == 0) state->wshadow = 1;
        else if (strcmp(arg, "-Wno-all") == 0) state->wno_all = 1;
        else if (strcmp(arg, "-Wunused") == 0) state->wunused = 1;
        else if (strcmp(arg, "-Wimplicit") == 0) state->wimplicit = 1;
        else if (strcmp(arg, "-fomit-frame-pointer") == 0) state->fomit_frame_pointer = 1;
        else if (strcmp(arg, "-fno-omit-frame-pointer") == 0) state->fno_omit_frame_pointer = 1;
        else if (strcmp(arg, "-funroll-loops") == 0) state->funroll_loops = 1;
        else if (strcmp(arg, "-finline-functions") == 0) state->finline_functions = 1;
        else if (strcmp(arg, "-ftree-vectorize") == 0) state->ftree_vectorize = 1;
        else if (strcmp(arg, "-fstrict-aliasing") == 0) state->fstrict_aliasing = 1;
        else if (strcmp(arg, "-fno-strict-aliasing") == 0) state->fno_strict_aliasing = 1;
        else if (strcmp(arg, "-ffast-math") == 0) state->ffast_math = 1;
        else if (strcmp(arg, "-fstack-protector") == 0) state->fstack_protector = 1;
        else if (strcmp(arg, "-fstack-protector-strong") == 0) state->fstack_protector_strong = 1;
        else if (strcmp(arg, "-fstack-clash-protection") == 0) state->fstack_clash_protection = 1;
        else if (strcmp(arg, "-fcf-protection") == 0) state->fcf_protection = 1;
        else if (strcmp(arg, "-flto") == 0) state->lto_mode = LTO_ENABLE;
        else if (strcmp(arg, "-save-temps") == 0) state->save_temps = 1;
        else if (strcmp(arg, "-nostdinc") == 0) state->nostdinc = 1;
        else if (strncmp(arg, "--sysroot=", 10) == 0) {
            strncpy(state->sysroot, arg + 10, MAX_PATH_LEN - 1);
        } else if (strncmp(arg, "-Wl,", 4) == 0) {
            const char *f = arg + 4;
            if (state->linker_flag_count < MAX_LINKER_FLAGS) {
                strncpy(state->linker_flags[state->linker_flag_count++], f, MAX_PATH_LEN - 1);
            }
            if (strncmp(f, "-e,", 3) == 0) {
                strncpy(state->entry_point, f + 3, MAX_PATH_LEN - 1);
            }
            if (strncmp(f, "--gc-sections", 13) == 0) state->gc_sections = 1;
            if (strncmp(f, "-E", 2) == 0) state->export_dynamic = 1;
        } else if (strncmp(arg, "-T", 2) == 0) {
            const char *s = (arg[2] != '\0') ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (s && state->linker_script_count < MAX_LIBRARY_DIRS) {
                strncpy(state->linker_scripts[state->linker_script_count++], s, MAX_PATH_LEN - 1);
            }
        } else if (strncmp(arg, "-fsanitize=", 11) == 0) {
            const char *s = arg + 11;
            if (strstr(s, "address")) state->sanitizers |= SAN_ADDRESS;
            if (strstr(s, "undefined")) state->sanitizers |= SAN_UNDEFINED;
            if (strstr(s, "thread")) state->sanitizers |= SAN_THREAD;
            if (strstr(s, "memory")) state->sanitizers |= SAN_MEMORY;
            if (strstr(s, "leak")) state->sanitizers |= SAN_LEAK;
        } else if (strcmp(arg, "--version") == 0) {
            state->dumpversion = 1;
        } else if (strcmp(arg, "-dumpmachine") == 0) {
            state->dumpmachine = 1;
        } else if (strcmp(arg, "-dumpspecs") == 0) {
            state->dumpspecs = 1;
        } else if (strcmp(arg, "-print-search-dirs") == 0) {
            state->print_search_dirs = 1;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            state->help = 1;
        } else if (strcmp(arg, "--target-help") == 0) {
            state->target_help = 1;
        } else if (strncmp(arg, "-fprofile-generate", 18) == 0) {
            state->fprofile_generate = 1;
        } else if (strncmp(arg, "-fprofile-use", 13) == 0) {
            state->fprofile_use = 1;
        } else if (strcmp(arg, "-ftime-report") == 0) {
            state->time_report = 1;
        } else if (strcmp(arg, "-pg") == 0) {
            state->p = 1;
        }
    }
    return 0;
}

int gcc_validate_options(GccState *state) {
    if (state->m32 && state->m64) {
        fprintf(stderr, "gcc: error: cannot specify both -m32 and -m64\n");
        return -1;
    }
    if (state->shared && state->static_link) {
        fprintf(stderr, "gcc: error: cannot specify both -shared and -static\n");
        return -1;
    }
    if (state->output_kind != OUTPUT_LINK && state->output_file[0] == '\0' && state->input_count > 1) {
        fprintf(stderr, "gcc: error: cannot specify -o with -c/-S/-E and multiple files\n");
        return -1;
    }
    return 0;
}

int gcc_determine_output_file(GccState *state, const char *input, CompilerPhase phase, char *out_path) {
    if (!input || !out_path) return -1;
    char base[MAX_PATH_LEN];
    strncpy(base, input, MAX_PATH_LEN - 1);
    base[MAX_PATH_LEN - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    switch (phase) {
        case PHASE_PREPROCESS:
            sprintf(out_path, "%s.i", base);
            break;
        case PHASE_COMPILE:
            sprintf(out_path, "%s.s", base);
            break;
        case PHASE_ASSEMBLE:
            sprintf(out_path, "%s.o", base);
            break;
        case PHASE_LINK:
            if (state->output_file[0]) {
                strncpy(out_path, state->output_file, MAX_PATH_LEN - 1);
            } else {
                strcpy(out_path, "a.out");
            }
            break;
    }
    return 0;
}

int gcc_build_jobs(GccState *state, CompileJob *jobs, int *out_job_count) {
    int count = 0;
    for (int i = 0; i < state->input_count; i++) {
        LanguageMode lang = state->language;
        if (lang == LANG_AUTO) {
            gcc_infer_language(state, state->input_files[i], &lang);
        }
        if (lang == LANG_AUTO) {
            fprintf(stderr, "gcc: warning: couldn't infer language for %s\n", state->input_files[i]);
            continue;
        }
        CompilerPhase start_phase;
        if (state->output_kind == OUTPUT_PREPROCESS) {
            start_phase = PHASE_PREPROCESS;
        } else if (lang == LANG_PREPROCESSOR) {
            start_phase = PHASE_COMPILE;
        } else if (lang == LANG_ASSEMBLER) {
            start_phase = (state->output_kind == OUTPUT_COMPILE) ? PHASE_COMPILE : PHASE_ASSEMBLE;
        } else {
            start_phase = (state->output_kind == OUTPUT_COMPILE) ? PHASE_COMPILE : PHASE_PREPROCESS;
        }
        CompilerPhase end_phase;
        if (state->output_kind == OUTPUT_PREPROCESS) end_phase = PHASE_PREPROCESS;
        else if (state->output_kind == OUTPUT_COMPILE) end_phase = PHASE_COMPILE;
        else if (state->output_kind == OUTPUT_ASSEMBLE) end_phase = PHASE_ASSEMBLE;
        else end_phase = PHASE_ASSEMBLE; /* link phase handled separately */
        for (CompilerPhase ph = start_phase; ph <= end_phase; ph = (CompilerPhase)(ph + 1)) {
            if (count >= MAX_INPUT_FILES * 4) break;
            CompileJob *j = &jobs[count++];
            if (ph == start_phase) {
                strncpy(j->input, state->input_files[i], MAX_PATH_LEN - 1);
            } else {
                gcc_determine_output_file(state, state->input_files[i],
                    (CompilerPhase)(ph - 1), j->input);
            }
            gcc_determine_output_file(state, state->input_files[i], ph, j->output);
            j->phase = ph;
            j->lang = lang;
            /* Only apply -o to the final output of the final file when linking */
            if (state->output_kind != OUTPUT_LINK && state->output_file[0] && i == state->input_count - 1 && ph == end_phase) {
                strncpy(j->output, state->output_file, MAX_PATH_LEN - 1);
            }
        }
    }
    *out_job_count = count;
    return 0;
}

int gcc_resolve_tool_paths(GccState *state) {
    static const char *check_paths[] = {
        "/usr/bin", "/usr/local/bin", "/bin", "/opt/kenuxk/bin", NULL
    };
    struct stat st;
    char candidate[MAX_PATH_LEN];
    const char *tools[][2] = {
        {"cc1", state->cc1_path},
        {"as", state->as_path},
        {"ld", state->ld_path},
        {"cpp", state->cpp_path},
        {NULL, NULL}
    };
    for (int t = 0; tools[t][0]; t++) {
        if (stat(tools[t][1], &st) == 0) continue;
        int found = 0;
        for (int p = 0; check_paths[p]; p++) {
            snprintf(candidate, sizeof(candidate), "%s/%s", check_paths[p], tools[t][0]);
            if (stat(candidate, &st) == 0) {
                char *store = (char *)tools[t][1];
                strncpy(store, candidate, MAX_PATH_LEN - 1);
                found = 1;
                break;
            }
        }
        if (!found && state->verbose) {
            fprintf(stderr, "gcc: note: tool %s not found on filesystem, will defer exec\n", tools[t][0]);
        }
    }
    return 0;
}

int gcc_build_cc1_args(const GccState *state, const CompileJob *job, char ***out_argv) {
    (void)state; (void)job; (void)out_argv;
    return 0;
}

int gcc_build_as_args(const GccState *state, const CompileJob *job, char ***out_argv) {
    (void)state; (void)job; (void)out_argv;
    return 0;
}

int gcc_build_ld_args(const GccState *state, const CompileJob *jobs, int njobs, char ***out_argv) {
    (void)state; (void)jobs; (void)njobs; (void)out_argv;
    return 0;
}

int gcc_run_command(const char *argv[]) {
    if (!argv || !argv[0]) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        perror("gcc: fork failed");
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], (char * const *)argv);
        perror(argv[0]);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int gcc_execute_jobs(GccState *state, const CompileJob *jobs, int njobs) {
    for (int i = 0; i < njobs; i++) {
        const CompileJob *j = &jobs[i];
        const char *tool = NULL;
        switch (j->phase) {
            case PHASE_PREPROCESS: tool = state->cpp_path; break;
            case PHASE_COMPILE:    tool = state->cc1_path; break;
            case PHASE_ASSEMBLE:   tool = state->as_path; break;
            default: continue;
        }
        if (state->verbose) {
            fprintf(stderr, "gcc: %s phase: %s -> %s (via %s)\n",
                language_name(j->lang), j->input, j->output, tool);
        }
        /* Minimal driver: run a simple wrapper that performs the actual work */
        const char *argv[16];
        int ai = 0;
        argv[ai++] = tool;
        if (j->phase == PHASE_ASSEMBLE) {
            argv[ai++] = "-o";
            argv[ai++] = j->output;
            argv[ai++] = j->input;
        } else {
            argv[ai++] = j->input;
            argv[ai++] = "-o";
            argv[ai++] = j->output;
            argv[ai++] = gcc_opt_option(state->opt_level);
            if (state->debug_level != DEBUG_NONE) argv[ai++] = gcc_debug_option(state->debug_level);
            const char *s = gcc_std_option(state->std_version);
            if (s) argv[ai++] = s;
            s = gcc_arch_option(state->arch);
            if (s) argv[ai++] = s;
            for (int d = 0; d < state->include_dir_count; d++) {
                char *buf = (char *)malloc(MAX_PATH_LEN + 4);
                if (buf) { snprintf(buf, MAX_PATH_LEN + 4, "-I%s", state->include_dirs[d]); argv[ai++] = buf; }
            }
            for (int d = 0; d < state->define_count; d++) {
                char *buf = (char *)malloc(MAX_PATH_LEN + 4);
                if (buf) { snprintf(buf, MAX_PATH_LEN + 4, "-D%s", state->defines[d]); argv[ai++] = buf; }
            }
        }
        argv[ai] = NULL;
        if (state->verbose) {
            fprintf(stderr, "gcc: invoking:");
            for (int k = 0; argv[k]; k++) fprintf(stderr, " %s", argv[k]);
            fprintf(stderr, "\n");
        }
        int rc = gcc_run_command(argv);
        if (rc != 0) {
            fprintf(stderr, "gcc: error: %s failed with exit code %d\n", tool, rc);
            state->error_count++;
            state->exit_code = rc ? rc : 1;
            return state->exit_code;
        }
    }
    return 0;
}

int gcc_do_link(GccState *state, const CompileJob *obj_jobs, int njobs) {
    (void)obj_jobs; (void)njobs;
    if (state->output_kind != OUTPUT_LINK) return 0;
    char out[MAX_PATH_LEN];
    if (state->output_file[0]) strncpy(out, state->output_file, MAX_PATH_LEN - 1);
    else strcpy(out, "a.out");
    if (state->verbose) {
        fprintf(stderr, "gcc: linking -> %s (via %s)\n", out, state->collect2_path);
    }
    /* Collect object files from inputs and jobs into a minimal ld invocation */
    const char *argv[MAX_INPUT_FILES + 64];
    int ai = 0;
    argv[ai++] = state->ld_path;
    argv[ai++] = "-o";
    argv[ai++] = out;
    if (state->nostdlib) argv[ai++] = "-nostdlib";
    if (state->static_link) argv[ai++] = "-static";
    if (state->build_shared) argv[ai++] = "-shared";
    if (state->no_pie) argv[ai++] = "-no-pie";
    if (state->gc_sections) argv[ai++] = "--gc-sections";
    if (state->export_dynamic) argv[ai++] = "-E";
    if (state->entry_point[0]) { argv[ai++] = "-e"; argv[ai++] = state->entry_point; }
    for (int s = 0; s < state->linker_script_count; s++) {
        argv[ai++] = "-T";
        argv[ai++] = state->linker_scripts[s];
    }
    for (int l = 0; l < state->library_dir_count; l++) {
        char *buf = (char *)malloc(MAX_PATH_LEN + 4);
        if (buf) { snprintf(buf, MAX_PATH_LEN + 4, "-L%s", state->library_dirs[l]); argv[ai++] = buf; }
    }
    /* Prepend object files from explicit .o inputs */
    for (int i = 0; i < state->input_count; i++) {
        if (file_extension_matches(state->input_files[i], "o") ||
            file_extension_matches(state->input_files[i], "a")) {
            argv[ai++] = state->input_files[i];
        } else {
            char obj[MAX_PATH_LEN];
            gcc_determine_output_file(state, state->input_files[i], PHASE_ASSEMBLE, obj);
            argv[ai++] = strdup(obj);
        }
    }
    if (!state->nostdlib && !state->nodefaultlibs) {
        /* Standard KenuxK link order */
        argv[ai++] = "-lkapi";
        argv[ai++] = "-lc";
    }
    for (int l = 0; l < state->library_count; l++) {
        char *buf = (char *)malloc(MAX_PATH_LEN + 4);
        if (buf) { snprintf(buf, MAX_PATH_LEN + 4, "-l%s", state->libraries[l]); argv[ai++] = buf; }
    }
    argv[ai] = NULL;
    if (state->verbose) {
        fprintf(stderr, "gcc: link cmd:");
        for (int k = 0; argv[k]; k++) fprintf(stderr, " %s", argv[k]);
        fprintf(stderr, "\n");
    }
    int rc = gcc_run_command(argv);
    if (rc != 0) {
        fprintf(stderr, "gcc: error: linker failed with exit code %d\n", rc);
        state->error_count++;
        state->exit_code = rc ? rc : 1;
    }
    return state->exit_code;
}

void gcc_report_version(void) {
    printf("%s\n", GCC_VERSION_STR);
    printf("Copyright (C) 2024 KenuxK Project\n");
    printf("Target: %s\n", GCC_MACHINE_STR);
    printf("Configured with: --prefix=/usr --target=%s --enable-languages=c,c++\n", GCC_MACHINE_STR);
    printf("Thread model: posix\n");
    printf("Supported LTO compression algorithms: zlib\n");
}

void gcc_report_machine(void) {
    printf("%s\n", GCC_MACHINE_STR);
}

void gcc_report_search_dirs(const GccState *state) {
    printf("install: /usr/lib/gcc/%s/13/\n", GCC_MACHINE_STR);
    printf("programs: =/usr/libexec/gcc/%s/13/:/usr/libexec/gcc/%s/13/:/usr/libexec/gcc/%s/:"
           "/usr/lib/gcc/%s/13/:/usr/lib/gcc/%s/:/usr/lib/gcc/%s/13/../../../../%s/bin/\n",
           GCC_MACHINE_STR, GCC_MACHINE_STR, GCC_MACHINE_STR,
           GCC_MACHINE_STR, GCC_MACHINE_STR, GCC_MACHINE_STR, GCC_MACHINE_STR);
    printf("libraries: =/usr/lib/gcc/%s/13/:/usr/lib/gcc/%s/13/../../../../lib/:"
           "/lib/../lib/:/usr/lib/../lib/:/usr/lib/gcc/%s/13/../../../:/lib/:/usr/lib/\n",
           GCC_MACHINE_STR, GCC_MACHINE_STR, GCC_MACHINE_STR);
    printf("include (c): %s\n", state->include_dirs[0]);
}

void gcc_print_usage(void) {
    printf("Usage: gcc [options] file...\n");
    printf("Options:\n");
    printf("  -c                Compile and assemble, but do not link\n");
    printf("  -S                Compile only; do not assemble or link\n");
    printf("  -E                Preprocess only; do not compile, assemble or link\n");
    printf("  -o <file>         Place the output into <file>\n");
    printf("  -O0/-O1/-O2/-O3   Optimization level (default: -O2)\n");
    printf("  -Os               Optimize for size\n");
    printf("  -g/-g1/-g3        Generate debug information\n");
    printf("  -std=<standard>   Specify language standard (c99, c11, c17, gnu17, etc.)\n");
    printf("  -Wall             Enable common warning messages\n");
    printf("  -Wextra           Enable extra warning messages\n");
    printf("  -Werror           Treat warnings as errors\n");
    printf("  -I<dir>           Add <dir> to include search path\n");
    printf("  -L<dir>           Add <dir> to library search path\n");
    printf("  -l<lib>           Link against library <lib>\n");
    printf("  -D<macro>[=val]   Define a preprocessor macro\n");
    printf("  -U<macro>         Undefine a preprocessor macro\n");
    printf("  -m32/-m64         Generate code for 32-bit or 64-bit x86\n");
    printf("  -shared           Create a shared library\n");
    printf("  -static           Static linking\n");
    printf("  -fPIC/-fpic       Generate position-independent code\n");
    printf("  -fpie/-fPIE       Generate position-independent executable\n");
    printf("  -nostdlib         Do not use standard libraries or startup files\n");
    printf("  -ffreestanding    Compile for freestanding environment (kernel)\n");
    printf("  -T <script>       Use linker script\n");
    printf("  -Wl,<opts>        Pass options to linker\n");
    printf("  -flto             Enable link-time optimization\n");
    printf("  --version         Display compiler version\n");
    printf("  --help            Display this information\n");
}

void gcc_print_target_help(void) {
    printf("x86_64-kenuxk target specific options:\n");
    printf("  -m32                 Generate 32-bit i386 code\n");
    printf("  -m64                 Generate 64-bit x86-64 code (default)\n");
    printf("  -march=<cpu>         Specify target CPU architecture\n");
    printf("  -mtune=<cpu>         Schedule code for CPU\n");
    printf("  -msoft-float         Use software floating point\n");
    printf("  -mhard-float         Use hardware floating point\n");
    printf("  -mno-sse             Disable SSE instructions\n");
    printf("  -msse/-msse2/...     Enable SSE extensions\n");
    printf("  -mcmodel=small/medium/large/kernel  Code model\n");
    printf("  -mred-zone           Use the red zone (default for -mcmodel=small)\n");
    printf("  -mno-red-zone        Do not use the red zone (kernel code)\n");
}

static int gcc_main_internal(int argc, char **argv) {
    static GccState state;
    gcc_init(&state);
    gcc_parse_arguments(&state, argc, argv);

    if (state.dumpversion) { gcc_report_version(); return 0; }
    if (state.dumpmachine) { gcc_report_machine(); return 0; }
    if (state.dumpspecs)    { printf("%s\n", GCC_SPEC_STR); return 0; }
    if (state.print_search_dirs) { gcc_report_search_dirs(&state); return 0; }
    if (state.help)         { gcc_print_usage(); return 0; }
    if (state.target_help)  { gcc_print_target_help(); return 0; }

    if (state.input_count == 0) {
        fprintf(stderr, "gcc: fatal error: no input files\n");
        fprintf(stderr, "compilation terminated.\n");
        return 1;
    }
    if (gcc_validate_options(&state) < 0) return 1;
    gcc_resolve_tool_paths(&state);

    static CompileJob jobs[MAX_INPUT_FILES * 4];
    int njobs = 0;
    gcc_build_jobs(&state, jobs, &njobs);
    if (njobs > 0 && gcc_execute_jobs(&state, jobs, njobs) != 0) {
        return state.exit_code;
    }
    if (state.output_kind == OUTPUT_LINK) {
        gcc_do_link(&state, jobs, njobs);
    }
    if (state.time_report) {
        fprintf(stderr, "gcc: total input files: %d, compile jobs: %d, errors: %d\n",
                state.input_count, njobs, state.error_count);
    }
    return state.exit_code;
}

#ifndef KENUXK_NO_MAIN_GCC
int main(int argc, char **argv) {
    return gcc_main_internal(argc, argv);
}
#endif
