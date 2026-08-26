

#ifndef KERNEL_SYSFS_H
#define KERNEL_SYSFS_H

#include <arch/types.h>
#include <arch/fs.h>

#define SYSFS_NAME_MAX  64
#define SYSFS_MAX_ATTRS 16

typedef struct sysfs_attr {
    char name[SYSFS_NAME_MAX];
    char value[256];
    int  (*show)(char* buf, uint64_t max);
    int  (*store)(const char* buf, uint64_t len);
    int  is_dynamic;
} sysfs_attr_t;

typedef struct sysfs_node {
    char name[SYSFS_NAME_MAX];
    struct sysfs_node* children[16];
    uint32_t child_count;
    sysfs_attr_t attrs[SYSFS_MAX_ATTRS];
    uint32_t attr_count;
    struct sysfs_node* parent;
} sysfs_node_t;

void sysfs_init(void);

sysfs_node_t* sysfs_create_dir(sysfs_node_t* parent, const char* name);

int sysfs_create_file(sysfs_node_t* dir, const char* name,
                       int (*show)(char*, uint64_t), int (*store)(const char*, uint64_t));

int sysfs_set_attr(sysfs_node_t* dir, const char* name, const char* value);

vfs_node_t* sysfs_get_root(void);

sysfs_node_t* sysfs_get_kernel_dir(void);
sysfs_node_t* sysfs_get_devices_dir(void);

#endif
