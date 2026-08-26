

#include "sysfs.h"
#include <arch/memory.h>
#include <string.h>
#include <slab.h>
#include <stdio.h>

static sysfs_node_t sysfs_root;
static sysfs_node_t* sysfs_kernel;
static sysfs_node_t* sysfs_devices;
static vfs_node_t* sysfs_vfs_root = NULL;

static sysfs_node_t* __sysfs_alloc_node(const char* name)
{
    sysfs_node_t* node = (sysfs_node_t*)kzalloc(sizeof(sysfs_node_t));
    if (!node) return NULL;
    strncpy(node->name, name, SYSFS_NAME_MAX - 1);
    return node;
}

void sysfs_init(void)
{
    memset(&sysfs_root, 0, sizeof(sysfs_root));
    strncpy(sysfs_root.name, "sys", SYSFS_NAME_MAX - 1);

    sysfs_kernel = sysfs_create_dir(&sysfs_root, "kernel");
    sysfs_devices = sysfs_create_dir(&sysfs_root, "devices");

    sysfs_create_dir(sysfs_kernel, "notes");
}

sysfs_node_t* sysfs_create_dir(sysfs_node_t* parent, const char* name)
{
    if (!parent || !name) return NULL;
    if (parent->child_count >= 16) return NULL;

    sysfs_node_t* node = __sysfs_alloc_node(name);
    if (!node) return NULL;

    node->parent = parent;
    parent->children[parent->child_count++] = node;
    return node;
}

int sysfs_create_file(sysfs_node_t* dir, const char* name,
                       int (*show)(char*, uint64_t), int (*store)(const char*, uint64_t))
{
    if (!dir || !name) return -1;
    if (dir->attr_count >= SYSFS_MAX_ATTRS) return -1;

    sysfs_attr_t* attr = &dir->attrs[dir->attr_count++];
    strncpy(attr->name, name, SYSFS_NAME_MAX - 1);
    attr->is_dynamic = 1;
    attr->show = show;
    attr->store = store;
    attr->value[0] = '\0';
    return 0;
}

int sysfs_set_attr(sysfs_node_t* dir, const char* name, const char* value)
{
    if (!dir || !name || !value) return -1;

    for (uint32_t i = 0; i < dir->attr_count; i++) {
        if (strcmp(dir->attrs[i].name, name) == 0) {
            strncpy(dir->attrs[i].value, value, 255);
            dir->attrs[i].value[255] = '\0';
            return 0;
        }
    }
    return -1;
}

static int sysfs_vfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t size)
{
    sysfs_node_t* snode = (sysfs_node_t*)node->impl_data;
    if (!snode) return 0;

    char temp[4096];
    int total = 0;

    for (uint32_t i = 0; i < snode->attr_count; i++) {
        sysfs_attr_t* attr = &snode->attrs[i];
        int len;
        if (attr->is_dynamic && attr->show) {
            len = attr->show(temp + total, sizeof(temp) - total);
        } else {
            len = snprintf(temp + total, sizeof(temp) - total, "%s\n", attr->value);
        }
        if (len > 0) total += len;
    }

    if ((uint64_t)total <= offset) return 0;
    if (offset + size > (uint64_t)total) size = (uint64_t)total - offset;
    memcpy(buf, temp + offset, size);
    return (int)size;
}

static int sysfs_vfs_readdir(vfs_node_t* node, int index, char* name, uint64_t max_len)
{
    sysfs_node_t* snode = (sysfs_node_t*)node->impl_data;
    if (!snode) return -1;

    int count = 0;

    for (uint32_t i = 0; i < snode->child_count; i++) {
        if (count == index) {
            uint64_t len = strlen(snode->children[i]->name);
            if (len >= max_len) len = max_len - 1;
            memcpy(name, snode->children[i]->name, len);
            name[len] = '\0';
            return 0;
        }
        count++;
    }

    index -= count;
    for (uint32_t i = 0; i < snode->attr_count; i++) {
        if (count == index) {
            uint64_t len = strlen(snode->attrs[i].name);
            if (len >= max_len) len = max_len - 1;
            memcpy(name, snode->attrs[i].name, len);
            name[len] = '\0';
            return 0;
        }
        count++;
    }

    return -1;
}

vfs_node_t* sysfs_get_root(void)
{
    if (!sysfs_vfs_root) {
        sysfs_vfs_root = vfs_create_node("sysfs", FS_TYPE_DIRECTORY);
        if (sysfs_vfs_root) {
            sysfs_vfs_root->impl_data = &sysfs_root;
            sysfs_vfs_root->read = sysfs_vfs_read;
            sysfs_vfs_root->readdir = sysfs_vfs_readdir;
        }
    }
    return sysfs_vfs_root;
}

sysfs_node_t* sysfs_get_kernel_dir(void)
{
    return sysfs_kernel;
}

sysfs_node_t* sysfs_get_devices_dir(void)
{
    return sysfs_devices;
}
