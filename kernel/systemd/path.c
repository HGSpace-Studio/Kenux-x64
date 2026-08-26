#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <arch/fs.h>

static int parse_path_config(const char* path, systemd_path_t* path_unit)
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
        
        if (strcmp(section, "Path") == 0) {
            if (strcmp(key, "PathExists") == 0) {
                strncpy(path_unit->path, value, SYSTEMD_MAX_PATH_LEN - 1);
                path_unit->exists = true;
            } else if (strcmp(key, "PathExistsGlob") == 0) {
                strncpy(path_unit->path, value, SYSTEMD_MAX_PATH_LEN - 1);
                path_unit->exists_glob = true;
            } else if (strcmp(key, "PathChanged") == 0) {
                strncpy(path_unit->path, value, SYSTEMD_MAX_PATH_LEN - 1);
                path_unit->changed = true;
            } else if (strcmp(key, "PathModified") == 0) {
                strncpy(path_unit->path, value, SYSTEMD_MAX_PATH_LEN - 1);
                path_unit->modified = true;
            } else if (strcmp(key, "Unit") == 0) {
                strncpy(path_unit->service_name, value, SYSTEMD_MAX_NAME - 1);
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&path_unit->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&path_unit->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int path_load(const char* path)
{
    if (systemd.path_count >= SYSTEMD_MAX_PATH) return -1;
    
    systemd_path_t* path_unit = &systemd.paths[systemd.path_count];
    memset(path_unit, 0, sizeof(systemd_path_t));
    INIT_LIST_HEAD(&path_unit->base.unit_list);
    
    strncpy(path_unit->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(path_unit->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_path_config(path, path_unit);
    
    path_unit->base.type = UNIT_TYPE_PATH;
    path_unit->base.state = UNIT_STATE_DEAD;
    path_unit->base.load_time = time_get_timestamp();
    
    list_add_tail(&path_unit->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &path_unit->base;
    systemd.path_count++;
    
    journal_log(path_unit->base.name, "Path unit loaded", LOG_INFO);
    
    if (path_unit->base.enabled) {
        path_start(path_unit);
    }
    
    return 0;
}

int path_start(systemd_path_t* path_unit)
{
    if (!path_unit) return -1;
    
    spinlock_lock(&systemd.lock);
    path_unit->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(path_unit->base.name, "Watching path", LOG_INFO);
    
    char msg[256];
    sprintf(msg, "Watching path %s", path_unit->path);
    journal_log(path_unit->base.name, msg, LOG_INFO);
    
    spinlock_lock(&systemd.lock);
    path_unit->base.state = UNIT_STATE_ACTIVE;
    path_unit->base.active_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    return 0;
}