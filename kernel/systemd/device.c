#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

static device_type_t parse_device_type(const char* type)
{
    if (strcmp(type, "block") == 0) return DEVICE_TYPE_BLOCK;
    if (strcmp(type, "char") == 0) return DEVICE_TYPE_CHAR;
    return DEVICE_TYPE_BLOCK;
}

static int parse_device_config(const char* path, device_t* device)
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
        
        if (strcmp(section, "Device") == 0) {
            if (strcmp(key, "DevicePath") == 0) {
                strncpy(device->devpath, value, SYSTEMD_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "DeviceType") == 0) {
                device->device_type = parse_device_type(value);
            } else if (strcmp(key, "SysName") == 0) {
                strncpy(device->sysname, value, 63);
            } else if (strcmp(key, "DevName") == 0) {
                strncpy(device->devname, value, 63);
            } else if (strcmp(key, "SubSystem") == 0) {
                strncpy(device->subsystem, value, 63);
            } else if (strcmp(key, "Driver") == 0) {
                strncpy(device->driver, value, 63);
            } else if (strcmp(key, "Major") == 0) {
                device->major = atoi(value);
            } else if (strcmp(key, "Minor") == 0) {
                device->minor = atoi(value);
            }
        } else if (strcmp(section, "Unit") == 0) {
            parse_unit_section(&device->base, key, value);
        } else if (strcmp(section, "Install") == 0) {
            parse_install_section(&device->base, key, value);
        }
        
        line = next + 1;
    }
    
    return 0;
}

int device_load(const char* path)
{
    if (systemd.device_count >= SYSTEMD_MAX_DEVICE) return -1;
    
    device_t* device = &systemd.devices[systemd.device_count];
    memset(device, 0, sizeof(device_t));
    INIT_LIST_HEAD(&device->base.unit_list);
    
    strncpy(device->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(device->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    parse_device_config(path, device);
    
    device->base.type = UNIT_TYPE_DEVICE;
    device->base.state = UNIT_STATE_DEAD;
    device->base.load_time = time_get_timestamp();
    
    list_add_tail(&device->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &device->base;
    systemd.device_count++;
    
    journal_log(device->base.name, "Device unit loaded", LOG_INFO);
    
    if (device->base.enabled) {
        device_start(device);
    }
    
    return 0;
}

int device_start(device_t* device)
{
    if (!device) return -1;
    
    spinlock_lock(&systemd.lock);
    device->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(device->base.name, "Starting device", LOG_INFO);
    
    char msg[256];
    sprintf(msg, "Device %s (%s) started", device->sysname, device->subsystem);
    journal_log(device->base.name, msg, LOG_INFO);
    
    spinlock_lock(&systemd.lock);
    device->base.state = UNIT_STATE_ACTIVE;
    device->base.active_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    return 0;
}