#ifndef BIN_SHELL_COMMANDS_H
#define BIN_SHELL_COMMANDS_H

#include <arch/types.h>

void shell_init(void);
void shell_run(void);
int shell_parse_line(char* line, char** args, int* arg_count);
void shell_execute(char** args, int arg_count);
void shell_add_history(const char* line);
void shell_print_history(void);
void shell_calculator(void);

#endif
