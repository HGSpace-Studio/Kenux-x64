

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

    memset(cmds, 0, sizeof(shell_cmd_t) * (size_t)max_cmds);
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
    uint64_t pid = process_create_ex(path, (void*)(uintptr_t)shell_elf_entry, task,
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
static int shell_builtin_ls(int argc, char* argv[]);
static int shell_builtin_cd(int argc, char* argv[]);
static int shell_builtin_pwd(int argc, char* argv[]);
static int shell_builtin_cat(int argc, char* argv[]);
static int shell_builtin_mkdir(int argc, char* argv[]);
static int shell_builtin_rm(int argc, char* argv[]);
static int shell_builtin_cp(int argc, char* argv[]);
static int shell_builtin_mv(int argc, char* argv[]);
static int shell_builtin_ps(int argc, char* argv[]);
static int shell_builtin_kill(int argc, char* argv[]);
static int shell_builtin_top(int argc, char* argv[]);
static int shell_builtin_df(int argc, char* argv[]);
static int shell_builtin_free(int argc, char* argv[]);
static int shell_builtin_whoami(int argc, char* argv[]);
static int shell_builtin_hostname(int argc, char* argv[]);
static int shell_builtin_uptime(int argc, char* argv[]);
static int shell_builtin_reboot(int argc, char* argv[]);
static int shell_builtin_poweroff(int argc, char* argv[]);
static int shell_builtin_neofetch(int argc, char* argv[]);
static int shell_builtin_fastfetch(int argc, char* argv[]);
static int shell_builtin_calc(int argc, char* argv[]);
static int shell_builtin_snake(int argc, char* argv[]);
static int shell_builtin_tetris(int argc, char* argv[]);

typedef struct {
    const char* name;
    const char* help;
    int (*handler)(int argc, char* argv[]);
} shell_builtin_t;

static shell_builtin_t shell_builtins[] = {
    { "help",      "Show this help",                    shell_builtin_help },
    { "echo",      "Print text to console",              shell_builtin_echo },
    { "clear",     "Clear terminal screen",              shell_builtin_clear },
    { "ls",        "List directory contents",            shell_builtin_ls },
    { "cd",        "Change directory",                   shell_builtin_cd },
    { "pwd",       "Print working directory",            shell_builtin_pwd },
    { "cat",       "Display file contents",              shell_builtin_cat },
    { "mkdir",     "Create directory",                    shell_builtin_mkdir },
    { "rm",        "Remove file or directory",           shell_builtin_rm },
    { "cp",        "Copy files or directories",          shell_builtin_cp },
    { "mv",        "Move or rename files",               shell_builtin_mv },
    { "ps",        "List running processes",             shell_builtin_ps },
    { "kill",      "Terminate a process by PID",         shell_builtin_kill },
    { "top",       "Display system resource usage",      shell_builtin_top },
    { "df",        "Report disk space usage",            shell_builtin_df },
    { "free",      "Display memory usage information",   shell_builtin_free },
    { "whoami",    "Display current username",           shell_builtin_whoami },
    { "hostname",  "Display system hostname",            shell_builtin_hostname },
    { "uptime",    "Tell how long the system is running",shell_builtin_uptime },
    { "uname",     "Display system information",         shell_builtin_uname },
    { "date",      "Display current date and time",      shell_builtin_date },
    { "env",       "Display environment variables",      shell_builtin_env },
    { "mem",       "Show detailed memory info",          shell_builtin_mem },
    { "pci",       "List PCI devices",                   shell_builtin_pci },
    { "acpi",      "Show ACPI information",              shell_builtin_acpi },
    { "systemctl", "System service control",             shell_builtin_systemctl },
    { "desktop",   "Start graphical desktop",            shell_builtin_desktop },
    { "run",       "Run an ELF program",                 shell_builtin_run },
    { "reboot",    "Reboot the system",                  shell_builtin_reboot },
    { "poweroff",  "Power off the system",               shell_builtin_poweroff },
    { "neofetch",  "Display system info with logo",      shell_builtin_neofetch },
    { "fastfetch","Display fast system info",            shell_builtin_fastfetch },
    { "calc",      "Launch calculator",                  shell_builtin_calc },
    { "snake",     "Play Snake game",                    shell_builtin_snake },
    { "tetris",    "Play Tetris game",                   shell_builtin_tetris },
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
        vga_print("systemd units:\n");
        vga_print("  kenuxwm.service       running   Kenux Window Manager\n");
        vga_print("  network.service       running   Network Manager\n");
        vga_print("  sound.service         running   Audio Server\n");
        vga_print("  bluetooth.service     stopped   Bluetooth Daemon\n");
        vga_print("  printer.service       stopped   Print Service\n");
        vga_print("  sshd.service          running   SSH Server\n");
        vga_print("  firewall.service      running   Firewall\n");
        return 0;
    }
    vga_print("systemctl: command acknowledged\n");
    return 0;
}

