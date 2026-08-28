#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>
#include <arch/process.h>
#include <signal.h>
#include <cgroup.h>
#include <unixsock.h>

#define WEXITSTATUS(status) ((status) >> 8)

static bool process_is_running(uint64_t pid)
{
    (void)pid;
    return false;
}

static service_type_t parse_service_type(const char* str)
{
    if (strcmp(str, "forking") == 0) return SERVICE_TYPE_FORKING;
    if (strcmp(str, "oneshot") == 0) return SERVICE_TYPE_ONESHOT;
    if (strcmp(str, "dbus") == 0) return SERVICE_TYPE_DBUS;
    if (strcmp(str, "notify") == 0) return SERVICE_TYPE_NOTIFY;
    if (strcmp(str, "idle") == 0) return SERVICE_TYPE_IDLE;
    return SERVICE_TYPE_SIMPLE;
}

static service_restart_t parse_service_restart(const char* str)
{
    if (strcmp(str, "no") == 0) return RESTART_NO;
    if (strcmp(str, "always") == 0) return RESTART_ALWAYS;
    if (strcmp(str, "on-success") == 0) return RESTART_ON_SUCCESS;
    if (strcmp(str, "on-failure") == 0) return RESTART_ON_FAILURE;
    if (strcmp(str, "on-abnormal") == 0) return RESTART_ON_ABNORMAL;
    if (strcmp(str, "on-watchdog") == 0) return RESTART_ON_WATCHDOG;
    if (strcmp(str, "on-abort") == 0) return RESTART_ON_ABORT;
    return RESTART_NO;
}

void parse_exec_command(const char* cmdline, exec_command_t* exec)
{
    strncpy(exec->command, cmdline, SYSTEMD_MAX_CMDLINE - 1);
    exec->command[SYSTEMD_MAX_CMDLINE - 1] = '\0';
    
    exec->argc = 0;
    exec->argv[0] = exec->command;
    
    char* ptr = exec->command;
    bool in_quote = false;
    while (*ptr && exec->argc < SYSTEMD_MAX_ARGS - 1) {
        if (*ptr == '"') {
            in_quote = !in_quote;
            *ptr = '\0';
            ptr++;
            continue;
        }
        if (*ptr == ' ' && !in_quote) {
            *ptr = '\0';
            ptr++;
            while (*ptr == ' ') ptr++;
            if (*ptr) {
                exec->argv[++exec->argc] = ptr;
            }
        } else {
            ptr++;
        }
    }
    exec->argc++;
    exec->argv[exec->argc] = NULL;
}

static void parse_env_vars(const char* line, service_t* service)
{
    const char* eq = strchr(line, '=');
    if (!eq || eq == line) return;
    
    int len = eq - line;
    if (len >= SYSTEMD_MAX_NAME) len = SYSTEMD_MAX_NAME - 1;
    
    if (service->env_count >= SYSTEMD_MAX_ENV_VARS) return;
    
    strncpy(service->env[service->env_count].name, line, len);
    service->env[service->env_count].name[len] = '\0';
    strncpy(service->env[service->env_count].value, eq + 1, SYSTEMD_MAX_PATH_LEN - 1);
    service->env[service->env_count].value[SYSTEMD_MAX_PATH_LEN - 1] = '\0';
    service->env_count++;
}

static int parse_exec_section(service_t* service, const char* key, const char* value)
{
    if (strcmp(key, "ExecStartPre") == 0) {
        if (service->exec_start_pre_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_start_pre[service->exec_start_pre_count]);
            service->exec_start_pre_count++;
        }
    } else if (strcmp(key, "ExecStart") == 0) {
        if (service->exec_start_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_start[service->exec_start_count]);
            service->exec_start_count++;
        }
    } else if (strcmp(key, "ExecStartPost") == 0) {
        if (service->exec_start_post_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_start_post[service->exec_start_post_count]);
            service->exec_start_post_count++;
        }
    } else if (strcmp(key, "ExecStop") == 0) {
        if (service->exec_stop_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_stop[service->exec_stop_count]);
            service->exec_stop_count++;
        }
    } else if (strcmp(key, "ExecStopPost") == 0) {
        if (service->exec_stop_post_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_stop_post[service->exec_stop_post_count]);
            service->exec_stop_post_count++;
        }
    } else if (strcmp(key, "ExecReload") == 0) {
        if (service->exec_reload_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_reload[service->exec_reload_count]);
            service->exec_reload_count++;
        }
    } else if (strcmp(key, "ConditionPathExists") == 0) {
        if (service->exec_condition_count < SYSTEMD_MAX_EXEC_COMMANDS) {
            parse_exec_command(value, &service->exec_condition[service->exec_condition_count]);
            service->exec_condition_count++;
        }
    }
    return 0;
}

