

#include <arch/types.h>
#include <arch/fs.h>
#include <arch/memory.h>
#include <arch/vga.h>
#include <arch/keyboard.h>
#include <arch/elf.h>
#include <arch/process.h>
#include <arch/usermode.h>
#include <string.h>
#include <stdio.h>
#include <kapi.h>
#include <slab.h>
#include <gui.h>

static int isalnum_c(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

extern int shell_execute(const char* cmd, int argc, char* argv[]);

/* Read a line from keyboard, returns 1 on success, 0 on EOF/error */
int kapi_shell_read_line(char* buf, uint64_t size)
{
    if (!buf || size == 0) return 0;
    uint64_t i = 0;
    buf[0] = '\0';

    while (i < size - 1) {
        key_event_t ev = keyboard_read();
        if (!ev.pressed) continue;

        char c = ev.ascii;

        /* Enter key - end of line */
        if (c == '\n' || c == '\r' || ev.keycode == KEY_ENTER) {
            buf[i] = '\0';
            vga_putc('\n');
            return 1;
        }

        /* Backspace */
        if (c == '\b' || ev.keycode == KEY_BACKSPACE) {
            if (i > 0) {
                i--;
                vga_putc('\b');
                vga_putc(' ');
                vga_putc('\b');
            }
            continue;
        }

        /* Printable characters */
        if (c >= 32 && c < 127) {
            buf[i++] = c;
            vga_putc(c);
        }
    }

    buf[i] = '\0';
    vga_putc('\n');
    return 1;
}

#define SHELL_MAX_ARGS      32
#define SHELL_MAX_PIPES     8
#define SHELL_MAX_ENVS      64
#define SHELL_ENV_NAME_MAX  64
#define SHELL_ENV_VAL_MAX   256
#define SHELL_HISTORY_MAX   16

typedef struct {
    char name[SHELL_ENV_NAME_MAX];
    char value[SHELL_ENV_VAL_MAX];
} env_var_t;

static env_var_t env_vars[SHELL_MAX_ENVS];
static uint32_t env_count = 0;

static char* history[SHELL_HISTORY_MAX];
static uint32_t history_count = 0;
static uint32_t history_index = 0;

static const char* shell_getenv(const char* name)
{
    for (uint32_t i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

static void shell_setenv(const char* name, const char* value)
{
    for (uint32_t i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            strncpy(env_vars[i].value, value, SHELL_ENV_VAL_MAX - 1);
            env_vars[i].value[SHELL_ENV_VAL_MAX - 1] = '\0';
            return;
        }
    }
    if (env_count < SHELL_MAX_ENVS) {
        strncpy(env_vars[env_count].name, name, SHELL_ENV_NAME_MAX - 1);
        env_vars[env_count].name[SHELL_ENV_NAME_MAX - 1] = '\0';
        strncpy(env_vars[env_count].value, value, SHELL_ENV_VAL_MAX - 1);
        env_vars[env_count].value[SHELL_ENV_VAL_MAX - 1] = '\0';
        env_count++;
    }
}

static void shell_expand_vars(char* dst, const char* src, uint64_t max)
{
    uint64_t di = 0;
    while (*src && di < max - 1) {
        if (*src == '$') {
            src++;
            char name[SHELL_ENV_NAME_MAX];
            uint32_t ni = 0;
            while (*src && (isalnum_c(*src) || *src == '_') && ni < SHELL_ENV_NAME_MAX - 1) {
                name[ni++] = *src++;
            }
            name[ni] = '\0';
            const char* val = shell_getenv(name);
            if (val) {
                while (*val && di < max - 1) {
                    dst[di++] = *val++;
                }
            }
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

static void shell_unquote(char* dst, const char* src, uint64_t max)
{
    uint64_t di = 0;
    char quote = 0;
    while (*src && di < max - 1) {
        if (!quote && (*src == '"' || *src == '\'')) {
            quote = *src;
            src++;
            continue;
        }
        if (quote && *src == quote) {
            quote = 0;
            src++;
            continue;
        }
        dst[di++] = *src++;
    }
    dst[di] = '\0';
}

typedef struct {
    char* argv[SHELL_MAX_ARGS];
    int argc;
    char* input_file;
    char* output_file;
    int append_output;
    int background;
} shell_cmd_t;

static int shell_parse_line(const char* line, shell_cmd_t* cmds, int max_cmds)
{
    int cmd_count = 0;
    char* tokens[128];
    int tok_count = 0;

    char expanded[512];
    shell_expand_vars(expanded, line, sizeof(expanded));

    char* p = expanded;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char token[256];
        uint32_t ti = 0;
        char quote = 0;

        while (*p && (quote || (*p != ' ' && *p != '\t'))) {
            if (*p == '"' || *p == '\'') {
                if (!quote) quote = *p;
                else if (quote == *p) quote = 0;
                else token[ti++] = *p;
            } else {
                token[ti++] = *p;
            }
            p++;
            if (ti >= 255) break;
        }
        token[ti] = '\0';

        if (ti > 0) {
            tokens[tok_count] = (char*)kmalloc(ti + 1);
            strcpy(tokens[tok_count], token);
            tok_count++;
        }
    }

    memset(cmds, 0, sizeof(shell_cmd_t) * max_cmds);
    int curr = 0;
    int arg = 0;

    for (int i = 0; i < tok_count; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            cmds[curr].argc = arg;
            curr++;
            arg = 0;
            if (curr >= max_cmds) break;
        } else if (strcmp(tokens[i], ">") == 0 && i + 1 < tok_count) {
            cmds[curr].output_file = tokens[++i];
            cmds[curr].append_output = 0;
        } else if (strcmp(tokens[i], ">>") == 0 && i + 1 < tok_count) {
            cmds[curr].output_file = tokens[++i];
            cmds[curr].append_output = 1;
        } else if (strcmp(tokens[i], "<") == 0 && i + 1 < tok_count) {
            cmds[curr].input_file = tokens[++i];
        } else if (strcmp(tokens[i], "&") == 0) {
            cmds[curr].background = 1;
        } else {
            if (arg < SHELL_MAX_ARGS - 1) {
                cmds[curr].argv[arg++] = tokens[i];
            }
        }
    }
    cmds[curr].argc = arg;
    cmd_count = curr + 1;

    return cmd_count;
}

static int shell_create_pipe(int pipefd[2])
{

    static int pipe_counter = 0;
    char path[64];
    snprintf(path, sizeof(path), "/tmp/pipe_%d", pipe_counter++);

    vfs_node_t* node = vfs_create_node(path, FS_TYPE_FIFO);
    if (!node) return -1;

    (void)node;
    pipefd[0] = -1;
    pipefd[1] = -1;
    return 0;
}

static int shell_exec_pipeline(shell_cmd_t* cmds, int cmd_count)
{
    if (cmd_count == 0) return -1;
    if (cmd_count == 1) {
        if (cmds[0].argc == 0) return 0;

        /* export VAR=VALUE */
        if (strcmp(cmds[0].argv[0], "export") == 0 && cmds[0].argc >= 2) {
            char* eq = strchr(cmds[0].argv[1], '=');
            if (eq) {
                *eq = '\0';
                shell_setenv(cmds[0].argv[1], eq + 1);
            }
            return 0;
        }

        return shell_execute(cmds[0].argv[0], cmds[0].argc, cmds[0].argv);
    }

    for (int i = 0; i < cmd_count; i++) {
        shell_execute(cmds[i].argv[0], cmds[i].argc, cmds[i].argv);
    }
    return 0;
}

void shell_enhanced_run(void);

typedef struct {
    elf_load_info_t info;
    int argc;
    char args[16][128];
} shell_elf_task_t;

static void shell_elf_entry(void* arg)
{
    shell_elf_task_t* task = (shell_elf_task_t*)arg;
    const char* argv[17];
    const char* envp[2] = { "PATH=/bin:/usr/bin", NULL };
    for (int i = 0; i < task->argc; i++) argv[i] = task->args[i];
    argv[task->argc] = NULL;
    uint64_t rsp = elf_setup_stack(task->info.stack_top, argv, envp, &task->info);
    usermode_switch_to_user((void*)task->info.entry, (void*)rsp);
    process_exit(0);
}

static int shell_run_elf(const char* path, int argc, char* argv[])
{
    shell_elf_task_t* task = (shell_elf_task_t*)kmalloc(sizeof(shell_elf_task_t));
    if (!task) return -1;
    memset(task, 0, sizeof(*task));
    if (elf_load_from_file(path, &task->info) != 0) {
        kfree(task);
        return -1;
    }
    task->argc = argc > 16 ? 16 : argc;
    for (int i = 0; i < task->argc; i++) {
        strncpy(task->args[i], argv[i], sizeof(task->args[i]) - 1);
        task->args[i][sizeof(task->args[i]) - 1] = '\0';
    }
    uint64_t pid = process_create_ex(path, shell_elf_entry, task,
                                     PRIORITY_NORMAL, PROC_FLAG_USER,
                                     process_current_id());
    if (pid == (uint64_t)-1) {
        kfree(task);
        return -1;
    }
    process_t* proc = process_get(pid);
    if (proc) proc->context.cr3 = (uint64_t)task->info.pml4;
    return (int)pid;
}

static int shell_find_elf(const char* cmd, char* out, uint64_t size)
{
    const char* roots[] = { "/bin/", "/usr/bin/", "/System/bin/" };
    if (!cmd || !out || size == 0) return 0;
    if (cmd[0] == '/') {
        strncpy(out, cmd, size - 1);
        out[size - 1] = '\0';
        return vfs_open(out, FS_O_RDONLY, 0) >= 0;
    }
    for (uint32_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        snprintf(out, size, "%s%s", roots[i], cmd);
        int fd = vfs_open(out, FS_O_RDONLY, 0);
        if (fd >= 0) {
            vfs_close(fd);
            return 1;
        }
    }
    return 0;
}

/* Built-in shell commands */
static int shell_builtin_help(int argc, char* argv[]);
static int shell_builtin_echo(int argc, char* argv[]);
static int shell_builtin_clear(int argc, char* argv[]);
static int shell_builtin_mem(int argc, char* argv[]);
static int shell_builtin_pci(int argc, char* argv[]);
static int shell_builtin_acpi(int argc, char* argv[]);
static int shell_builtin_date(int argc, char* argv[]);
static int shell_builtin_uname(int argc, char* argv[]);
static int shell_builtin_env(int argc, char* argv[]);
static int shell_builtin_systemctl(int argc, char* argv[]);
static int shell_builtin_desktop(int argc, char* argv[]);
static int shell_builtin_run(int argc, char* argv[]);

typedef struct {
    const char* name;
    const char* help;
    int (*handler)(int argc, char* argv[]);
} shell_builtin_t;

static shell_builtin_t shell_builtins[] = {
    { "help",      "Show this help",              shell_builtin_help },
    { "echo",      "Print text",                  shell_builtin_echo },
    { "desktop",   "Start graphical desktop",     shell_builtin_desktop },
    { "run",       "Run an ELF program",          shell_builtin_run },
    { "clear",     "Clear screen",                shell_builtin_clear },
    { "mem",       "Show memory info",            shell_builtin_mem },
    { "pci",       "List PCI devices",            shell_builtin_pci },
    { "acpi",      "Show ACPI info",              shell_builtin_acpi },
    { "date",      "Show current date/time",      shell_builtin_date },
    { "uname",     "Show system info",            shell_builtin_uname },
    { "env",       "Show environment variables",  shell_builtin_env },
    { "systemctl", "Systemd service control",     shell_builtin_systemctl },
    { NULL, NULL, NULL }
};

static int shell_builtin_help(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Kenux Shell - Available commands:\n");
    vga_print("========================================\n");
    for (int i = 0; shell_builtins[i].name; i++) {
        vga_print(shell_builtins[i].name);
        vga_print("  - ");
        vga_print(shell_builtins[i].help);
        vga_putc('\n');
    }
    vga_print("========================================\n");
    return 0;
}

static int shell_builtin_echo(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        vga_print(argv[i]);
        if (i < argc - 1) vga_putc(' ');
    }
    vga_putc('\n');
    return 0;
}

static int shell_builtin_clear(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_clear();
    return 0;
}

static int shell_builtin_mem(int argc, char* argv[])
{
    (void)argc; (void)argv;
    extern uint64_t memory_get_total(void);
    extern uint64_t memory_get_free(void);
    char buf[64];
    sprintf(buf, "Memory total: %lu KB\n", memory_get_total() / 1024);
    vga_print(buf);
    sprintf(buf, "Memory free:  %lu KB\n", memory_get_free() / 1024);
    vga_print(buf);
    return 0;
}

static int shell_builtin_pci(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("PCI devices listed (see boot log)\n");
    return 0;
}

static int shell_builtin_acpi(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("ACPI tables detected at boot\n");
    return 0;
}

static int shell_builtin_date(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Date command - RTC based\n");
    return 0;
}

static int shell_builtin_uname(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Kenux 26.7.9K x86_64\n");
    return 0;
}

static int shell_builtin_env(int argc, char* argv[])
{
    (void)argc; (void)argv;
    for (uint32_t i = 0; i < env_count; i++) {
        char vbuf[320];
        snprintf(vbuf, sizeof(vbuf), "%s=%s\n", env_vars[i].name, env_vars[i].value);
        vga_print(vbuf);
    }
    return 0;
}

static int shell_builtin_systemctl(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: systemctl <list|status|start|stop> [service]\n");
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) {
        vga_print("systemd units (static list):\n");
        vga_print("  multi-user.target - loaded\n");
        vga_print("  basic.target      - loaded\n");
        vga_print("  default.target    - loaded\n");
        return 0;
    }
    vga_print("systemctl: command acknowledged\n");
    return 0;
}

static int shell_builtin_desktop(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Starting graphical desktop (ESC to exit)...\n");
    gui_run();
    vga_clear();
    vga_print("Returned to text mode shell.\n");
    return 0;
}

static int shell_builtin_run(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: run <elf> [args...]\n");
        return -1;
    }
    char path[256];
    if (!shell_find_elf(argv[1], path, sizeof(path))) {
        vga_print("run: ELF file not found\n");
        return -1;
    }
    int pid = shell_run_elf(path, argc - 1, &argv[1]);
    if (pid < 0) {
        vga_print("run: ELF load failed\n");
        return -1;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Started %s, pid=%d\n", path, pid);
    vga_print(buf);
    return 0;
}

int shell_execute(const char* cmd, int argc, char* argv[])
{
    if (!cmd) return -1;
    for (int i = 0; shell_builtins[i].name; i++) {
        if (strcmp(cmd, shell_builtins[i].name) == 0) {
            return shell_builtins[i].handler(argc, argv);
        }
    }
    char path[256];
    if (shell_find_elf(cmd, path, sizeof(path))) {
        int pid = shell_run_elf(path, argc, argv);
        if (pid >= 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Started %s, pid=%d\n", path, pid);
            vga_print(buf);
            return 0;
        }
    }
    vga_print(cmd);
    vga_print(": command not found (try 'help')\n");
    return -1;
}

void shell_init(void)
{
}

void shell_run(void)
{
    shell_enhanced_run();
}

void shell_enhanced_run(void)
{

    shell_setenv("PATH", "/bin:/usr/bin");
    shell_setenv("HOME", "/root");
    shell_setenv("USER", "root");
    shell_setenv("SHELL", "/bin/sh");
    shell_setenv("TERM", "linux");

    char line[512];
    shell_cmd_t cmds[SHELL_MAX_PIPES];

    for (;;) {
        vga_puts("$ ");
        if (!kapi_shell_read_line(line, sizeof(line))) break;

        if (history_count < SHELL_HISTORY_MAX) {
            history[history_count] = (char*)kmalloc(strlen(line) + 1);
            if (history[history_count]) {
                strcpy(history[history_count], line);
                history_count++;
            }
        }

        int cmd_count = shell_parse_line(line, cmds, SHELL_MAX_PIPES);
        if (cmd_count > 0) {
            shell_exec_pipeline(cmds, cmd_count);
        }

        for (int i = 0; i < cmd_count; i++) {
            for (int j = 0; j < cmds[i].argc; j++) {
                if (cmds[i].argv[j]) kfree(cmds[i].argv[j]);
            }
        }
    }
}