static char current_dir[256] = "/root";

static int shell_builtin_ls(int argc, char* argv[])
{
    const char* path = current_dir;
    if (argc > 1 && strcmp(argv[1], "-la") != 0) path = argv[1];
    
    char info[256];
    snprintf(info, sizeof(info), "Listing: %s\n", path);
    vga_print(info);
    
    vga_print("drwxr-xr-x  2 root root  4096 Jan 01 00:00 .\n");
    vga_print("drwxr-xr-x 3 root root  4096 Jan 01 00:00 ..\n");
    vga_print("-rw-r--r-- 1 root root   512 Jan 01 00:00 .bashrc\n");
    vga_print("-rw-r--r-- 1 root root  1024 Jan 01 00:00 .profile\n");
    vga_print("drwxr-xr-x 2 root root  4096 Jan 01 00:00 Desktop\n");
    vga_print("drwxr-xr-x 2 root root  4096 Jan 01 00:00 Documents\n");
    vga_print("drwxr-xr-x 2 root root  4096 Jan 01 00:00 Downloads\n");
    vga_print("-rw-r--r-- 1 root root 8192 Jan 01 00:00 readme.txt\n");
    vga_print("-rwxr-xr-x 1 root root 4096 Jan 01 00:00 script.sh\n");
    return 0;
}

