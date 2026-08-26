

#ifndef KERNEL_DEVTMPFS_H
#define KERNEL_DEVTMPFS_H

#include <arch/types.h>
#include <arch/fs.h>

#define DEV_NAME_MAX   64
#define DEVTMPFS_MAX   128

#define DEV_TYPE_CHAR   0
#define DEV_TYPE_BLOCK  1
#define DEV_TYPE_MISC   2

typedef struct devtmpfs_entry {
    char     name[DEV_NAME_MAX];
    uint32_t type;
    uint32_t major;
    uint32_t minor;
    uint32_t mode;
    uint32_t ref_count;
} devtmpfs_entry_t;

void devtmpfs_init(void);

int devtmpfs_register(const char* name, uint32_t type, uint32_t major, uint32_t minor);
int devtmpfs_unregister(const char* name);

devtmpfs_entry_t* devtmpfs_find(const char* name);
devtmpfs_entry_t* devtmpfs_find_by_dev(uint32_t type, uint32_t major, uint32_t minor);

vfs_node_t* devtmpfs_get_root(void);

void devtmpfs_register_defaults(void);

#endif
