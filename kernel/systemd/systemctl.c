#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

#define SYSTEMCTL_MAX_ARGS 32
#define SYSTEMD_UNIT_PATH "/etc/systemd/system"

unit_t* find_unit(const char* name);

static void serial_printf(const char* format, ...)
{
    (void)format;
}

static void systemctl_usage(const char* prog)
{
    serial_printf("Usage: %s [COMMAND] [UNIT...]\n\n", prog);
    serial_printf("Commands:\n");
    serial_printf("  start <unit>          Start a unit\n");
    serial_printf("  stop <unit>           Stop a unit\n");
    serial_printf("  restart <unit>        Restart a unit\n");
    serial_printf("  reload <unit>         Reload a unit's configuration\n");
    serial_printf("  status <unit>         Show unit status\n");
    serial_printf("  list-units            List all loaded units\n");
    serial_printf("  list-unit-files       List all unit files\n");
    serial_printf("  enable <unit>         Enable a unit\n");
    serial_printf("  disable <unit>        Disable a unit\n");
    serial_printf("  is-active <unit>      Check if unit is active\n");
    serial_printf("  is-enabled <unit>     Check if unit is enabled\n");
    serial_printf("  daemon-reload         Reload systemd configuration\n");
    serial_printf("  get-default           Show default target\n");
    serial_printf("  set-default <target>  Set default target\n");
    serial_printf("  isolate <target>      Switch to a target\n");
    serial_printf("\n");
}

static void systemctl_list_units(void)
{
    serial_printf("%-30s %-15s %-20s %s\n", "UNIT", "LOAD", "ACTIVE", "DESCRIPTION");
    serial_printf("-----------------------------------------------------------\n");
    
    for (int i = 0; i < systemd.unit_count; i++) {
        unit_t* unit = systemd.units[i];
        
        const char* load_state;
        switch (unit->state) {
            case UNIT_STATE_DEAD: load_state = "loaded"; break;
            case UNIT_STATE_LOADING: load_state = "loading"; break;
            case UNIT_STATE_ACTIVATING: load_state = "loaded"; break;
            case UNIT_STATE_ACTIVE: load_state = "loaded"; break;
            case UNIT_STATE_RUNNING: load_state = "loaded"; break;
            case UNIT_STATE_DEACTIVATING: load_state = "loaded"; break;
            case UNIT_STATE_EXITED: load_state = "loaded"; break;
            case UNIT_STATE_FAILED: load_state = "loaded"; break;
            default: load_state = "unknown";
        }
        
        const char* active_state;
        switch (unit->state) {
            case UNIT_STATE_DEAD: active_state = "inactive"; break;
            case UNIT_STATE_LOADING: active_state = "activating"; break;
            case UNIT_STATE_ACTIVATING: active_state = "activating"; break;
            case UNIT_STATE_ACTIVE: active_state = "active"; break;
            case UNIT_STATE_RUNNING: active_state = "active"; break;
            case UNIT_STATE_DEACTIVATING: active_state = "deactivating"; break;
            case UNIT_STATE_EXITED: active_state = "inactive"; break;
            case UNIT_STATE_FAILED: active_state = "failed"; break;
            default: active_state = "unknown";
        }
        
        const char* sub_state = "-";
        if (unit->type == UNIT_TYPE_SERVICE) {
            service_t* svc = (service_t*)unit;
            if (svc->running) sub_state = "running";
            else sub_state = "exited";
        }
        
        const char* type_suffix = "";
        switch (unit->type) {
            case UNIT_TYPE_SERVICE: type_suffix = ".service"; break;
            case UNIT_TYPE_TARGET: type_suffix = ".target"; break;
            case UNIT_TYPE_SOCKET: type_suffix = ".socket"; break;
            case UNIT_TYPE_TIMER: type_suffix = ".timer"; break;
            case UNIT_TYPE_MOUNT: type_suffix = ".mount"; break;
            case UNIT_TYPE_SLICE: type_suffix = ".slice"; break;
            case UNIT_TYPE_DEVICE: type_suffix = ".device"; break;
            case UNIT_TYPE_AUTOMOUNT: type_suffix = ".automount"; break;
            case UNIT_TYPE_PATH: type_suffix = ".path"; break;
            case UNIT_TYPE_SWAP: type_suffix = ".swap"; break;
            default: type_suffix = "";
        }
        
        char full_name[SYSTEMD_MAX_NAME + 32];
        sprintf(full_name, "%s%s", unit->name[0] ? unit->name : "unknown", type_suffix);
        serial_printf("%-30s %-15s %-20s %s\n", 
                      full_name,
                      load_state,
                      active_state,
                      unit->description[0] ? unit->description : "-");
    }
}