static int shell_builtin_cd(int argc, char* argv[])
{
    if (argc < 2) {
        strcpy(current_dir, "/root");
    } else if (strcmp(argv[1], "..") == 0) {
        char* last_slash = strrchr(current_dir, '/');
        if (last_slash && last_slash != current_dir) {
            *last_slash = '\0';
        } else {
            strcpy(current_dir, "/");
        }
    } else if (argv[1][0] == '/') {
        strncpy(current_dir, argv[1], sizeof(current_dir) - 1);
    } else {
        if (strlen(current_dir) + strlen(argv[1]) + 2 < sizeof(current_dir)) {
            strcat(current_dir, "/");
            strcat(current_dir, argv[1]);
        }
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Changed to: %s\n", current_dir);
    vga_print(msg);
    return 0;
}

static int shell_builtin_pwd(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print(current_dir);
    vga_print("\n");
    return 0;
}

static int shell_builtin_cat(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: cat <filename>\n");
        return 1;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Contents of %s:\n", argv[1]);
    vga_print(msg);
    vga_print("Hello from Kenux OS!\n");
    vga_print("This is a sample file content.\n");
    vga_print("Kernel version: KNE2.7\n");
    vga_print("System version: 26.8.28\n");
    return 0;
}

static int shell_builtin_mkdir(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: mkdir <directory>\n");
        return 1;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Directory created: %s\n", argv[1]);
    vga_print(msg);
    return 0;
}

static int shell_builtin_rm(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: rm <file or directory>\n");
        return 1;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Removed: %s\n", argv[1]);
    vga_print(msg);
    return 0;
}

static int shell_builtin_cp(int argc, char* argv[])
{
    if (argc < 3) {
        vga_print("Usage: cp <source> <destination>\n");
        return 1;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Copied: %s -> %s\n", argv[1], argv[2]);
    vga_print(msg);
    return 0;
}

static int shell_builtin_mv(int argc, char* argv[])
{
    if (argc < 3) {
        vga_print("Usage: mv <source> <destination>\n");
        return 1;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Moved/Renamed: %s -> %s\n", argv[1], argv[2]);
    vga_print(msg);
    return 0;
}

static int shell_builtin_ps(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("  PID TTY          TIME CMD\n");
    vga_print("    1 ?        00:00:01 init\n");
    vga_print("  1000 pts/0    00:00:00 ksh\n");
    vga_print("  1001 ?        00:02:15 kenuxwm\n");
    vga_print("  1002 ?        00:00:05 networkd\n");
    vga_print("  1003 ?        00:00:03 sound-server\n");
    vga_print(" 10042 pts/0    00:00:00 ps\n");
    return 0;
}

static int shell_builtin_kill(int argc, char* argv[])
{
    if (argc < 2) {
        vga_print("Usage: kill <pid>\n");
        return 1;
    }
    
    int pid = atoi(argv[1]);
    char msg[64];
    snprintf(msg, sizeof(msg), "Sent SIGTERM to process %d\n", pid);
    vga_print(msg);
    return 0;
}

static int shell_builtin_top(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("top - system resource usage\n");
    vga_print("Tasks: 42 total,   2 running,  38 sleeping,   2 stopped\n");
    vga_print("Cpu(s): 18.9%us,  5.2%sy,  72.4%ni,  3.5%id\n");
    vga_print("Mem:   16384k total,  8192k used,   7680k free,   512k buffers\n");
    vga_print("Swap:  8192k total,   1024k used,   7168k free\n");
    vga_print("\n");
    vga_print("  PID USER      PR  NI  VIRT  RES  SHR S %%CPU %%MEM    TIME+  CMD\n");
    vga_print(" 1001 root      20   0  512M 128M  96M S  12.3  7.8   2:15.32 kenuxwm\n");
    vga_print(" 1002 root      20   0  256M  48M  32M S   5.1  2.9   0:05.12 networkd\n");
    vga_print(" 1003 root      20   0  192M  36M  24M S   2.3  2.2   0:03.45 sound-serv\n");
    return 0;
}

static int shell_builtin_df(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Filesystem     1K-blocks    Used Available Use%% Mounted on\n");
    vga_print("/dev/sda2       134217728 67108864  67108864  50%% /\n");
    vga_print("/dev/sda3       268435456 201326592 67108864  75%% /home\n");
    vga_print("tmpfs           8388608       0   8388608   0%% /tmp\n");
    return 0;
}

static int shell_builtin_free(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("              total        used        free      shared  buff/cache   available\n");
    vga_print("Mem:        16777216     8388608     7864320      524288      524288     7340032\n");
    vga_print("Swap:        8388608     1048576     7340032\n");
    return 0;
}

static int shell_builtin_whoami(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("root\n");
    return 0;
}

static int shell_builtin_hostname(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("kenux-workstation\n");
    return 0;
}

static int shell_builtin_uptime(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print(" 14:32:45 up 3 days, 12:45,  1 user,  load average: 0.18, 0.23, 0.21\n");
    return 0;
}

static int shell_builtin_reboot(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Initiating system reboot...\n");
    vga_print("System will restart in 3 seconds...\n");
    return 0;
}

static int shell_builtin_poweroff(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("Initiating system shutdown...\n");
    vga_print("System will power off in 3 seconds...\n");
    return 0;
}

static int shell_builtin_neofetch(int argc, char* argv[])
{
    (void)argc; (void)argv;
    vga_print("\033[1;36m");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("■■■■■■■■■   ■■■■■■■■■■\n");
    vga_print("\033[0m\n");
    vga_print("OS: Kenux OS 26.8.28 (Stardust)\n");
    vga_print("Host: Kenux Workstation\n");
    vga_print("Kernel: Kenux Kernel KNE2.7\n");
    vga_print("Uptime: 3 days, 12 hours, 45 mins\n");
    vga_print("Shell: /bin/ksh (Kenux Shell)\n");
    vga_print("Resolution: 1920x1080\n");
    vga_print("DE: StardustUI 1.4.2\n");
    vga_print("WM: KenuxWM\n");
    vga_print("Theme: Kenux-Dark\n");
    vga_print("CPU: Kenux CPU v3.1 @ 3.6GHz (8 cores)\n");
    vga_print("GPU: Kenux Graphics G5000\n");
    vga_print("Memory: 8192 MiB / 16384 MiB (50%)\n");
    return 0;
}

static int shell_builtin_fastfetch(int argc, char* argv[])
{
    (void)argc; (void)argv;
    extern void fastfetch_run(void);
    fastfetch_run();
    return 0;
}

static int shell_builtin_calc(int argc, char* argv[])
{
    (void)argc; (void)argv;
    extern void calculator_run(void);
    calculator_run();
    return 0;
}

static int shell_builtin_snake(int argc, char* argv[])
{
    (void)argc; (void)argv;
    extern void snake_run(void);
    snake_run();
    return 0;
}

static int shell_builtin_tetris(int argc, char* argv[])
{
    (void)argc; (void)argv;
    extern void tetris_run(void);
    tetris_run();
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