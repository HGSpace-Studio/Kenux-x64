#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

static automount_type_t parse_automount_type(const char* type)
{
    if (strcmp(type, "fstab") == 0) return AUTOMOUNT_TYPE_FSTAB;
    return AUTOMOUNT_TYPE_PATH;
}

static int parse_automount_config(const char* path, automount_t* automount)
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
        
        if (strcmp(section, "Automount") == 0) {
            if (strcmp(key, "Where") == 0) {
                strncpy(automount->where, value, SYSTEMD_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "DirectoryMode") == 0) {
                strncpy(automount->directory_mode, value, 15);
            } else if (strcmp(key, "TimeoutIdleSec") == 0) {
                strncpy(automount->timeout_idle_sec, value, 63);
                automount->timeout_usec = (uint64_t)atoi(value) * 1000000;
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&automount->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&automount->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int automount_load(const char* path)
{
    if (systemd.automount_count >= SYSTEMD_MAX_AUTOMOUNT) return -1;
    
    automount_t* automount = &systemd.automounts[systemd.automount_count];
    memset(automount, 0, sizeof(automount_t));
    INIT_LIST_HEAD(&automount->base.unit_list);
    
    strncpy(automount->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(automount->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_automount_config(path, automount);
    
    automount->base.type = UNIT_TYPE_AUTOMOUNT;
    automount->base.state = UNIT_STATE_DEAD;
    automount->base.load_time = time_get_timestamp();
    
    list_add_tail(&automount->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &automount->base;
    systemd.automount_count++;
    
    journal_log(automount->base.name, "Automount unit loaded", LOG_INFO);
    
    if (automount->base.enabled) {
        automount_start(automount);
    }
    
    return 0;
}

int automount_start(automount_t* automount)
{
    if (!automount) return -1;
    
    spinlock_lock(&systemd.lock);
    automount->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(automount->base.name, "Setting up automount", LOG_INFO);
    
    vfs_mkdir(automount->where, 0755);
    
    char msg[256];
    sprintf(msg, "Automount set up at %s", automount->where);
    journal_log(automount->base.name, msg, LOG_INFO);
    
    spinlock_lock(&systemd.lock);
    automount->base.state = UNIT_STATE_ACTIVE;
    automount->base.active_time = time_get_timestamp();
    automount->active = true;
    spinlock_unlock(&systemd.lock);
    
    return 0;
}