#include <shell.h>
#include <string.h>
#include <memory.h>
#include <fs.h>
#include <vga.h>
#include <process.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include "user_apps.h"

static shell_t shell;

void shell_init(void);
void shell_run(void);
int shell_parse_line(char* line, char** args, int* arg_count);
int shell_execute(char** args, int arg_count);
void shell_add_history(const char* line);
void shell_calculator(void);

static shell_t shell;

void shell_init(void)
{
    memset(&shell, 0, sizeof(shell));
    shell.history_count = 0;
    shell.history_index = 0;
}

void shell_run(void)
{
    vga_print("Kenux Shell v1.0\n");
    vga_print("Type 'help' for commands\n\n");
    
    while (1) {
        char line[256];
        char cwd[256];
        getcwd(cwd, sizeof(cwd));
        vga_print(cwd);
        vga_print(" $ ");
        vga_gets(line, sizeof(line));
        
        if (strlen(line) > 0) {
            shell_add_history(line);
            char* args[16];
            int arg_count;
            shell_parse_line(line, args, &arg_count);
            shell_execute(args, arg_count);
        }
    }
}

int shell_parse_line(char* line, char** args, int* arg_count)
{
    *arg_count = 0;
    
    char* token = strtok(line, " \t\n");
    while (token && *arg_count < 16) {
        args[(*arg_count)++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    return *arg_count;
}

void shell_execute_builtin(char** args, int arg_count)
{
    if (strcmp(args[0], "help") == 0) {
        vga_print("Available commands:\n");
        vga_print("  help       - Show this help\n");
        vga_print("  clear      - Clear screen\n");
        vga_print("  echo       - Print message\n");
        vga_print("  ls         - List files\n");
        vga_print("  cat        - Display file contents\n");
        vga_print("  cd         - Change directory\n");
        vga_print("  pwd        - Print working directory\n");
        vga_print("  mkdir      - Create directory\n");
        vga_print("  rm         - Remove file or directory\n");
        vga_print("  cp         - Copy file\n");
        vga_print("  mv         - Move file\n");
        vga_print("  touch      - Create empty file\n");
        vga_print("  chmod      - Change file permissions\n");
        vga_print("  ps         - List processes\n");
        vga_print("  kill       - Terminate process\n");
        vga_print("  history    - Show command history\n");
        vga_print("  sysinfo    - Show system info\n");
        vga_print("  meminfo    - Show memory info\n");
        vga_print("  date       - Show date and time\n");
        vga_print("  calculator - Simple calculator\n");
        vga_print("  editor     - Text editor\n");
        vga_print("  desktop    - Start desktop environment\n");
        vga_print("  exit       - Exit shell\n");
    } else if (strcmp(args[0], "clear") == 0) {
        vga_clear();
    } else if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; i < arg_count; i++) {
            vga_print(args[i]);
            if (i < arg_count - 1) {
                vga_print(" ");
            }
        }
        vga_print("\n");
    } else if (strcmp(args[0], "ls") == 0) {
        fs_list_directory("/");
    } else if (strcmp(args[0], "cat") == 0 && arg_count > 1) {
        fs_read_file(args[1]);
    } else if (strcmp(args[0], "cd") == 0 && arg_count > 1) {
        fs_change_directory(args[1]);
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[256];
        getcwd(cwd, sizeof(cwd));
        vga_print(cwd);
        vga_print("\n");
    } else if (strcmp(args[0], "mkdir") == 0 && arg_count > 1) {
        fs_create_directory(args[1]);
    } else if (strcmp(args[0], "rm") == 0 && arg_count > 1) {
        fs_remove_file(args[1]);
    } else if (strcmp(args[0], "cp") == 0 && arg_count > 2) {
        fs_copy_file(args[1], args[2]);
    } else if (strcmp(args[0], "mv") == 0 && arg_count > 2) {
        fs_move_file(args[1], args[2]);
    } else if (strcmp(args[0], "touch") == 0 && arg_count > 1) {
        fs_create_file(args[1]);
    } else if (strcmp(args[0], "chmod") == 0 && arg_count > 2) {
        fs_change_permissions(args[1], atoi(args[2]));
    } else if (strcmp(args[0], "ps") == 0) {
        process_list_all();
    } else if (strcmp(args[0], "kill") == 0 && arg_count > 1) {
        process_kill(atoi(args[1]));
    } else if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < shell.history_count; i++) {
            char buf[256];
            sprintf(buf, "  %d: %s\n", i + 1, shell.history[i]);
            vga_print(buf);
        }
    } else if (strcmp(args[0], "sysinfo") == 0) {
        vga_print("KenuxOS System Information\n");
        vga_print("  Kernel: x86_64\n");
        vga_print("  Version: 1.0\n");
        vga_print("  CPU: Virtual CPU\n");
    } else if (strcmp(args[0], "meminfo") == 0) {
        vga_print("Memory Information\n");
        vga_print("  Total: 64 MB\n");
        vga_print("  Free: 32 MB\n");
        vga_print("  Used: 32 MB\n");
    } else if (strcmp(args[0], "date") == 0) {
        vga_print("Date: 2026-03-08\n");
        vga_print("Time: 12:00:00\n");
    } else if (strcmp(args[0], "calculator") == 0) {
        shell_calculator();
    } else if (strcmp(args[0], "editor") == 0) {
        text_editor_run();
    } else if (strcmp(args[0], "desktop") == 0) {
        desktop_run();
    } else if (strcmp(args[0], "exit") == 0) {
        syscall_halt();
    } else {
        vga_print("Command not found: ");
        vga_print(args[0]);
        vga_print("\n");
    }
}

int shell_execute(char** args, int arg_count)
{
    if (arg_count == 0) {
        return 0;
    }
    
    shell_execute_builtin(args, arg_count);
    return 0;
}

void shell_add_history(const char* line)
{
    if (shell.history_count < 100) {
        strcpy(shell.history[shell.history_count++], line);
    }
}

void shell_calculator(void)
{
    vga_print("Simple Calculator\n");
    vga_print("Enter expression (e.g., 2+2)\n");
    vga_print("Type 'exit' to quit\n\n");
    
    while (1) {
        char line[64];
        vga_print("calc> ");
        vga_gets(line, sizeof(line));
        
        if (strcmp(line, "exit") == 0) {
            break;
        }
        
        int a = 0, b = 0;
        char op = '+';
        
        int i = 0;
        while (line[i] && line[i] >= '0' && line[i] <= '9') {
            a = a * 10 + (line[i] - '0');
            i++;
        }
        
        if (line[i]) {
            op = line[i];
            i++;
        }
        
        while (line[i] && line[i] >= '0' && line[i] <= '9') {
            b = b * 10 + (line[i] - '0');
            i++;
        }
        
        int result = 0;
        if (op == '+') {
            result = a + b;
        } else if (op == '-') {
            result = a - b;
        } else if (op == '*') {
            result = a * b;
        } else if (op == '/') {
            if (b != 0) {
                result = a / b;
            }
        }
        
        char buf[64];
        sprintf(buf, "%d %c %d = %d\n", a, op, b, result);
        vga_print(buf);
    }
}