int parse_unit_section(unit_t* unit, const char* key, const char* value)
{
    if (strcmp(key, "Description") == 0) {
        strncpy(unit->description, value, SYSTEMD_MAX_DESCRIPTION - 1);
    } else if (strcmp(key, "Documentation") == 0) {
        strncpy(unit->documentation, value, SYSTEMD_MAX_PATH_LEN - 1);
    } else if (strcmp(key, "Wants") == 0) {
        parse_dependency_list(value, unit->wants, &unit->wants_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "Requires") == 0) {
        parse_dependency_list(value, unit->requires, &unit->requires_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "Before") == 0) {
        parse_dependency_list(value, unit->before, &unit->before_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "After") == 0) {
        parse_dependency_list(value, unit->after, &unit->after_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "BindsTo") == 0) {
        parse_dependency_list(value, unit->binds_to, &unit->binds_to_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "Conflicts") == 0) {
        parse_dependency_list(value, unit->conflicts, &unit->conflicts_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "DefaultDependencies") == 0) {
        unit->default_dependencies = (strcmp(value, "no") != 0);
    } else if (strcmp(key, "AllowIsolate") == 0) {
        unit->allow_isolate = (strcmp(value, "yes") == 0);
    }
    return 0;
}

int parse_install_section(unit_t* unit, const char* key, const char* value)
{
    if (strcmp(key, "WantedBy") == 0 || strcmp(key, "RequiredBy") == 0) {
        unit->enabled = true;
    } else if (strcmp(key, "Alias") == 0) {
        unit->enabled = true;
    }
    return 0;
}

static int parse_resource_limit(const char* value, char* limit)
{
    strncpy(limit, value, 63);
    limit[63] = '\0';
    return 0;
}

