/*
 * Kenux OS - Bash Shell Implementation
 * Header file for shell functionality
 */

#ifndef _SHELL_H
#define _SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
/* Windows MinGW compatibility */
#include <io.h>
#include <process.h>
#include <direct.h>

#ifndef uid_t
typedef int uid_t;
#endif
#ifndef gid_t
typedef int gid_t;
#endif
/* pid_t is provided by MinGW <sys/types.h> via <sys/stat.h>; do not redefine. */

#define WIFEXITED(s)    (1)
#define WEXITSTATUS(s)  (0)
#define WIFSIGNALED(s)  (0)
#define WTERMSIG(s)      (0)
#define WIFSTOPPED(s)    (0)
#define WSTOPSIG(s)      (0)

static inline pid_t kenux_fork(void) { return -1; }
static inline int kenux_wait(int *s) { if (s) *s = 0; return -1; }
static inline pid_t kenux_waitpid(pid_t p, int *s, int o) { (void)p; (void)o; if (s) *s = 0; return -1; }
static inline int kenux_execvp(const char *f, char *const a[]) { (void)f; (void)a; return -1; }
static inline int kenux_chdir(const char *p) { return _chdir(p); }
static inline int kenux_access(const char *p, int m) { return _access(p, m); }

#define fork    kenux_fork
#define wait    kenux_wait
#define waitpid kenux_waitpid
#define execvp  kenux_execvp
#define chdir   kenux_chdir
#define access  kenux_access

#ifndef X_OK
#define X_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* POSIX signals missing on Windows */
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGTSTP
#define SIGTSTP 20
#endif

/* ssize_t / getline stubs for Windows */
#ifndef _SSIZE_T_DEFINED
typedef long ssize_t;
#define _SSIZE_T_DEFINED
#endif
static inline ssize_t kenux_getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    size_t cap = *n ? *n : 128;
    char *buf = *lineptr;
    if (!buf || cap < 128) {
        buf = (char *)realloc(*lineptr, cap);
        if (!buf) return -1;
        *lineptr = buf; *n = cap;
    }
    size_t len = 0; int c;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(*lineptr, cap);
            if (!nb) return -1;
            *lineptr = nb; *n = cap; buf = *lineptr;
        }
        buf[len++] = (char)c;
        if (c == '\n') break;
    }
    if (len == 0 && c == EOF) return -1;
    buf[len] = '\0';
    *lineptr = buf; *n = cap;
    return (ssize_t)len;
}
#define getline kenux_getline
#endif

// Maximum command line length
#define MAX_CMD_LEN 4096
#define MAX_ARGS 64
#define MAX_PATH_LEN 1024

// Command structure
typedef struct {
    char command[MAX_CMD_LEN];
    char *args[MAX_ARGS];
    int argc;
    char input_file[MAX_PATH_LEN];
    char output_file[MAX_PATH_LEN];
    int append_output;
    int background;
} Command;

// Shell state
typedef struct {
    char current_dir[MAX_PATH_LEN];
    char previous_dir[MAX_PATH_LEN];
    int running;
    int interactive;
} ShellState;

// Function prototypes
void shell_init(ShellState *state, int interactive);
void shell_loop(ShellState *state);
void parse_command(char *input, Command *cmd);
void execute_command(Command *cmd, ShellState *state);
int builtin_cd(char **args, int argc, ShellState *state);
int builtin_pwd(char **args, int argc, ShellState *state);
int builtin_echo(char **args, int argc, ShellState *state);
int builtin_exit(char **args, int argc, ShellState *state);
int builtin_help(char **args, int argc, ShellState *state);
int builtin_export(char **args, int argc, ShellState *state);
char *find_executable(const char *name, char *path);
void print_prompt(ShellState *state);

#endif /* _SHELL_H */