#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

static int fs_mount(const char* fs_name, const char* device, const char* mount_point)
{
    (void)fs_name; (void)device; (void)mount_point;
    journal_log("fstab", "fs_mount not implemented", LOG_WARNING);
    return -1;
}

static int parse_fstab_line(const char* line, fstab_entry_t* entry)
{
    if (!line || !entry) return -1;
    
    char copy[1024];
    strncpy(copy, line, sizeof(copy) - 1);
    
    char* token = strtok(copy, " \t");
    if (!token) return -1;
    strncpy(entry->source, token, SYSTEMD_MAX_PATH - 1);
    
    token = strtok(NULL, " \t");
    if (!token) return -1;
    strncpy(entry->destination, token, SYSTEMD_MAX_PATH - 1);
    
    token = strtok(NULL, " \t");
    if (!token) return -1;
    strncpy(entry->fstype, token, 31);
    
    token = strtok(NULL, " \t");
    if (!token) return -1;
    strncpy(entry->options, token, 127);
    
    token = strtok(NULL, " \t");
    if (!token) return -1;
    entry->dump_freq = atoi(token);
    
    token = strtok(NULL, " \t");
    if (!token) return -1;
    entry->pass_num = atoi(token);
    
    return 0;
}

int fstab_load(const char* path)
{
    systemd.fstab_count = 0;
    
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret <= 0) {
        journal_log("systemd", "fstab not found, using defaults", LOG_NOTICE);
        fstab_entry_t* entry = &systemd.fstab[systemd.fstab_count++];
        memset(entry, 0, sizeof(fstab_entry_t));
        strcpy(entry->source, "tmpfs");
        strcpy(entry->destination, "/tmp");
        strcpy(entry->fstype, "tmpfs");
        strcpy(entry->options, "mode=1777");
        entry->dump_freq = 0;
        entry->pass_num = 0;
        
        entry = &systemd.fstab[systemd.fstab_count++];
        memset(entry, 0, sizeof(fstab_entry_t));
        strcpy(entry->source, "proc");
        strcpy(entry->destination, "/proc");
        strcpy(entry->fstype, "proc");
        strcpy(entry->options, "defaults");
        entry->dump_freq = 0;
        entry->pass_num = 0;
        
        journal_log("systemd", "fstab loaded (defaults)", LOG_INFO);
        return 0;
    }
    
    char* line = content;
    char* next;
    
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        trim(line);
        
        if (*line == '\0' || *line == '#') {
            line = next + 1;
            continue;
        }
        
        if (systemd.fstab_count >= 128) {
            journal_log("systemd", "fstab entry limit reached", LOG_WARNING);
            break;
        }
        
        fstab_entry_t* entry = &systemd.fstab[systemd.fstab_count];
        memset(entry, 0, sizeof(fstab_entry_t));
        
        if (parse_fstab_line(line, entry) == 0) {
            systemd.fstab_count++;
        }
        
        line = next + 1;
    }
    
    journal_log("systemd", "fstab loaded", LOG_INFO);
    return 0;
}

int fstab_mount_all(void)
{
    journal_log("systemd", "Mounting filesystems", LOG_INFO);
    
    for (int i = 0; i < systemd.fstab_count; i++) {
        fstab_entry_t* entry = &systemd.fstab[i];
        
        vfs_mkdir(entry->destination, 0755);
        
        int ret = vfs_mount(entry->source, entry->destination, entry->fstype);
        if (ret != 0) {
            ret = fs_mount(entry->fstype, entry->source, entry->destination);
        }
        
        if (ret == 0) {
            char msg[256];
            sprintf(msg, "Mounted %s on %s", entry->source, entry->destination);
            journal_log("systemd", msg, LOG_INFO);
        } else {
            char msg[256];
            sprintf(msg, "Failed to mount %s on %s", entry->source, entry->destination);
            journal_log("systemd", msg, LOG_WARNING);
        }
    }
    
    return 0;
}