static int parse_service_section(service_t* service, const char* key, const char* value)
{
    if (strcmp(key, "Type") == 0) {
        service->type = parse_service_type(value);
    } else if (strcmp(key, "Restart") == 0) {
        service->restart = parse_service_restart(value);
    } else if (strcmp(key, "WorkingDirectory") == 0) {
        strncpy(service->working_dir, value, SYSTEMD_MAX_PATH_LEN - 1);
    } else if (strcmp(key, "User") == 0) {
        strncpy(service->user, value, 63);
        service->uid = atoi(value);
    } else if (strcmp(key, "Group") == 0) {
        strncpy(service->group, value, 63);
        service->gid = atoi(value);
    } else if (strcmp(key, "DynamicUser") == 0) {
        service->dynamic_user = (strcmp(value, "yes") == 0);
    } else if (strcmp(key, "RestartSec") == 0) {
        service->restart_sec = atoi(value);
    } else if (strcmp(key, "StartLimitInterval") == 0) {
        service->start_limit_interval = atoi(value);
    } else if (strcmp(key, "StartLimitBurst") == 0) {
        service->start_limit_burst = atoi(value);
    } else if (strcmp(key, "WatchdogSec") == 0) {
        service->watchdog_usec = (uint64_t)atoi(value) * 1000000;
    } else if (strcmp(key, "SuccessExitStatus") == 0) {
        char* token = strtok((char*)value, " ");
        while (token) {
            int status = atoi(token);
            if (status >= 0 && status < 256) {
                service->success_exit_status[status] = true;
            }
            token = strtok(NULL, " ");
        }
    } else if (strcmp(key, "Environment") == 0) {
        parse_env_vars(value, service);
    } else if (strcmp(key, "EnvironmentFile") == 0) {
        strncpy(service->environment_file, value, SYSTEMD_MAX_PATH_LEN - 1);
        char content[8192];
        if (fs_read_file_content(value, content, sizeof(content)) > 0) {
            char* line = content;
            char* next;
            while ((next = strchr(line, '\n')) != NULL) {
                *next = '\0';
                if (*line != '#' && *line != '\0') {
                    parse_env_vars(line, service);
                }
                line = next + 1;
            }
        }
    } else if (strcmp(key, "TimeoutStartSec") == 0) {
        service->timeout_start_sec = atoi(value);
    } else if (strcmp(key, "TimeoutStopSec") == 0) {
        service->timeout_stop_sec = atoi(value);
    } else if (strcmp(key, "TimeoutAbortSec") == 0) {
        service->timeout_abort_sec = atoi(value);
    } else if (strcmp(key, "KillMode") == 0) {
        if (strcmp(value, "process") == 0) service->kill_mode = KILL_MODE_PROCESS;
        else if (strcmp(value, "mixed") == 0) service->kill_mode = KILL_MODE_MIXED;
        else if (strcmp(value, "none") == 0) service->kill_mode = KILL_MODE_NONE;
        else service->kill_mode = KILL_MODE_CONTROL_GROUP;
    } else if (strcmp(key, "KillSignal") == 0) {
        if (strcmp(value, "SIGTERM") == 0) service->kill_signal = SIGTERM;
        else if (strcmp(value, "SIGKILL") == 0) service->kill_signal = SIGKILL;
        else if (strcmp(value, "SIGHUP") == 0) service->kill_signal = SIGHUP;
        else service->kill_signal = SIGTERM;
    } else if (strcmp(key, "FinalKillSignal") == 0) {
        service->final_sigkill_timeout = 10;
    } else if (strcmp(key, "Nice") == 0) {
        service->nice = atoi(value);
    } else if (strcmp(key, "OOMScoreAdjust") == 0) {
        service->oom_score_adj = atoi(value);
    } else if (strcmp(key, "CPUShares") == 0) {
        service->cpu_shares = atoi(value);
    } else if (strcmp(key, "MemoryLimit") == 0) {
        char* end;
        service->memory_limit = strtoull(value, &end, 10);
        if (*end == 'M') service->memory_limit *= 1024 * 1024;
        else if (*end == 'G') service->memory_limit *= 1024 * 1024 * 1024;
    } else if (strcmp(key, "MemoryMax") == 0) {
        char* end;
        service->memory_max = strtoull(value, &end, 10);
        if (*end == 'M') service->memory_max *= 1024 * 1024;
        else if (*end == 'G') service->memory_max *= 1024 * 1024 * 1024;
    } else if (strcmp(key, "MemoryHigh") == 0) {
        char* end;
        service->memory_high = strtoull(value, &end, 10);
        if (*end == 'M') service->memory_high *= 1024 * 1024;
        else if (*end == 'G') service->memory_high *= 1024 * 1024 * 1024;
    } else if (strcmp(key, "MemoryLow") == 0) {
        char* end;
        service->memory_low = strtoull(value, &end, 10);
        if (*end == 'M') service->memory_low *= 1024 * 1024;
        else if (*end == 'G') service->memory_low *= 1024 * 1024 * 1024;
    } else if (strcmp(key, "UMask") == 0) {
        service->umask = strtoul(value, NULL, 8);
    } else if (strcmp(key, "CapabilityBoundingSet") == 0) {
        strncpy(service->capabilities, value, 255);
        service->capability_bounding_set = true;
    } else if (strcmp(key, "AmbientCapabilities") == 0) {
        strncpy(service->ambient_capabilities, value, 255);
        service->ambient_capabilities_enabled = true;
    } else if (strcmp(key, "LimitCPU") == 0) {
        parse_resource_limit(value, service->limit_cpu);
    } else if (strcmp(key, "LimitMemory") == 0) {
        parse_resource_limit(value, service->limit_memory);
    } else if (strcmp(key, "LimitNOFILE") == 0) {
        parse_resource_limit(value, service->limit_nofile);
    } else if (strcmp(key, "LimitNPROC") == 0) {
        parse_resource_limit(value, service->limit_nproc);
    } else if (strcmp(key, "LimitSTACK") == 0) {
        parse_resource_limit(value, service->limit_stack);
    } else if (strcmp(key, "StandardInput") == 0) {
        service->standard_input = (strcmp(value, "null") != 0);
        strncpy(service->standard_input_path, value, SYSTEMD_MAX_PATH_LEN - 1);
    } else if (strcmp(key, "StandardOutput") == 0) {
        service->standard_output = (strcmp(value, "null") != 0);
        strncpy(service->standard_output_path, value, SYSTEMD_MAX_PATH_LEN - 1);
    } else if (strcmp(key, "StandardError") == 0) {
        service->standard_error = (strcmp(value, "null") != 0);
        strncpy(service->standard_error_path, value, SYSTEMD_MAX_PATH_LEN - 1);
    } else if (strcmp(key, "Slice") == 0) {
        strncpy(service->slice, value, SYSTEMD_MAX_NAME - 1);
    } else if (strcmp(key, "RestartPreventExitStatus") == 0) {
        char* token = strtok((char*)value, " ");
        while (token) {
            int status = atoi(token);
            if (status >= 0 && status < 256) {
                service->restart_prevent_exit_status[status] = true;
            }
            token = strtok(NULL, " ");
        }
    } else if (strcmp(key, "StopWhenUnneeded") == 0) {
        service->stop_when_unneeded = (strcmp(value, "yes") == 0);
    } else {
        return parse_exec_section(service, key, value);
    }
    return 0;
}

static char** service_build_envp(service_t* service)
{
    int total_env = service->env_count + 10;
    char** envp = (char**)malloc((total_env + 1) * sizeof(char*));
    if (!envp) return NULL;
    
    int idx = 0;
    
    envp[idx] = (char*)malloc(256);
    if (envp[idx]) {
        sprintf(envp[idx], "PATH=/usr/bin:/bin:/usr/sbin:/sbin");
        idx++;
    }
    
    envp[idx] = (char*)malloc(256);
    if (envp[idx]) {
        sprintf(envp[idx], "HOME=/");
        idx++;
    }
    
    envp[idx] = (char*)malloc(256);
    if (envp[idx]) {
        sprintf(envp[idx], "SHELL=/bin/sh");
        idx++;
    }
    
    envp[idx] = (char*)malloc(256);
    if (envp[idx]) {
        sprintf(envp[idx], "USER=%s", service->user[0] ? service->user : "root");
        idx++;
    }
    
    envp[idx] = (char*)malloc(256);
    if (envp[idx]) {
        sprintf(envp[idx], "LOGNAME=%s", service->user[0] ? service->user : "root");
        idx++;
    }
    
    if (service->watchdog_usec > 0) {
        envp[idx] = (char*)malloc(256);
        if (envp[idx]) {
            sprintf(envp[idx], "WATCHDOG_PID=%llu", service->pid);
            idx++;
        }
        envp[idx] = (char*)malloc(256);
        if (envp[idx]) {
            sprintf(envp[idx], "WATCHDOG_USEC=%llu", service->watchdog_usec);
            idx++;
        }
    }
    
    for (int i = 0; i < service->env_count; i++) {
        envp[idx] = (char*)malloc(SYSTEMD_MAX_NAME + SYSTEMD_MAX_PATH_LEN + 2);
        if (!envp[idx]) {
            for (int j = 0; j < idx; j++) free(envp[j]);
            free(envp);
            return NULL;
        }
        sprintf(envp[idx], "%s=%s", service->env[i].name, service->env[i].value);
        idx++;
    }
    envp[idx] = NULL;
    
    return envp;
}

