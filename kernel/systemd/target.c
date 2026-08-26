#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <arch/fs.h>

static void resolve_dependencies(unit_t* unit)
{
    for (int i = 0; i < unit->after_count; i++) {
        unit_t* dep = find_unit(unit->after[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD && dep->enabled) {
            if (dep->type == UNIT_TYPE_TARGET) {
                target_start((target_t*)dep);
            } else if (dep->type == UNIT_TYPE_SERVICE) {
                service_start((service_t*)dep);
            }
        }
    }
}

static int parse_target_section(target_t* target, const char* key, const char* value)
{
    if (strcmp(key, "Requires") == 0) {
        parse_dependency_list(value, target->base.requires, &target->base.requires_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "Wants") == 0) {
        parse_dependency_list(value, target->base.wants, &target->base.wants_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "Before") == 0) {
        parse_dependency_list(value, target->base.before, &target->base.before_count, SYSTEMD_MAX_DEPENDENCIES);
    } else if (strcmp(key, "After") == 0) {
        parse_dependency_list(value, target->base.after, &target->base.after_count, SYSTEMD_MAX_DEPENDENCIES);
    }
    return 0;
}

int target_load(const char* path)
{
    if (systemd.target_count >= SYSTEMD_MAX_TARGETS) return -1;
    
    target_t* target = &systemd.targets[systemd.target_count];
    memset(target, 0, sizeof(target_t));
    INIT_LIST_HEAD(&target->base.unit_list);
    INIT_LIST_HEAD(&target->services);
    
    strncpy(target->base.source_path, path, SYSTEMD_MAX_PATH - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(target->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    target->base.type = UNIT_TYPE_TARGET;
    target->base.state = UNIT_STATE_DEAD;
    
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
            if (*line == '\0' || *line == '#') {
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
                if (strcmp(key, "Description") == 0) {
                    strncpy(target->base.description, value, SYSTEMD_MAX_DESCRIPTION - 1);
                } else {
                    parse_target_section(target, key, value);
                }
            } else if (strcmp(section, "Install") == 0) {
                if (strcmp(key, "WantedBy") == 0) {
                    target->base.enabled = true;
                }
            }
            
            line = next + 1;
        }
    }
    
    target->base.load_time = time_get_timestamp();
    
    list_add_tail(&target->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &target->base;
    systemd.target_count++;
    
    journal_log(target->base.name, "Target loaded", LOG_INFO);
    return 0;
}

int target_start(target_t* target)
{
    if (!target) return -1;
    
    spinlock_lock(&systemd.lock);
    target->base.state = UNIT_STATE_LOADING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(target->base.name, "Starting target", LOG_INFO);
    
    resolve_dependencies(&target->base);
    
    for (int i = 0; i < target->base.requires_count; i++) {
        unit_t* dep = find_unit(target->base.requires[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD) {
            if (dep->type == UNIT_TYPE_SERVICE) {
                service_start((service_t*)dep);
            } else if (dep->type == UNIT_TYPE_TARGET) {
                target_start((target_t*)dep);
            } else if (dep->type == UNIT_TYPE_MOUNT) {
                mount_start((mount_t*)dep);
            }
        }
    }
    
    for (int i = 0; i < target->base.wants_count; i++) {
        unit_t* dep = find_unit(target->base.wants[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD && dep->enabled) {
            if (dep->type == UNIT_TYPE_SERVICE) {
                service_start((service_t*)dep);
            } else if (dep->type == UNIT_TYPE_TARGET) {
                target_start((target_t*)dep);
            }
        }
    }
    
    for (int i = 0; i < systemd.service_count; i++) {
        service_t* service = &systemd.services[i];
        if (service->base.state == UNIT_STATE_DEAD && service->base.enabled) {
            for (int j = 0; j < service->base.wants_count; j++) {
                if (strcmp(service->base.wants[j].name, target->base.name) == 0) {
                    service_start(service);
                    break;
                }
            }
            for (int j = 0; j < service->base.requires_count; j++) {
                if (strcmp(service->base.requires[j].name, target->base.name) == 0) {
                    service_start(service);
                    break;
                }
            }
        }
    }
    
    spinlock_lock(&systemd.lock);
    target->base.state = UNIT_STATE_ACTIVE;
    target->base.active_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    char msg[256];
    sprintf(msg, "Target %s reached", target->base.name);
    journal_log("systemd", msg, LOG_INFO);
    
    return 0;
}