static void systemctl_list_unit_files(void)
{
    serial_printf("%-40s %s\n", "UNIT FILE", "STATE");
    serial_printf("-----------------------------------------\n");
    
    char path[SYSTEMD_MAX_PATH_LEN];
    char entries[8192];
    sprintf(path, "%s", SYSTEMD_UNIT_PATH);
    fs_list_dir(path, entries, sizeof(entries));
    
    sprintf(path, "/usr/lib/systemd/system");
    fs_list_dir(path, entries, sizeof(entries));
    
    sprintf(path, "/run/systemd/system");
    fs_list_dir(path, entries, sizeof(entries));
    
    for (int i = 0; i < systemd.unit_count; i++) {
        unit_t* unit = systemd.units[i];
        
        const char* state = unit->enabled ? "enabled" : "disabled";
        
        const char* type_suffix = "";
        switch (unit->type) {
            case UNIT_TYPE_SERVICE: type_suffix = ".service"; break;
            case UNIT_TYPE_TARGET: type_suffix = ".target"; break;
            case UNIT_TYPE_SOCKET: type_suffix = ".socket"; break;
            case UNIT_TYPE_TIMER: type_suffix = ".timer"; break;
            case UNIT_TYPE_MOUNT: type_suffix = ".mount"; break;
            case UNIT_TYPE_SLICE: type_suffix = ".slice"; break;
            default: type_suffix = "";
        }
        
        char full_name[SYSTEMD_MAX_NAME + 32];
        sprintf(full_name, "%s%s", unit->name[0] ? unit->name : "unknown", type_suffix);
        serial_printf("%-40s %s\n", 
                      full_name,
                      state);
    }
}

static void systemctl_status(const char* unit_name)
{
    unit_t* unit = find_unit(unit_name);
    if (!unit) {
        serial_printf("Unit '%s' not found.\n", unit_name);
        return;
    }
    
    const char* type_suffix = "";
    switch (unit->type) {
        case UNIT_TYPE_SERVICE: type_suffix = ".service"; break;
        case UNIT_TYPE_TARGET: type_suffix = ".target"; break;
        case UNIT_TYPE_SOCKET: type_suffix = ".socket"; break;
        case UNIT_TYPE_TIMER: type_suffix = ".timer"; break;
        case UNIT_TYPE_MOUNT: type_suffix = ".mount"; break;
        case UNIT_TYPE_SLICE: type_suffix = ".slice"; break;
        default: type_suffix = "";
    }
    
    serial_printf("%s%s\n", unit->name, type_suffix);
    serial_printf("  Loaded: %s (%s)\n", 
                  unit->state != UNIT_STATE_DEAD ? "loaded" : "not-found",
                  unit->source_path);
    serial_printf("  Active: %s\n", 
                  unit->state == UNIT_STATE_ACTIVE || unit->state == UNIT_STATE_RUNNING ? "active" : 
                  unit->state == UNIT_STATE_DEAD ? "inactive" : 
                  unit->state == UNIT_STATE_FAILED ? "failed" : "activating");
    
    if (unit->type == UNIT_TYPE_SERVICE) {
        service_t* svc = (service_t*)unit;
        serial_printf("  Main PID: %llu\n", svc->main_pid);
        serial_printf("  Status: %d\n", svc->exit_code);
        serial_printf("  Restarts: %d\n", svc->restart_count);
    } else if (unit->type == UNIT_TYPE_TIMER) {
        timer_t* timer = (timer_t*)unit;
        serial_printf("  Next elapse: %llu\n", timer->next_elapse_time);
        serial_printf("  Last trigger: %llu\n", timer->last_trigger_time);
        serial_printf("  Trigger count: %d\n", timer->trigger_count);
    }
    
    serial_printf("  Description: %s\n", unit->description);
    
    if (unit->wants_count > 0) {
        serial_printf("  Wants: ");
        for (int i = 0; i < unit->wants_count; i++) {
            if (i > 0) serial_printf(", ");
            serial_printf("%s", unit->wants[i].name);
        }
        serial_printf("\n");
    }
    
    if (unit->requires_count > 0) {
        serial_printf("  Requires: ");
        for (int i = 0; i < unit->requires_count; i++) {
            if (i > 0) serial_printf(", ");
            serial_printf("%s", unit->requires[i].name);
        }
        serial_printf("\n");
    }
}

static void systemctl_is_active(const char* unit_name)
{
    unit_t* unit = find_unit(unit_name);
    if (!unit) {
        serial_printf("inactive\n");
        return;
    }
    
    if (unit->state == UNIT_STATE_ACTIVE || unit->state == UNIT_STATE_RUNNING) {
        serial_printf("active\n");
    } else {
        serial_printf("inactive\n");
    }
}