static void service_free_envp(char** envp)
{
    if (!envp) return;
    for (int i = 0; envp[i]; i++) {
        free(envp[i]);
    }
    free(envp);
}

static int service_create_dynamic_user(service_t* service)
{
    if (!service->dynamic_user) return 0;
    
    service->uid = 1000 + systemd.service_count;
    service->gid = service->uid;
    sprintf(service->user, "dynuser_%d", service->uid);
    sprintf(service->group, "dyncroup_%d", service->gid);
    
    char msg[256];
    sprintf(msg, "Created dynamic user %s (uid=%llu)", service->user, service->uid);
    journal_log(service->base.name, msg, LOG_DEBUG);
    
    return 0;
}

static int service_apply_cgroup(service_t* service)
{
    char cgroup_path[SYSTEMD_MAX_PATH_LEN];
    const char* slice_name = service->slice[0] ? service->slice : "system.slice";
    sprintf(cgroup_path, "/%s/%s", slice_name, service->base.name);
    
    struct cgroup* parent = cgroup_find_by_name("/");
    if (strcmp(slice_name, "system.slice") == 0) {
        parent = cgroup_find_by_name("/system.slice");
    } else if (strcmp(slice_name, "user.slice") == 0) {
        parent = cgroup_find_by_name("/user.slice");
    }
    
    if (!parent) {
        parent = cgroup_find_by_name("/");
        if (!parent) return -1;
    }
    
    struct cgroup* cgrp = cgroup_create(parent, service->base.name);
    if (!cgrp) {
        journal_log(service->base.name, "Failed to create cgroup", LOG_WARNING);
        return -1;
    }
    
    service->cgroup = cgrp;
    
    if (service->cpu_shares > 0) {
        cgrp->cpu_shares = service->cpu_shares;
    }
    if (service->memory_limit > 0 && service->memory_limit != MEM_LIMIT_MAX) {
        cgrp->mem_limit_pages = service->memory_limit / 4096;
    }
    if (service->memory_max > 0) {
        cgrp->mem_limit_pages = service->memory_max / 4096;
    }
    
    char msg[256];
    sprintf(msg, "Created cgroup %s", cgroup_path);
    journal_log(service->base.name, msg, LOG_DEBUG);
    
    return 0;
}

static int service_setup_watchdog(service_t* service)
{
    if (service->watchdog_usec == 0) return 0;
    
    char watchdog_path[SYSTEMD_MAX_PATH_LEN];
    sprintf(watchdog_path, "/run/systemd/watchdog/%s", service->base.name);
    
    vfs_mkdir("/run/systemd/watchdog", 0755);
    
    service->watchdog_sock = unix_socket_create(UNIX_SOCK_DGRAM);
    if (!service->watchdog_sock) {
        journal_log(service->base.name, "Failed to create watchdog socket", LOG_WARNING);
        return -1;
    }
    
    vfs_unlink(watchdog_path);
    
    if (unix_socket_bind(service->watchdog_sock, watchdog_path) != 0) {
        journal_log(service->base.name, "Failed to bind watchdog socket", LOG_WARNING);
        unix_socket_close(service->watchdog_sock);
        service->watchdog_sock = NULL;
        return -1;
    }
    
    char msg[256];
    sprintf(msg, "Watchdog socket created at %s", watchdog_path);
    journal_log(service->base.name, msg, LOG_DEBUG);
    
    service->watchdog_enabled = true;
    service->watchdog_last_ping = time_get_timestamp();
    
    return 0;
}

static void service_monitor_watchdog(void* arg)
{
    service_t* service = (service_t*)arg;
    if (!service || !service->watchdog_sock) return;
    
    char buffer[256];
    while (service->running) {
        if (unix_socket_recvfrom(service->watchdog_sock, buffer, sizeof(buffer), NULL) > 0) {
            spinlock_lock(&systemd.lock);
            service->watchdog_last_ping = time_get_timestamp();
            service->watchdog_ping = true;
            spinlock_unlock(&systemd.lock);
        }
        msleep(100);
    }
}

