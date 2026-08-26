/*
 * Kenux OS - Bash Shell Implementation
 * Main shell functionality
 */

#include "shell.h"

#include <signal.h>
#include <sys/stat.h>

#ifdef _WIN32
/* Windows MinGW compatibility: POSIX-ish function/flag shims */
#ifndef S_IXUSR
#define S_IXUSR 0100
#endif
#ifndef isatty
#define isatty _isatty
#endif
#ifndef dup2
#define dup2 _dup2
#endif
#ifndef fileno
#define fileno _fileno
#endif
/* setenv: MinGW-w64 may or may not expose setenv depending on _POSIX; always
 * provide a shim backed by _putenv so the call site is portable. */
static inline int kenux_setenv(const char *name, const char *value, int overwrite) {
    if (!overwrite) {
        size_t namelen = strlen(name);
        char *existing = getenv(name);
        (void)namelen;
        if (existing) return 0;
    }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s=%s", name, value ? value : "");
    return _putenv(buf);
}
#define setenv kenux_setenv
#endif

// Built-in command names
static const char *builtin_names[] = {
    "cd", "pwd", "echo", "exit", "help", "export", "history", "jobs"
};

// Built-in function pointers (unified signature)
static int (*builtin_funcs[])(char **, int, ShellState *) = {
    builtin_cd, builtin_pwd, builtin_echo, builtin_exit, builtin_help,
    builtin_export, NULL, NULL
};

// Number of built-in commands
static const int num_builtins = sizeof(builtin_names) / sizeof(char *);

void shell_init(ShellState *state, int interactive) {
    state->interactive = interactive;
    state->running = 1;

    // Get current working directory
    if (getcwd(state->current_dir, MAX_PATH_LEN) == NULL) {
        strcpy(state->current_dir, "/");
    }

    // Initialize previous directory
    strcpy(state->previous_dir, state->current_dir);

    // Set up signal handlers
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    printf("Kenux OS Shell\n");
    if (interactive) {
        printf("Type 'help' for available commands\n");
    }
}