static void systemctl_is_enabled(const char* unit_name)
{
    unit_t* unit = find_unit(unit_name);
    if (!unit) {
        serial_printf("disabled\n");
        return;
    }
    
    serial_printf("%s\n", unit->enabled ? "enabled" : "disabled");
}

static void systemctl_get_default(void)
{
    serial_printf("%s.target\n", systemd.default_target);
}

static void systemctl_set_default(const char* target_name)
{
    strncpy(systemd.default_target, target_name, SYSTEMD_MAX_NAME - 1);
    
    char path[SYSTEMD_MAX_PATH_LEN];
    sprintf(path, "%s/default.target", SYSTEMD_UNIT_PATH);
    
    vfs_mkdir(SYSTEMD_UNIT_PATH, 0755);
    
    int fd = vfs_open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0644);
    if (fd >= 0) {
        char content[256];
        sprintf(content, "[Unit]\nDescription=Default Target\n\n[Install]\nAlias=default.target\n");
        vfs_write(fd, content, strlen(content));
        vfs_close(fd);
    }
    
    serial_printf("Created symlink %s.\n", path);
}

int systemctl_main(int argc, char** argv)
{
    if (argc < 2) {
        systemctl_usage(argv[0]);
        return 1;
    }
    
    const char* command = argv[1];
    
    if (strcmp(command, "start") == 0) {
        if (argc < 3) {
            serial_printf("Error: start requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            int ret = systemd_start_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to start %s\n", argv[i]);
            } else {
                serial_printf("Started %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "stop") == 0) {
        if (argc < 3) {
            serial_printf("Error: stop requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            int ret = systemd_stop_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to stop %s\n", argv[i]);
            } else {
                serial_printf("Stopped %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "restart") == 0) {
        if (argc < 3) {
            serial_printf("Error: restart requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            systemd_stop_unit(argv[i]);
            msleep(100);
            int ret = systemd_start_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to restart %s\n", argv[i]);
            } else {
                serial_printf("Restarted %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "reload") == 0) {
        if (argc < 3) {
            serial_printf("Error: reload requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            int ret = systemd_reload_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to reload %s\n", argv[i]);
            } else {
                serial_printf("Reloaded %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "status") == 0) {
        if (argc < 3) {
            systemctl_list_units();
        } else {
            for (int i = 2; i < argc; i++) {
                systemctl_status(argv[i]);
            }
        }
    } else if (strcmp(command, "list-units") == 0) {
        systemctl_list_units();
    } else if (strcmp(command, "list-unit-files") == 0) {
        systemctl_list_unit_files();
    } else if (strcmp(command, "enable") == 0) {
        if (argc < 3) {
            serial_printf("Error: enable requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            int ret = systemd_enable_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to enable %s\n", argv[i]);
            } else {
                serial_printf("Enabled %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "disable") == 0) {
        if (argc < 3) {
            serial_printf("Error: disable requires a unit name.\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            int ret = systemd_disable_unit(argv[i]);
            if (ret != 0) {
                serial_printf("Failed to disable %s\n", argv[i]);
            } else {
                serial_printf("Disabled %s\n", argv[i]);
            }
        }
    } else if (strcmp(command, "is-active") == 0) {
        if (argc < 3) {
            serial_printf("Error: is-active requires a unit name.\n");
            return 1;
        }
        systemctl_is_active(argv[2]);
    } else if (strcmp(command, "is-enabled") == 0) {
        if (argc < 3) {
            serial_printf("Error: is-enabled requires a unit name.\n");
            return 1;
        }
        systemctl_is_enabled(argv[2]);
    } else if (strcmp(command, "daemon-reload") == 0) {
        systemd_reload_config();
        serial_printf("Reloaded systemd configuration.\n");
    } else if (strcmp(command, "get-default") == 0) {
        systemctl_get_default();
    } else if (strcmp(command, "set-default") == 0) {
        if (argc < 3) {
            serial_printf("Error: set-default requires a target name.\n");
            return 1;
        }
        systemctl_set_default(argv[2]);
    } else if (strcmp(command, "isolate") == 0) {
        if (argc < 3) {
            serial_printf("Error: isolate requires a target name.\n");
            return 1;
        }
        systemd_isolate(argv[2]);
        serial_printf("Isolated to %s\n", argv[2]);
    } else {
        serial_printf("Unknown command: %s\n", command);
        systemctl_usage(argv[0]);
        return 1;
    }
    
    return 0;
}