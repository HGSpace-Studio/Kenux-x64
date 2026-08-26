#ifndef ARCH_X86_64_SHELL_H
#define ARCH_X86_64_SHELL_H

#include <arch/types.h>

#define SHELL_MAX_LINE 256
#define SHELL_MAX_ARGS 16
#define SHELL_MAX_HISTORY 32

typedef struct {
    char line[SHELL_MAX_LINE];
    char* args[SHELL_MAX_ARGS];
    int arg_count;
    char history[SHELL_MAX_HISTORY][SHELL_MAX_LINE];
    int history_count;
    int history_index;
} shell_t;

void shell_init(void);
void shell_run(void);
int shell_parse_line(char* line, char** args, int* arg_count);
int shell_execute(char** args, int arg_count);
void shell_add_history(const char* line);
void shell_print_history(void);

#endif
