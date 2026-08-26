/*
 * Kenux OS - Make Utility Implementation
 * Main make functionality
 */

#include "make.h"

#include <sys/stat.h>

void make_init(MakeState *state) {
    memset(state, 0, sizeof(MakeState));
    state->verbose = 0;
    state->dry_run = 0;
}

const char *get_variable(MakeState *state, const char *name) {
    for (int i = 0; i < state->num_variables; i++) {
        if (strcmp(state->variables[i].name, name) == 0) {
            return state->variables[i].value;
        }
    }
    return NULL;
}

void set_variable(MakeState *state, const char *name, const char *value) {
    // Check if variable already exists
    for (int i = 0; i < state->num_variables; i++) {
        if (strcmp(state->variables[i].name, name) == 0) {
            strncpy(state->variables[i].value, value, MAX_VAR_VALUE - 1);
            state->variables[i].value[MAX_VAR_VALUE - 1] = '\0';
            return;
        }
    }
    
    // Add new variable
    if (state->num_variables < MAX_VAR_NAME) {
        strncpy(state->variables[state->num_variables].name, name, MAX_VAR_NAME - 1);
        state->variables[state->num_variables].name[MAX_VAR_NAME - 1] = '\0';
        strncpy(state->variables[state->num_variables].value, value, MAX_VAR_VALUE - 1);
        state->variables[state->num_variables].value[MAX_VAR_VALUE - 1] = '\0';
        state->num_variables++;
    }
}

void expand_variables(MakeState *state, char *str) {
    char *ptr = str;
    char *dst = str;
    char var_name[MAX_VAR_NAME];
    
    while (*ptr) {
        if (*ptr == '$' && *(ptr + 1) == '{') {
            // Find variable name
            ptr += 2;
            char *var_start = ptr;
            while (*ptr && *ptr != '}') {
                ptr++;
            }
            
            if (*ptr == '}') {
                int len = ptr - var_start;
                if (len < MAX_VAR_NAME) {
                    strncpy(var_name, var_start, len);
                    var_name[len] = '\0';
                    
                    const char *var_value = get_variable(state, var_name);
                    if (var_value) {
                        strcpy(dst, var_value);
                        dst += strlen(var_value);
                    }
                }
                ptr++; // Skip the '}'
            } else {
                // Invalid variable syntax, copy as-is
                *dst++ = *ptr++;
            }
        } else {
            *dst++ = *ptr++;
        }
    }
    *dst = '\0';
}

int load_makefile(MakeState *state, const char *filename) {
    FILE *file;
    
    if (filename) {
        strcpy(state->makefile_path, filename);
        file = fopen(filename, "r");
    } else {
        // Try common makefile names
        const char *filenames[] = {"Makefile", "makefile", "Makefile.mk", ".makefile"};
        
        for (int i = 0; i < sizeof(filenames) / sizeof(char *); i++) {
            strcpy(state->makefile_path, filenames[i]);
            file = fopen(filenames[i], "r");
            if (file) {
                break;
            }
        }
        
        if (!file) {
            fprintf(stderr, "kenux-make: no makefile found\n");
            return -1;
        }
    }
    
    if (!file) {
        fprintf(stderr, "kenux-make: cannot open file: %s\n", state->makefile_path);
        return -1;
    }
    
    int result = parse_makefile(state, file);
    fclose(file);
    return result;
}

int parse_makefile(MakeState *state, FILE *file) {
    char line[MAX_LINE_LEN];
    int line_number = 0;
    int in_target = 0;
    int current_target = -1;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        
        // Remove trailing comments
        char *comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }
        
        // Expand variables in the line
        expand_variables(state, line);
        
        // Determine if this is a target line, variable line, or command line
        if (strchr(line, ':')) {
            // Target line
            current_target = parse_target_line(state, line);
            if (current_target >= 0) {
                in_target = 1;
            } else {
                in_target = 0;
            }
        } else if (in_target && (line[0] == '\t' || strncmp(line, "    ", 4) == 0)) {
            // Command line
            if (current_target >= 0) {
                parse_command_line(state, line, current_target);
            }
        } else if (strchr(line, '=')) {
            // Variable line
            parse_variable_line(state, line);
        } else if (line[0] == '\t' || strncmp(line, "    ", 4) == 0) {
            // Command line without target (error)
            fprintf(stderr, "kenux-make: line %d: command without target\n", line_number);
        }
    }
    
    return 0;
}

