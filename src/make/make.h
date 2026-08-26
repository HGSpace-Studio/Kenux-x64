/*
 * Kenux OS - Make Utility Implementation
 * Header file for make functionality
 */

#ifndef _MAKE_H
#define _MAKE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
/* Windows MinGW compatibility */
#include <io.h>
#include <process.h>
#include <direct.h>

#define WIFEXITED(s)    (1)
#define WEXITSTATUS(s)  (0)
#define WIFSIGNALED(s)  (0)
#define WTERMSIG(s)      (0)
#define WIFSTOPPED(s)    (0)
#define WSTOPSIG(s)      (0)

static inline int kenux_fork(void) { return -1; }
static inline int kenux_wait(int *s) { if (s) *s = 0; return -1; }
static inline int kenux_waitpid(int p, int *s, int o) { (void)p; (void)o; if (s) *s = 0; return -1; }
static inline int kenux_execvp(const char *f, char *const a[]) { (void)f; (void)a; return -1; }
static inline int kenux_chdir(const char *p) { return _chdir(p); }
static inline int kenux_access(const char *p, int m) { return _access(p, m); }

#define fork    kenux_fork
#define wait    kenux_wait
#define waitpid kenux_waitpid
#define execvp  kenux_execvp
#define chdir   kenux_chdir
#define access  kenux_access

#define X_OK 0
#define W_OK 2
#define R_OK 4

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* S_ISREG/S_ISDIR on Windows _stat */
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#endif

// Maximum line length in Makefile
#define MAX_LINE_LEN 4096
#define MAX_TARGETS 100
#define MAX_RULES 200
#define MAX_DEPENDENCIES 50
#define MAX_COMMANDS 10
#define MAX_VAR_NAME 100
#define MAX_VAR_VALUE 4096

// Target structure
typedef struct {
    char name[MAX_LINE_LEN];
    char dependencies[MAX_DEPENDENCIES][MAX_LINE_LEN];
    int num_dependencies;
    char commands[MAX_COMMANDS][MAX_LINE_LEN];
    int num_commands;
    time_t timestamp;
    int built;
} Target;

// Variable structure
typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
} Variable;

// Make state
typedef struct {
    Target targets[MAX_RULES];
    int num_targets;
    Variable variables[MAX_VAR_NAME];
    int num_variables;
    char makefile_path[MAX_LINE_LEN];
    int verbose;
    int dry_run;
} MakeState;

// Function prototypes
void make_init(MakeState *state);
int load_makefile(MakeState *state, const char *filename);
int parse_makefile(MakeState *state, FILE *file);
int parse_line(MakeState *state, char *line);
int parse_target_line(MakeState *state, char *line);
int parse_variable_line(MakeState *state, char *line);
int parse_command_line(MakeState *state, char *line, int target_idx);
void expand_variables(MakeState *state, char *str);
const char *get_variable(MakeState *state, const char *name);
void set_variable(MakeState *state, const char *name, const char *value);
int target_exists(MakeState *state, const char *name);
int find_target_index(MakeState *state, const char *name);
int build_target(MakeState *state, int target_idx, int dependency_depth);
int execute_commands(MakeState *state, int target_idx);
int execute_command(MakeState *state, const char *command);
int is_file_uptodate(MakeState *state, int target_idx);
time_t get_file_timestamp(const char *filename);
void print_target_info(MakeState *state, int target_idx);
void print_help(void);

#endif /* _MAKE_H */