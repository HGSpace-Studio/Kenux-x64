

#include "devtmpfs.h"
#include <arch/memory.h>
#include <string.h>
#include <slab.h>

static devtmpfs_entry_t dev_table[DEVTMPFS_MAX];
static uint32_t dev_count = 0;
static vfs_node_t* devtmpfs_root = NULL;

void devtmpfs_init(void)
{
    memset(dev_table, 0, sizeof(dev_table));
    dev_count = 0;
    devtmpfs_register_defaults();
}

int devtmpfs_register(const char* name, uint32_t type, uint32_t major, uint32_t minor)
{
    if (!name || dev_count >= DEVTMPFS_MAX) return -1;

    for (uint32_t i = 0; i < dev_count; i++) {
        if (strcmp(dev_table[i].name, name) == 0) {
            dev_table[i].major = major;
            dev_table[i].minor = minor;
            return 0;
        }
    }

    devtmpfs_entry_t* entry = &dev_table[dev_count++];
    strncpy(entry->name, name, DEV_NAME_MAX - 1);
    entry->type = type;
    entry->major = major;
    entry->minor = minor;
    entry->mode = (type == DEV_TYPE_CHAR) ? 0600 : 0600;
    entry->ref_count = 1;

    if (devtmpfs_root) {
        vfs_node_t* node = vfs_create_node(name, FS_TYPE_REGULAR);
        if (node) {

            uint32_t* devnum = (uint32_t*)kzalloc(8);
            if (devnum) {
                devnum[0] = (major << 16) | minor;
                devnum[1] = type;
                node->impl_data = devnum;
            }
            vfs_add_child(devtmpfs_root, node);
        }
    }

    return 0;
}

int devtmpfs_unregister(const char* name)
{
    if (!name) return -1;
    for (uint32_t i = 0; i < dev_count; i++) {
        if (strcmp(dev_table[i].name, name) == 0) {
            dev_table[i] = dev_table[dev_count - 1];
            memset(&dev_table[dev_count - 1], 0, sizeof(devtmpfs_entry_t));
            dev_count--;
            return 0;
        }
    }
    return -1;
}

devtmpfs_entry_t* devtmpfs_find(const char* name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < dev_count; i++) {
        if (strcmp(dev_table[i].name, name) == 0) return &dev_table[i];
    }
    return NULL;
}

devtmpfs_entry_t* devtmpfs_find_by_dev(uint32_t type, uint32_t major, uint32_t minor)
{
    for (uint32_t i = 0; i < dev_count; i++) {
        if (dev_table[i].type == type &&
            dev_table[i].major == major &&
            dev_table[i].minor == minor) {
            return &dev_table[i];
        }
    }
    return NULL;
}

vfs_node_t* devtmpfs_get_root(void)
{
    if (!devtmpfs_root) {
        devtmpfs_root = vfs_create_node("dev", FS_TYPE_DIRECTORY);
    }
    return devtmpfs_root;
}

void devtmpfs_register_defaults(void)
{
    devtmpfs_register("null",  DEV_TYPE_CHAR, 1, 3);
    devtmpfs_register("zero",  DEV_TYPE_CHAR, 1, 5);
    devtmpfs_register("full",  DEV_TYPE_CHAR, 1, 7);
    devtmpfs_register("random", DEV_TYPE_CHAR, 1, 8);
    devtmpfs_register("urandom", DEV_TYPE_CHAR, 1, 9);
    devtmpfs_register("tty",   DEV_TYPE_CHAR, 5, 0);
    devtmpfs_register("console", DEV_TYPE_CHAR, 5, 1);
    devtmpfs_register("sda",   DEV_TYPE_BLOCK, 8, 0);
    devtmpfs_register("sda1",  DEV_TYPE_BLOCK, 8, 1);
    devtmpfs_register("sda2",  DEV_TYPE_BLOCK, 8, 2);
    devtmpfs_register("hda",   DEV_TYPE_BLOCK, 3, 0);
    devtmpfs_register("hda1",  DEV_TYPE_BLOCK, 3, 1);
    devtmpfs_register("loop0", DEV_TYPE_BLOCK, 7, 0);
    devtmpfs_register("loop1", DEV_TYPE_BLOCK, 7, 1);
    devtmpfs_register("loop2", DEV_TYPE_BLOCK, 7, 2);
}