int parse_variable_line(MakeState *state, char *line) {
    char *eq = strchr(line, '=');
    if (!eq) {
        return -1;
    }
    
    *eq = '\0';
    char *name = line;
    char *value = eq + 1;
    
    // Trim whitespace
    while (*name == ' ' || *name == '\t') name++;
    while (*value == ' ' || *value == '\t') value++;
    
    // Remove trailing whitespace from value
    char *end = value + strlen(value) - 1;
    while (end > value && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    set_variable(state, name, value);
    return 0;
}

int parse_target_line(MakeState *state, char *line) {
    char *colon = strchr(line, ':');
    if (!colon) {
        return -1;
    }
    
    *colon = '\0';
    char *target_name = line;
    char *deps = colon + 1;
    
    // Trim whitespace from target name
    while (*target_name == ' ' || *target_name == '\t') target_name++;
    
    // Check if target already exists
    int target_idx = find_target_index(state, target_name);
    if (target_idx < 0) {
        // Add new target
        if (state->num_targets >= MAX_RULES) {
            fprintf(stderr, "kenux-make: too many targets\n");
            return -1;
        }
        target_idx = state->num_targets++;
        memset(&state->targets[target_idx], 0, sizeof(Target));
        strcpy(state->targets[target_idx].name, target_name);
    }
    
    // Parse dependencies
    char *dep = strtok(deps, " \t");
    while (dep && state->targets[target_idx].num_dependencies < MAX_DEPENDENCIES) {
        // Trim whitespace from dependency
        while (*dep == ' ' || *dep == '\t') dep++;
        
        if (*dep) {
            strcpy(state->targets[target_idx].dependencies[state->targets[target_idx].num_dependencies], dep);
            state->targets[target_idx].num_dependencies++;
        }
        dep = strtok(NULL, " \t");
    }
    
    // Get timestamp
    struct stat st;
    if (stat(target_name, &st) == 0) {
        state->targets[target_idx].timestamp = st.st_mtime;
    } else {
        state->targets[target_idx].timestamp = 0;
    }
    
    return target_idx;
}

int parse_command_line(MakeState *state, char *line, int target_idx) {
    if (target_idx < 0 || target_idx >= state->num_targets) {
        return -1;
    }
    
    // Remove leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    if (*line && state->targets[target_idx].num_commands < MAX_COMMANDS) {
        strcpy(state->targets[target_idx].commands[state->targets[target_idx].num_commands], line);
        state->targets[target_idx].num_commands++;
        return 0;
    }
    
    return -1;
}

