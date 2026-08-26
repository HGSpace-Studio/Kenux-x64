#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cgroup.h>

static slice_type_t parse_slice_type(const char* name)
{
    if (strstr(name, "user") != NULL) return SLICE_TYPE_USER;
    if (strstr(name, "machine") != NULL) return SLICE_TYPE_MACHINE;
    return SLICE_TYPE_SYSTEM;
}

static int parse_slice_config(const char* path, slice_t* slice)
{
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret <= 0) {
        return -1;
    }
    
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
        
        if (strcmp(section, "Slice") == 0) {
            if (strcmp(key, "CPUWeight") == 0) {
                slice->cpu_weight = atoi(value);
            } else if (strcmp(key, "IOWeight") == 0) {
                slice->io_weight = atoi(value);
            } else if (strcmp(key, "MemoryWeight") == 0) {
                slice->memory_weight = atoi(value);
            } else if (strcmp(key, "CPUMax") == 0) {
                slice->cpu_max = atoi(value);
            } else if (strcmp(key, "IOMax") == 0) {
                slice->io_max = atoi(value);
            } else if (strcmp(key, "MemoryMax") == 0) {
                slice->memory_max = atoi(value);
            } else if (strcmp(key, "AllowKill") == 0) {
                slice->allow_kill = (strcmp(value, "yes") == 0);
            } else if (strcmp(key, "Delegate") == 0) {
                slice->delegate = (strcmp(value, "yes") == 0);
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&slice->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&slice->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int slice_load(const char* path)
{
    if (systemd.slice_count >= SYSTEMD_MAX_SLICE) return -1;
    
    slice_t* slice = &systemd.slices[systemd.slice_count];
    memset(slice, 0, sizeof(slice_t));
    INIT_LIST_HEAD(&slice->base.unit_list);
    INIT_LIST_HEAD(&slice->children);
    INIT_LIST_HEAD(&slice->tasks);
    
    strncpy(slice->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(slice->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_slice_config(path, slice);
    
    slice->type = parse_slice_type(slice->base.name);
    slice->base.type = UNIT_TYPE_SLICE;
    slice->base.state = UNIT_STATE_DEAD;
    slice->base.load_time = time_get_timestamp();
    
    list_add_tail(&slice->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &slice->base;
    systemd.slice_count++;
    
    journal_log(slice->base.name, "Slice unit loaded", LOG_INFO);
    
    if (slice->base.enabled) {
        slice_start(slice);
    }
    
    return 0;
}

int slice_start(slice_t* slice)
{
    if (!slice) return -1;
    
    spinlock_lock(&systemd.lock);
    slice->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(slice->base.name, "Creating slice", LOG_INFO);
    
    char cgroup_path[SYSTEMD_MAX_PATH_LEN];
    sprintf(cgroup_path, "/sys/fs/cgroup/%s", slice->base.name);
    
    struct cgroup* parent = cgroup_find_by_name("/");
    slice->cgroup = cgroup_create(parent, slice->base.name);
    
    if (slice->cgroup) {
        slice->cgroup->cpu_shares = slice->cpu_weight;
        
        spinlock_lock(&systemd.lock);
        slice->base.state = UNIT_STATE_ACTIVE;
        slice->base.active_time = time_get_timestamp();
        spinlock_unlock(&systemd.lock);
        
        char msg[256];
        sprintf(msg, "Created slice %s", cgroup_path);
        journal_log(slice->base.name, msg, LOG_INFO);
    } else {
        spinlock_lock(&systemd.lock);
        slice->base.state = UNIT_STATE_FAILED;
        spinlock_unlock(&systemd.lock);
        
        journal_log(slice->base.name, "Failed to create slice cgroup", LOG_ERR);
        return -1;
    }
    
    return 0;
}