void print_prompt(ShellState *state) {
    if (state->interactive) {
        printf("\033[32m%s@kenux:\033[34m%s\033[0m$ ",
               getenv("USER") ? getenv("USER") : "user",
               state->current_dir);
        fflush(stdout);
    }
}

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char **split_line(char *line) {
    int bufsize = MAX_ARGS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens) {
        fprintf(stderr, "kenux-shell: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, "\t\r\n");
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += MAX_ARGS;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                fprintf(stderr, "kenux-shell: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, "\t\r\n");
    }

    tokens[position] = NULL;
    return tokens;
}

void parse_redirects(Command *cmd) {
    char *input_redirect = strchr(cmd->command, '<');
    char *output_redirect = strchr(cmd->command, '>');

    if (input_redirect) {
        *input_redirect = '\0';
        sscanf(input_redirect + 1, "%s", cmd->input_file);
    }

    if (output_redirect) {
        *output_redirect = '\0';
        if (output_redirect[1] == '>') {
            cmd->append_output = 1;
            output_redirect++;
            sscanf(output_redirect + 1, "%s", cmd->output_file);
        } else {
            cmd->append_output = 0;
            sscanf(output_redirect + 1, "%s", cmd->output_file);
        }
    }

    // Check for background execution
    char *ampersand = strchr(cmd->command, '&');
    if (ampersand) {
        *ampersand = '\0';
        cmd->background = 1;
    } else {
        cmd->background = 0;
    }
}

void parse_command(char *input, Command *cmd) {
    // Initialize command structure
    memset(cmd, 0, sizeof(Command));

    // Parse input and output redirects
    strcpy(cmd->command, input);
    parse_redirects(cmd);

    // Split command into arguments
    char **args = split_line(cmd->command);

    // Count arguments
    cmd->argc = 0;
    while (args[cmd->argc] != NULL) {
        cmd->argc++;
    }

    // Copy arguments
    for (int i = 0; i < cmd->argc && i < MAX_ARGS - 1; i++) {
        cmd->args[i] = args[i];
    }
    cmd->args[cmd->argc] = NULL;

    free(args);
}

int builtin_cd(char **args, int argc, ShellState *state) {
    char *target_dir;
    char *path = (argc > 1) ? args[1] : NULL;

    if (path == NULL || strcmp(path, "") == 0) {
        target_dir = getenv("HOME");
        if (target_dir == NULL) {
            target_dir = "/";
        }
    } else if (strcmp(path, "-") == 0) {
        target_dir = state->previous_dir;
    } else {
        target_dir = path;
    }

    // Store current directory as previous
    strcpy(state->previous_dir, state->current_dir);

    // Change directory
    if (chdir(target_dir) == 0) {
        if (getcwd(state->current_dir, MAX_PATH_LEN) == NULL) {
            strcpy(state->current_dir, "/");
        }
        return 0;
    } else {
        perror("cd");
        return 1;
    }
}

int builtin_pwd(char **args, int argc, ShellState *state) {
    (void)args; (void)argc; (void)state;
    char cwd[MAX_PATH_LEN];
    if (getcwd(cwd, MAX_PATH_LEN) != NULL) {
        printf("%s\n", cwd);
        return 0;
    } else {
        perror("pwd");
        return 1;
    }
}

int builtin_echo(char **args, int argc, ShellState *state) {
    (void)state;
    for (int i = 1; i < argc; i++) {
        printf("%s", args[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

int builtin_exit(char **args, int argc, ShellState *state) {
    (void)args; (void)argc; (void)state;
    return -1; // Signal to exit
}

int builtin_help(char **args, int argc, ShellState *state) {
    (void)args; (void)argc; (void)state;
    printf("Kenux OS Shell - Available Commands:\n");
    printf("  cd [dir]      - Change directory\n");
    printf("  pwd           - Print working directory\n");
    printf("  echo [text]   - Echo text to output\n");
    printf("  exit          - Exit the shell\n");
    printf("  help          - Show this help message\n");
    printf("  export VAR=val - Set environment variable\n");
    printf("\n");
    printf("  Command syntax: [command] [args] [< input] [> output] [> output] [&]\n");
    return 0;
}

int builtin_export(char **args, int argc, ShellState *state) {
    (void)state;
    if (argc < 2) {
        printf("Usage: export VAR=value\n");
        return 1;
    }

    // Simple variable=value parsing
    char *eq = strchr(args[1], '=');
    if (eq) {
        *eq = '\0';
        char *var = args[1];
        char *value = eq + 1;
        if (setenv(var, value, 1) != 0) {
            perror("export");
            return 1;
        }
    } else {
        if (setenv(args[1], "", 1) != 0) {
            perror("export");
            return 1;
        }
    }

    return 0;
}

char *find_executable(const char *name, char *path) {
    char *env_path = getenv("PATH");
    if (env_path == NULL) {
        env_path = "/bin:/usr/bin";
    }

    char *path_copy = strdup(env_path);
    char *dir = strtok(path_copy, ":");

    while (dir != NULL) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", dir, name);

        struct stat st;
        if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(path_copy);
            strcpy(path, full_path);
            return path;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

void execute_command(Command *cmd, ShellState *state) {
    // Check if it's a built-in command
    for (int i = 0; i < num_builtins; i++) {
        if (strcmp(cmd->args[0], builtin_names[i]) == 0) {
            int result;
            if (builtin_funcs[i] != NULL) {
                result = builtin_funcs[i](cmd->args, cmd->argc, state);
            } else {
                result = 0;
            }

            if (result == -1) {
                state->running = 0;
            }
            return;
        }
    }

    // Find executable
    char exec_path[MAX_PATH_LEN];
    if (find_executable(cmd->args[0], exec_path) == NULL) {
        fprintf(stderr, "kenux-shell: command not found: %s\n", cmd->args[0]);
        return;
    }

    // Handle input redirection
    if (cmd->input_file[0] != '\0') {
        FILE *input = fopen(cmd->input_file, "r");
        if (input == NULL) {
            perror("input file");
            return;
        }
        dup2(fileno(input), STDIN_FILENO);
        fclose(input);
    }

    // Handle output redirection
    if (cmd->output_file[0] != '\0') {
        FILE *output = fopen(cmd->output_file,
                          cmd->append_output ? "a" : "w");
        if (output == NULL) {
            perror("output file");
            return;
        }
        dup2(fileno(output), STDOUT_FILENO);
        fclose(output);
    }

    // Fork and execute
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        execvp(exec_path, cmd->args);
        perror("exec");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
    } else {
        // Parent process
        if (!cmd->background) {
            int status;
            waitpid(pid, &status, 0);
        }
    }

    // Restore standard input/output
    if (cmd->input_file[0] != '\0') {
        dup2(STDIN_FILENO, STDIN_FILENO);
    }
    if (cmd->output_file[0] != '\0') {
        dup2(STDOUT_FILENO, STDOUT_FILENO);
    }
}

void shell_loop(ShellState *state) {
    char *line;
    Command cmd;

    while (state->running) {
        print_prompt(state);

        line = read_line();

        if (line[0] != '\0' && line[0] != '\n') {
            parse_command(line, &cmd);
            if (cmd.args[0] != NULL) {
                execute_command(&cmd, state);
            }
        }

        free(line);
    }
}

int main(int argc, char **argv) {
    ShellState state;
    int interactive = isatty(STDIN_FILENO);

    shell_init(&state, interactive);

    if (argc > 1) {
        // Execute command from file
        FILE *script = fopen(argv[1], "r");
        if (script == NULL) {
            perror("script file");
            return EXIT_FAILURE;
        }

        char *line = NULL;
        size_t len = 0;
        ssize_t got;
        Command cmd;

        while ((got = getline(&line, &len, script)) != -1) {
            parse_command(line, &cmd);
            if (cmd.args[0] != NULL) {
                execute_command(&cmd, &state);
            }
        }

        free(line);
        fclose(script);
    } else {
        // Interactive mode
        shell_loop(&state);
    }

    return EXIT_SUCCESS;
}