static int service_check_start_limit(service_t* service)
{
    time_t now = time_get_timestamp();
    
    if (service->start_limit_interval > 0) {
        if (now - service->last_start_time < service->start_limit_interval) {
            service->start_limit_count++;
            if (service->start_limit_count >= service->start_limit_burst) {
                char msg[256];
                sprintf(msg, "Start limit exceeded (%d/%d in %llus)", 
                        service->start_limit_count, service->start_limit_burst, service->start_limit_interval);
                journal_log(service->base.name, msg, LOG_WARNING);
                return -1;
            }
        } else {
            service->start_limit_count = 0;
        }
    }
    
    return 0;
}

int service_run_command(exec_command_t* cmd, const char* working_dir)
{
    if (!cmd || cmd->argc == 0) return -1;
    
    journal_log("systemd", cmd->command, LOG_DEBUG);
    
    uint64_t pid = process_create(cmd->argv[0], (void*)cmd->argv, 0);
    if (pid == 0) {
        journal_log("systemd", "Failed to execute command", LOG_ERR);
        return -1;
    }
    
    int status = 0;
    process_wait(pid, &status, 0);
    
    return WEXITSTATUS(status);
}

static int service_run_exec_commands(exec_command_t* cmds, int count, const char* working_dir)
{
    for (int i = 0; i < count; i++) {
        int ret = service_run_command(&cmds[i], working_dir);
        if (ret != 0) {
            char msg[256];
            sprintf(msg, "Exec command failed with code %d", ret);
            journal_log("systemd", msg, LOG_ERR);
            return ret;
        }
    }
    return 0;
}

