#ifndef KERNEL_FS_H
#define KERNEL_FS_H

#include <arch/types.h>

#define FS_MAX 16
#define FS_NAME_MAX 32
#define FS_PATH_MAX 256

typedef struct {
    char name[FS_NAME_MAX];
    int (*mount)(const char* device, const char* mount_point);
    int (*unmount)(const char* mount_point);
} filesystem_t;

void fs_init(void);
int fs_register(filesystem_t* fs);
int fs_mount(const char* fs_name, const char* device, const char* mount_point);
int fs_unmount(const char* mount_point);

void fs_list_directory(const char* path);
void fs_read_file(const char* filename);
void fs_change_directory(const char* path);
void fs_create_directory(const char* path);
void fs_remove_file(const char* path);
void fs_copy_file(const char* src, const char* dst);
void fs_move_file(const char* src, const char* dst);
void fs_create_file(const char* filename);
void fs_change_permissions(const char* path, int mode);

#endif
