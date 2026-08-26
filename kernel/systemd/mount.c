#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

static mount_flags_t parse_mount_flags(const char* options)
{
    mount_flags_t flags = 0;
    char* opt = (char*)options;
    char* token;
    
    while ((token = strtok(opt, ",")) != NULL) {
        if (strcmp(token, "ro") == 0) flags |= MOUNT_FLAGS_READ_ONLY;
        else if (strcmp(token, "noexec") == 0) flags |= MOUNT_FLAGS_NOEXEC;
        else if (strcmp(token, "nodev") == 0) flags |= MOUNT_FLAGS_NODEV;
        else if (strcmp(token, "nosuid") == 0) flags |= MOUNT_FLAGS_NOSUID;
        else if (strcmp(token, "noatime") == 0) flags |= MOUNT_FLAGS_NOATIME;
        else if (strcmp(token, "relatime") == 0) flags |= MOUNT_FLAGS_RELATIME;
        opt = NULL;
    }
    
    return flags;
}

static int parse_mount_config(const char* path, mount_t* mount)
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
        
        if (strcmp(section, "Mount") == 0) {
            if (strcmp(key, "What") == 0) {
                strncpy(mount->what, value, SYSTEMD_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "Where") == 0) {
                strncpy(mount->where, value, SYSTEMD_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "Type") == 0) {
                strncpy(mount->type, value, 63);
            } else if (strcmp(key, "Options") == 0) {
                strncpy(mount->options, value, 1023);
                mount->mount_flags = parse_mount_flags(value);
            } else if (strcmp(key, "Dump") == 0) {
                mount->dump_freq = atoi(value);
            } else if (strcmp(key, "PassNo") == 0) {
                mount->pass_num = atoi(value);
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&mount->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&mount->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int mount_load(const char* path)
{
    if (systemd.mount_count >= SYSTEMD_MAX_MOUNTS) return -1;
    
    mount_t* mount = &systemd.mounts[systemd.mount_count];
    memset(mount, 0, sizeof(mount_t));
    INIT_LIST_HEAD(&mount->base.unit_list);
    
    strncpy(mount->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(mount->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_mount_config(path, mount);
    
    mount->base.type = UNIT_TYPE_MOUNT;
    mount->base.state = UNIT_STATE_DEAD;
    mount->base.load_time = time_get_timestamp();
    
    list_add_tail(&mount->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &mount->base;
    systemd.mount_count++;
    
    journal_log(mount->base.name, "Mount unit loaded", LOG_INFO);
    
    if (mount->base.enabled) {
        mount_start(mount);
    }
    
    return 0;
}

int mount_start(mount_t* mount)
{
    if (!mount) return -1;
    
    spinlock_lock(&systemd.lock);
    mount->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(mount->base.name, "Mounting", LOG_INFO);
    
    vfs_mkdir(mount->where, 0755);
    
    int ret = vfs_mount(mount->what, mount->where, mount->type);
    
    spinlock_lock(&systemd.lock);
    if (ret == 0) {
        mount->base.state = UNIT_STATE_ACTIVE;
        mount->base.active_time = time_get_timestamp();
        mount->mount_result = 0;
        
        char msg[256];
        sprintf(msg, "Mounted %s on %s", mount->what, mount->where);
        journal_log(mount->base.name, msg, LOG_INFO);
    } else {
        mount->base.state = mount->nofail ? UNIT_STATE_DEAD : UNIT_STATE_FAILED;
        mount->mount_result = ret;
        
        char msg[256];
        sprintf(msg, "Failed to mount %s (%d)", mount->what, ret);
        journal_log(mount->base.name, msg, mount->nofail ? LOG_WARNING : LOG_ERR);
    }
    spinlock_unlock(&systemd.lock);
    
    return ret;
}

int mount_stop(mount_t* mount)
{
    if (!mount) return -1;
    
    spinlock_lock(&systemd.lock);
    mount->base.state = UNIT_STATE_DEACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(mount->base.name, "Unmounting", LOG_INFO);
    
    int ret = vfs_umount(mount->where);
    
    spinlock_lock(&systemd.lock);
    mount->base.state = UNIT_STATE_DEAD;
    mount->base.inactive_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    journal_log(mount->base.name, "Unmounted", LOG_INFO);
    
    return ret;
}