int service_load(const char* path)
{
    if (systemd.service_count >= SYSTEMD_MAX_SERVICES) return -1;
    
    service_t* service = &systemd.services[systemd.service_count];
    memset(service, 0, sizeof(service_t));
    INIT_LIST_HEAD(&service->base.unit_list);
    INIT_LIST_HEAD(&service->sockets);
    INIT_LIST_HEAD(&service->cgroup_list);
    INIT_LIST_HEAD(&service->service_list);
    
    strncpy(service->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(service->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    service->base.type = UNIT_TYPE_SERVICE;
    service->base.state = UNIT_STATE_DEAD;
    service->restart = RESTART_NO;
    service->success_exit_status[0] = true;
    service->success_exit_status[143] = true;
    service->kill_mode = KILL_MODE_CONTROL_GROUP;
    service->kill_signal = SIGTERM;
    service->timeout_start_sec = 90;
    service->timeout_stop_sec = 90;
    service->timeout_abort_sec = 0;
    service->cpu_shares = CPU_SHARES_DEFAULT;
    service->memory_limit = MEM_LIMIT_MAX;
    service->memory_max = MEM_LIMIT_MAX;
    service->umask = 0022;
    service->nice = 0;
    service->oom_score_adj = 0;
    
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret > 0) {
        char section[64] = "";
        char key[128], value[1024];
        char* line = content;
        char* next;
        
        while ((next = strchr(line, '\n')) != NULL) {
            *next = '\0';
            trim(line);
            if (*line == '\0') {
                line = next + 1;
                continue;
            }
            if (*line == '#') {
                line = next + 1;
                continue;
            }
            if (line[0] == '[' && parse_ini_section(line, section, sizeof(section)) == 0) {
                line = next + 1;
                continue;
            }
            if (parse_ini_keyvalue(line, key, sizeof(key), value, sizeof(value)) != 0) {
                line = next + 1;
                continue;
            }
            
            if (strcmp(section, "Unit") == 0) {
                parse_unit_section(&service->base, key, value);
            } else if (strcmp(section, "Service") == 0) {
                parse_service_section(service, key, value);
            } else if (strcmp(section, "Install") == 0) {
                parse_install_section(&service->base, key, value);
            }
            
            line = next + 1;
        }
    }
    
    if (service->exec_start_count == 0 && ret <= 0) {
        strncpy(service->base.description, "Built-in Service", SYSTEMD_MAX_DESCRIPTION - 1);
    }
    
    service->restart_sec = (service->restart_sec == 0 && service->restart != RESTART_NO) ? 5 : service->restart_sec;
    service->start_limit_interval = (service->start_limit_interval == 0) ? 10 : service->start_limit_interval;
    service->start_limit_burst = (service->start_limit_burst == 0) ? 5 : service->start_limit_burst;
    
    service->base.load_time = time_get_timestamp();
    
    list_add_tail(&service->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &service->base;
    systemd.service_count++;
    
    journal_log(service->base.name, "Service loaded", LOG_INFO);
    return 0;
}

int service_start(service_t* service)
{
    if (!service) return -1;
    
    spinlock_lock(&systemd.lock);
    
    if (service->running) {
        spinlock_unlock(&systemd.lock);
        return -1;
    }
    
    service->base.state = UNIT_STATE_LOADING;
    spinlock_unlock(&systemd.lock);
    
    if (service_check_start_limit(service) != 0) {
        spinlock_lock(&systemd.lock);
        service->base.state = UNIT_STATE_FAILED;
        spinlock_unlock(&systemd.lock);
        return -1;
    }
    
    service->last_start_time = time_get_timestamp();
    service->start_limit_count++;
    service->base.start_count++;
    
    spinlock_lock(&systemd.lock);
    service->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(service->base.name, "Starting", LOG_INFO);
    
    service_create_dynamic_user(service);
    service_apply_cgroup(service);
    
    if (service->watchdog_usec > 0) {
        service_setup_watchdog(service);
    }
    
    if (service->exec_start_pre_count > 0) {
        int ret = service_run_exec_commands(service->exec_start_pre, service->exec_start_pre_count, service->working_dir);
        if (ret != 0) {
            spinlock_lock(&systemd.lock);
            service->base.state = UNIT_STATE_FAILED;
            service->exit_code = ret;
            service->exit_type = SERVICE_EXIT_FAILURE;
            spinlock_unlock(&systemd.lock);
            journal_log(service->base.name, "Start-pre failed", LOG_ERR);
            return -1;
        }
    }
    
    if (service->exec_start_count > 0) {
        exec_command_t* cmd = &service->exec_start[0];
        
        char** envp = service_build_envp(service);
        
        if (service->type == SERVICE_TYPE_FORKING) {
            uint64_t pid = process_create(cmd->argv[0], (void*)cmd->argv, 0);
            
            if (pid == 0) {
                service_free_envp(envp);
                spinlock_lock(&systemd.lock);
                service->base.state = UNIT_STATE_FAILED;
                service->exit_type = SERVICE_EXIT_FAILURE;
                spinlock_unlock(&systemd.lock);
                journal_log(service->base.name, "Failed to start", LOG_ERR);
                return -1;
            }
            
            msleep(1000);
            
            spinlock_lock(&systemd.lock);
            service->pid = pid;
            service->main_pid = pid;
            service->running = true;
            service->base.state = UNIT_STATE_RUNNING;
            service->base.active_time = time_get_timestamp();
            spinlock_unlock(&systemd.lock);
            
            if (service->cgroup) {
                cgroup_attach_task(service->cgroup, pid);
            }
            
            char msg[256];
            sprintf(msg, "Started with PID %llu (forking)", pid);
            journal_log(service->base.name, msg, LOG_INFO);
            
            if (service->watchdog_sock) {
                thread_create(service_monitor_watchdog, service);
            }
        } else if (service->type == SERVICE_TYPE_ONESHOT) {
            int ret = service_run_command(cmd, service->working_dir);
            
            service_free_envp(envp);
            
            spinlock_lock(&systemd.lock);
            service->running = false;
            service->base.state = (ret == 0) ? UNIT_STATE_EXITED : UNIT_STATE_FAILED;
            service->exit_code = ret;
            service->exit_type = (ret == 0) ? SERVICE_EXIT_SUCCESS : SERVICE_EXIT_FAILURE;
            service->base.active_time = time_get_timestamp();
            service->last_exit_time = time_get_timestamp();
            spinlock_unlock(&systemd.lock);
            
            char msg[256];
            sprintf(msg, "Oneshot service completed with code %d", ret);
            journal_log(service->base.name, msg, LOG_INFO);
            
            if (service->exec_start_post_count > 0) {
                service_run_exec_commands(service->exec_start_post, service->exec_start_post_count, service->working_dir);
            }
            
            return ret;
        } else if (service->type == SERVICE_TYPE_NOTIFY) {
            uint64_t pid = process_create(cmd->argv[0], (void*)cmd->argv, 0);
            
            if (pid == 0) {
                service_free_envp(envp);
                spinlock_lock(&systemd.lock);
                service->base.state = UNIT_STATE_FAILED;
                service->exit_type = SERVICE_EXIT_FAILURE;
                spinlock_unlock(&systemd.lock);
                journal_log(service->base.name, "Failed to start", LOG_ERR);
                return -1;
            }
            
            if (service->cgroup) {
                cgroup_attach_task(service->cgroup, pid);
            }
            
            spinlock_lock(&systemd.lock);
            service->pid = pid;
            service->main_pid = pid;
            service->running = true;
            service->base.state = UNIT_STATE_ACTIVATING;
            service->base.active_time = time_get_timestamp();
            spinlock_unlock(&systemd.lock);
            
            char msg[256];
            sprintf(msg, "Started with PID %llu (notify, waiting)", pid);
            journal_log(service->base.name, msg, LOG_INFO);
            
            if (service->watchdog_sock) {
                thread_create(service_monitor_watchdog, service);
            }
            
            uint64_t timeout = service->timeout_start_sec * 1000;
            uint64_t start = time_get_timestamp() * 1000;
            
            while ((time_get_timestamp() * 1000 - start) < timeout) {
                if (!process_is_running(pid)) {
                    spinlock_lock(&systemd.lock);
                    service->running = false;
                    service->base.state = UNIT_STATE_FAILED;
                    service->exit_type = SERVICE_EXIT_FAILURE;
                    spinlock_unlock(&systemd.lock);
                    journal_log(service->base.name, "Notify service failed before ready", LOG_ERR);
                    service_free_envp(envp);
                    return -1;
                }
                msleep(100);
            }
            
            spinlock_lock(&systemd.lock);
            service->base.state = UNIT_STATE_RUNNING;
            spinlock_unlock(&systemd.lock);
            
            char msg2[256];
            sprintf(msg2, "Notify service ready (PID %llu)", pid);
            journal_log(service->base.name, msg2, LOG_INFO);
            
            service_free_envp(envp);
        } else if (service->type == SERVICE_TYPE_IDLE) {
            msleep(500);
            
            uint64_t pid = process_create(cmd->argv[0], (void*)cmd->argv, 0);
            
            if (pid == 0) {
                service_free_envp(envp);
                spinlock_lock(&systemd.lock);
                service->base.state = UNIT_STATE_FAILED;
                service->exit_type = SERVICE_EXIT_FAILURE;
                spinlock_unlock(&systemd.lock);
                journal_log(service->base.name, "Failed to start", LOG_ERR);
                return -1;
            }
            
            spinlock_lock(&systemd.lock);
            service->pid = pid;
            service->main_pid = pid;
            service->running = true;
            service->base.state = UNIT_STATE_RUNNING;
            service->base.active_time = time_get_timestamp();
            spinlock_unlock(&systemd.lock);
            
            if (service->cgroup) {
                cgroup_attach_task(service->cgroup, pid);
            }
            
            char msg[256];
            sprintf(msg, "Started with PID %llu (idle)", pid);
            journal_log(service->base.name, msg, LOG_INFO);
            
            if (service->watchdog_sock) {
                thread_create(service_monitor_watchdog, service);
            }
            
            service_free_envp(envp);
        } else {
            uint64_t pid = process_create(cmd->argv[0], (void*)cmd->argv, 0);
            
            if (pid == 0) {
                service_free_envp(envp);
                spinlock_lock(&systemd.lock);
                service->base.state = UNIT_STATE_FAILED;
                service->exit_type = SERVICE_EXIT_FAILURE;
                spinlock_unlock(&systemd.lock);
                journal_log(service->base.name, "Failed to start", LOG_ERR);
                return -1;
            }
            
            spinlock_lock(&systemd.lock);
            service->pid = pid;
            service->main_pid = pid;
            service->running = true;
            service->base.state = UNIT_STATE_RUNNING;
            service->base.active_time = time_get_timestamp();
            spinlock_unlock(&systemd.lock);
            
            if (service->cgroup) {
                cgroup_attach_task(service->cgroup, pid);
            }
            
            char msg[256];
            sprintf(msg, "Started with PID %llu", pid);
            journal_log(service->base.name, msg, LOG_INFO);
            
            if (service->watchdog_sock) {
                thread_create(service_monitor_watchdog, service);
            }
            
            service_free_envp(envp);
        }
    } else {
        spinlock_lock(&systemd.lock);
        service->running = true;
        service->base.state = UNIT_STATE_ACTIVE;
        service->base.active_time = time_get_timestamp();
        spinlock_unlock(&systemd.lock);
    }
    
    if (service->exec_start_post_count > 0) {
        service_run_exec_commands(service->exec_start_post, service->exec_start_post_count, service->working_dir);
    }
    
    if (service->watchdog_usec > 0) {
        service->watchdog_last_ping = time_get_timestamp();
        journal_log(service->base.name, "Watchdog enabled", LOG_DEBUG);
    }
    
    return 0;
}

static void service_kill_all_in_cgroup(service_t* service)
{
    if (!service->cgroup) return;
    
    for (int i = 0; i < CGROUP_MAX_PROCS; i++) {
        uint64_t pid = service->cgroup->proc_pids[i];
        if (pid != 0) {
            process_kill(pid, service->kill_signal);
        }
    }
}

int service_stop(service_t* service)
{
    if (!service || !service->running) return -1;
    
    spinlock_lock(&systemd.lock);
    service->stopping = true;
    service->base.state = UNIT_STATE_DEACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(service->base.name, "Stopping", LOG_INFO);
    
    if (service->exec_stop_count > 0) {
        service_run_exec_commands(service->exec_stop, service->exec_stop_count, service->working_dir);
    }
    
    if (service->kill_mode == KILL_MODE_CONTROL_GROUP) {
        service_kill_all_in_cgroup(service);
    } else if (service->kill_mode == KILL_MODE_PROCESS) {
        if (service->pid != 0) {
            process_kill(service->pid, service->kill_signal);
        }
    } else if (service->kill_mode == KILL_MODE_MIXED) {
        if (service->pid != 0) {
            process_kill(service->pid, service->kill_signal);
        }
        if (service->main_pid != 0 && service->main_pid != service->pid) {
            process_kill(service->main_pid, service->kill_signal);
        }
    } else {
    }
    
    uint64_t timeout = service->timeout_stop_sec * 1000;
    uint64_t start = time_get_timestamp() * 1000;
    
    while (service->running) {
        if (!process_is_running(service->pid)) {
            break;
        }
        if ((time_get_timestamp() * 1000 - start) >= timeout) {
            journal_log(service->base.name, "Stop timeout, sending SIGKILL", LOG_WARNING);
            if (service->kill_mode == KILL_MODE_CONTROL_GROUP) {
                for (int i = 0; i < CGROUP_MAX_PROCS; i++) {
                    uint64_t pid = service->cgroup->proc_pids[i];
                    if (pid != 0) {
                        process_kill(pid, SIGKILL);
                    }
                }
            } else {
                process_kill(service->pid, SIGKILL);
            }
            break;
        }
        msleep(100);
    }
    
    if (service->exec_stop_post_count > 0) {
        service_run_exec_commands(service->exec_stop_post, service->exec_stop_post_count, service->working_dir);
    }
    
    if (service->watchdog_sock) {
        unix_socket_close(service->watchdog_sock);
        service->watchdog_sock = NULL;
        service->watchdog_enabled = false;
    }
    
    spinlock_lock(&systemd.lock);
    service->running = false;
    service->stopping = false;
    service->base.state = UNIT_STATE_EXITED;
    service->base.inactive_time = time_get_timestamp();
    service->last_exit_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    journal_log(service->base.name, "Stopped", LOG_INFO);
    
    return 0;
}

int service_kill(service_t* service, int signal)
{
    if (!service || service->pid == 0) return -1;
    
    process_kill(service->pid, signal);
    
    char msg[256];
    sprintf(msg, "Killed with signal %d", signal);
    journal_log(service->base.name, msg, LOG_DEBUG);
    
    return 0;
}

int service_reload(service_t* service)
{
    if (!service || !service->running) return -1;
    
    spinlock_lock(&systemd.lock);
    service->reloading = true;
    spinlock_unlock(&systemd.lock);
    
    journal_log(service->base.name, "Reloading", LOG_INFO);
    
    if (service->exec_reload_count > 0) {
        service_run_exec_commands(service->exec_reload, service->exec_reload_count, service->working_dir);
    } else {
        service_kill(service, SIGHUP);
    }
    
    spinlock_lock(&systemd.lock);
    service->reloading = false;
    spinlock_unlock(&systemd.lock);
    
    journal_log(service->base.name, "Reloaded", LOG_INFO);
    
    return 0;
}

static bool service_should_restart(service_t* service)
{
    if (service->restart_prevent_exit_status[service->exit_code]) {
        return false;
    }
    
    switch (service->restart) {
        case RESTART_ALWAYS:
            return true;
        case RESTART_ON_SUCCESS:
            return (service->exit_type == SERVICE_EXIT_SUCCESS);
        case RESTART_ON_FAILURE:
            return (service->exit_type != SERVICE_EXIT_SUCCESS);
        case RESTART_ON_ABNORMAL:
            return (service->exit_type == SERVICE_EXIT_SIGNALED || 
                    (service->exit_type == SERVICE_EXIT_FAILURE && service->exit_code == 0));
        case RESTART_ON_WATCHDOG:
            return (service->exit_type == SERVICE_EXIT_FAILURE && service->watchdog_usec > 0);
        case RESTART_ON_ABORT:
            return (service->exit_type == SERVICE_EXIT_SIGNALED && 
                    (service->exit_code == SIGABRT || service->exit_code == SIGBUS || 
                     service->exit_code == SIGFPE || service->exit_code == SIGILL || 
                     service->exit_code == SIGSEGV));
        default:
            return false;
    }
}

static bool service_check_watchdog(service_t* service)
{
    if (service->watchdog_usec == 0) return true;
    
    time_t now = time_get_timestamp();
    uint64_t elapsed_ms = (now - service->watchdog_last_ping) * 1000;
    
    if (elapsed_ms > service->watchdog_usec / 1000) {
        char msg[256];
        sprintf(msg, "Watchdog timeout (last ping %llums ago, limit %llums)", elapsed_ms, service->watchdog_usec / 1000);
        journal_log(service->base.name, msg, LOG_WARNING);
        return false;
    }
    
    return true;
}

void service_monitor(service_t* service)
{
    if (!service || !service->running) return;
    
    if (!service_check_watchdog(service)) {
        spinlock_lock(&systemd.lock);
        service->running = false;
        service->base.state = UNIT_STATE_FAILED;
        service->exit_type = SERVICE_EXIT_FAILURE;
        service->last_exit_time = time_get_timestamp();
        spinlock_unlock(&systemd.lock);
        
        journal_log(service->base.name, "Watchdog timeout, stopping", LOG_ERR);
        
        if (service_should_restart(service)) {
            msleep(service->restart_sec * 1000);
            service_start(service);
        }
        return;
    }
    
    if (!process_is_running(service->pid)) {
        spinlock_lock(&systemd.lock);
        service->running = false;
        service->base.state = UNIT_STATE_EXITED;
        service->exit_type = SERVICE_EXIT_FAILURE;
        service->last_exit_time = time_get_timestamp();
        spinlock_unlock(&systemd.lock);
        
        char msg[256];
        sprintf(msg, "Process exited (PID %llu)", service->pid);
        journal_log(service->base.name, msg, LOG_NOTICE);
        
        if (service_should_restart(service)) {
            journal_log(service->base.name, "Restarting automatically", LOG_INFO);
            
            if (service->restart_sec > 0) {
                msleep(service->restart_sec * 1000);
            }
            
            service_start(service);
        }
    }
}