int find_target_index(MakeState *state, const char *name) {
    for (int i = 0; i < state->num_targets; i++) {
        if (strcmp(state->targets[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int target_exists(MakeState *state, const char *name) {
    return find_target_index(state, name) >= 0;
}

time_t get_file_timestamp(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

int is_file_uptodate(MakeState *state, int target_idx) {
    Target *target = &state->targets[target_idx];
    time_t target_time = target->timestamp;
    
    // Check dependencies
    for (int i = 0; i < target->num_dependencies; i++) {
        const char *dep = target->dependencies[i];
        time_t dep_time = get_file_timestamp(dep);
        
        if (dep_time == 0) {
            return 0; // Dependency doesn't exist
        }
        
        if (dep_time > target_time) {
            return 0; // Dependency is newer
        }
    }
    
    return 1; // Target is up to date
}

int build_target(MakeState *state, int target_idx, int dependency_depth) {
    if (target_idx < 0 || target_idx >= state->num_targets) {
        return -1;
    }
    
    Target *target = &state->targets[target_idx];
    
    // Check if target is already built
    if (target->built) {
        return 0;
    }
    
    // Build dependencies first
    for (int i = 0; i < target->num_dependencies; i++) {
        int dep_idx = find_target_index(state, target->dependencies[i]);
        if (dep_idx >= 0) {
            if (dependency_depth < 10) { // Prevent infinite recursion
                if (build_target(state, dep_idx, dependency_depth + 1) != 0) {
                    fprintf(stderr, "kenux-make: failed to build dependency: %s\n", 
                            target->dependencies[i]);
                    return -1;
                }
            } else {
                fprintf(stderr, "kenux-make: dependency depth exceeded for: %s\n", 
                        target->dependencies[i]);
                return -1;
            }
        }
    }
    
    // Check if target needs to be built
    if (is_file_uptodate(state, target_idx)) {
        if (state->verbose) {
            printf("%s is up to date\n", target->name);
        }
        target->built = 1;
        return 0;
    }
    
    // Execute commands
    if (state->verbose) {
        printf("Building %s...\n", target->name);
    }
    
    if (execute_commands(state, target_idx) != 0) {
        fprintf(stderr, "kenux-make: failed to build target: %s\n", target->name);
        return -1;
    }
    
    // Update timestamp
    target->timestamp = get_file_timestamp(target->name);
    target->built = 1;
    
    return 0;
}

int execute_commands(MakeState *state, int target_idx) {
    Target *target = &state->targets[target_idx];
    
    for (int i = 0; i < target->num_commands; i++) {
        if (execute_command(state, target->commands[i]) != 0) {
            return -1;
        }
    }
    
    return 0;
}

int execute_command(MakeState *state, const char *command) {
    if (state->dry_run) {
        printf("%s\n", command);
        return 0;
    }
    
    if (state->verbose) {
        printf("Executing: %s\n", command);
    }
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("exec");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        return -1;
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Command terminated by signal %d\n", WTERMSIG(status));
            return -1;
        } else {
            fprintf(stderr, "Command terminated abnormally\n");
            return -1;
        }
    }
}

void print_help(void) {
    printf("Kenux OS Make Utility\n");
    printf("Usage: make [options] [target]\n");
    printf("Options:\n");
    printf("  -f file       Specify makefile name\n");
    printf("  -v            Verbose output\n");
    printf("  -n            Dry run (don't execute commands)\n");
    printf("  -h            Show this help message\n");
    printf("  -p            Print all variables and targets\n");
    printf("Targets:\n");
    printf("  (specify a target name on the command line)\n");
}

int main(int argc, char **argv) {
    MakeState state;
    make_init(&state);
    
    // Parse command line arguments
    char *makefile = NULL;
    const char *target = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            makefile = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            state.verbose = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            state.dry_run = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_help();
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-p") == 0) {
            // Print all variables and targets
            for (int j = 0; j < state.num_variables; j++) {
                printf("%s = %s\n", state.variables[j].name, state.variables[j].value);
            }
            for (int j = 0; j < state.num_targets; j++) {
                printf("%s: ", state.targets[j].name);
                for (int k = 0; k < state.targets[j].num_dependencies; k++) {
                    printf("%s ", state.targets[j].dependencies[k]);
                }
                printf("\n");
            }
            return EXIT_SUCCESS;
        } else if (argv[i][0] != '-') {
            // Treat as target name
            if (!target) {
                target = argv[i];
            } else {
                fprintf(stderr, "kenux-make: multiple targets specified\n");
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr, "kenux-make: unknown option: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }
    
    // Load makefile
    if (load_makefile(&state, makefile) != 0) {
        return EXIT_FAILURE;
    }
    
    // If no target specified, use first target or default to "all"
    if (!target) {
        for (int i = 0; i < state.num_targets; i++) {
            if (strcmp(state.targets[i].name, "all") == 0) {
                target = "all";
                break;
            }
        }
        
        if (!target && state.num_targets > 0) {
            target = state.targets[0].name;
        } else {
            fprintf(stderr, "kenux-make: no targets to build\n");
            return EXIT_FAILURE;
        }
    }
    
    // Find and build target
    int target_idx = find_target_index(&state, target);
    if (target_idx < 0) {
        fprintf(stderr, "kenux-make: target '%s' not found\n", target);
        return EXIT_FAILURE;
    }
    
    if (build_target(&state, target_idx, 0) != 0) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}