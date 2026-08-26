#include <systemd.h>
#include <string.h>
#include <stdio.h>

static int parse_swap_config(const char* path, swap_t* swap)
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
        
        if (strcmp(section, "Swap") == 0) {
            if (strcmp(key, "What") == 0) {
                strncpy(swap->what, value, SYSTEMD_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "Options") == 0) {
                strncpy(swap->options, value, 1023);
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&swap->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&swap->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int swap_load(const char* path)
{
    if (systemd.swap_count >= SYSTEMD_MAX_SWAP) return -1;
    
    swap_t* swap = &systemd.swaps[systemd.swap_count];
    memset(swap, 0, sizeof(swap_t));
    INIT_LIST_HEAD(&swap->base.unit_list);
    
    strncpy(swap->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(swap->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_swap_config(path, swap);
    
    swap->base.type = UNIT_TYPE_SWAP;
    swap->base.state = UNIT_STATE_DEAD;
    swap->base.load_time = time_get_timestamp();
    
    list_add_tail(&swap->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &swap->base;
    systemd.swap_count++;
    
    journal_log(swap->base.name, "Swap unit loaded", LOG_INFO);
    
    if (swap->base.enabled) {
        swap_start(swap);
    }
    
    return 0;
}

int swap_start(swap_t* swap)
{
    if (!swap) return -1;
    
    spinlock_lock(&systemd.lock);
    swap->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(swap->base.name, "Activating swap", LOG_INFO);
    
    char msg[256];
    sprintf(msg, "Swap %s activated", swap->what);
    journal_log(swap->base.name, msg, LOG_INFO);
    
    spinlock_lock(&systemd.lock);
    swap->base.state = UNIT_STATE_ACTIVE;
    swap->base.active_time = time_get_timestamp();
    swap->swapon_done = true;
    spinlock_unlock(&systemd.lock);
    
    return 0;
}