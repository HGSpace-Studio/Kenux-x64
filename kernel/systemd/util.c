#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arch/fs.h>
#include <systemd.h>

vfs_node_t* vfs_find_path(const char* path);

int fs_read_file_content(const char* path, char* buffer, int size)
{
    if (!path || !buffer || size <= 0) return -1;
    
    int fd = vfs_open(path, FS_O_RDONLY, 0);
    if (fd < 0) return -1;
    
    int bytes_read = vfs_read(fd, buffer, size - 1);
    vfs_close(fd);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    }
    
    return bytes_read;
}

int fs_list_dir(const char* path, char* entries, int size)
{
    if (!path || !entries || size <= 0) return -1;
    
    vfs_node_t* node = vfs_find_path(path);
    if (!node || node->type != FS_TYPE_DIRECTORY) return -1;
    
    int pos = 0;
    int index = 0;
    char name[FS_MAX_NAME];
    
    while (1) {
        int ret = node->readdir(node, index++, name, sizeof(name));
        if (ret != 0) break;
        
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        
        int len = strlen(name);
        if (pos + len + 2 >= size) break;
        
        strcpy(entries + pos, name);
        pos += len;
        entries[pos++] = '\n';
    }
    
    entries[pos] = '\0';
    return pos;
}

char* trim_left(char* str)
{
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) str++;
    return str;
}

char* trim_right(char* str)
{
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

char* trim(char* str)
{
    return trim_right(trim_left(str));
}

int parse_ini_section(const char* line, char* section, int size)
{
    if (!line || !section || size <= 0) return -1;
    if (line[0] != '[') return -1;
    
    const char* end = strchr(line, ']');
    if (!end) return -1;
    
    int len = end - line - 1;
    if (len < 0) return -1;
    if (len >= size) len = size - 1;
    
    strncpy(section, line + 1, len);
    section[len] = '\0';
    
    trim(section);
    return 0;
}

int parse_ini_keyvalue(const char* line, char* key, int key_size, char* value, int value_size)
{
    if (!line || !key || !value || key_size <= 0 || value_size <= 0) return -1;
    
    const char* eq = strchr(line, '=');
    if (!eq) return -1;
    
    int key_len = eq - line;
    if (key_len <= 0) return -1;
    if (key_len >= key_size) key_len = key_size - 1;
    
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    trim(key);
    
    const char* value_start = eq + 1;
    int value_len = strlen(value_start);
    if (value_len >= value_size) value_len = value_size - 1;
    
    strncpy(value, value_start, value_len);
    value[value_len] = '\0';
    trim(value);
    
    return 0;
}

int parse_dependency_list(const char* value, dependency_t* deps, int* count, int max)
{
    if (!value || !deps || !count) return -1;
    
    char copy[1024];
    strncpy(copy, value, sizeof(copy) - 1);
    
    char* token = strtok(copy, " ");
    while (token) {
        if (*count >= max) break;
        
        trim(token);
        strncpy(deps[*count].name, token, SYSTEMD_MAX_NAME - 1);
        deps[*count].type = UNIT_TYPE_SERVICE;
        deps[*count].satisfied = 0;
        (*count)++;
        
        token = strtok(NULL, " ");
    }
    
    return 0;
}