#ifndef FS_H
#define FS_H

#include <arch/types.h>

#define FS_MAX 16
#define PATH_MAX 256

typedef struct filesystem {
    const char* name;
    int (*mount)(const char* device, const char* mount_point);
    int (*unmount)(const char* mount_point);
} filesystem_t;

void fs_init(void);
int fs_register(filesystem_t* fs);
int fs_mount(const char* fs_name, const char* device, const char* mount_point);

#endif
