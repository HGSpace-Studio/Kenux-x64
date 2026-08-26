

#include <arch/fs.h>
#include <arch/memory.h>
#include <string.h>

#define TMPFS_BLOCK_SIZE    4096

typedef struct tmpfs_data {
    uint8_t* data;
    uint64_t size;
    uint64_t capacity;
} tmpfs_data_t;

typedef struct tmpfs_dir_entry {
    char name[FS_MAX_NAME];
    vfs_node_t* node;
    struct tmpfs_dir_entry* next;
} tmpfs_dir_entry_t;

static int tmpfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t size)
{
    tmpfs_data_t* d = (tmpfs_data_t*)node->impl_data;
    if (!d || !d->data) return 0;
    if (offset >= d->size) return 0;
    if (offset + size > d->size) size = d->size - offset;
    memcpy(buf, d->data + offset, size);
    return (int)size;
}

static int tmpfs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t size)
{
    tmpfs_data_t* d = (tmpfs_data_t*)node->impl_data;
    if (!d) return -1;

    uint64_t need = offset + size;
    if (need > d->capacity) {
        uint64_t new_cap = d->capacity ? d->capacity * 2 : TMPFS_BLOCK_SIZE;
        while (new_cap < need) new_cap *= 2;
        uint8_t* new_data = (uint8_t*)memory_alloc(new_cap);
        if (!new_data) return -1;
        if (d->data) {
            memcpy(new_data, d->data, d->size);
            memory_free(d->data);
        }
        d->data = new_data;
        d->capacity = new_cap;
    }

    memcpy(d->data + offset, buf, size);
    if (need > d->size) d->size = need;
    node->size = d->size;
    return (int)size;
}

static int tmpfs_open(vfs_node_t* node, int flags)
{
    (void)flags;
    if (!node) return -1;
    node->nlink++;
    return 0;
}

static int tmpfs_close(vfs_node_t* node)
{
    if (!node) return -1;
    if (node->nlink > 0) node->nlink--;
    return 0;
}

static vfs_node_t* tmpfs_finddir(vfs_node_t* node, const char* name)
{
    tmpfs_dir_entry_t* entry = (tmpfs_dir_entry_t*)node->impl_data;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry->node;
        }
        entry = entry->next;
    }
    return NULL;
}

static int tmpfs_readdir(vfs_node_t* node, int index, char* name, uint64_t max_len)
{
    tmpfs_dir_entry_t* entry = (tmpfs_dir_entry_t*)node->impl_data;
    int i = 0;
    while (entry) {
        if (i == index) {
            uint64_t len = strlen(entry->name);
            if (len >= max_len) len = max_len - 1;
            memcpy(name, entry->name, len);
            name[len] = '\0';
            return 0;
        }
        entry = entry->next;
        i++;
    }
    return -1;
}

static int tmpfs_unlink(vfs_node_t* parent, const char* name);

static int tmpfs_mkdir(vfs_node_t* parent, const char* name, uint64_t mode)
{
    (void)mode;
    vfs_node_t* node = vfs_create_node(name, FS_TYPE_DIRECTORY);
    if (!node) return -1;
    node->read = tmpfs_read;
    node->write = tmpfs_write;
    node->open = tmpfs_open;
    node->close = tmpfs_close;
    node->finddir = tmpfs_finddir;
    node->readdir = tmpfs_readdir;
    node->mkdir = tmpfs_mkdir;
    node->unlink = tmpfs_unlink;
    vfs_add_child(parent, node);
    return 0;
}

static int tmpfs_unlink(vfs_node_t* parent, const char* name)
{
    if (!parent || !name) return -1;
    tmpfs_dir_entry_t** current = (tmpfs_dir_entry_t**)&parent->impl_data;
    while (*current) {
        tmpfs_dir_entry_t* entry = *current;
        if (strcmp(entry->name, name) == 0) {
            *current = entry->next;
            if (entry->node) {
                if (entry->node->impl_data) {
                    tmpfs_data_t* d = (tmpfs_data_t*)entry->node->impl_data;
                    if (d->data) memory_free(d->data);
                    memory_free(d);
                }
                memory_free(entry->node);
            }
            memory_free(entry);
            return 0;
        }
        current = &entry->next;
    }
    return -1;
}

vfs_node_t* tmpfs_create(void)
{
    vfs_node_t* root = vfs_create_node("tmpfs", FS_TYPE_DIRECTORY);
    if (!root) return NULL;

    root->read = tmpfs_read;
    root->write = tmpfs_write;
    root->open = tmpfs_open;
    root->close = tmpfs_close;
    root->finddir = tmpfs_finddir;
    root->readdir = tmpfs_readdir;
    root->mkdir = tmpfs_mkdir;
    root->unlink = tmpfs_unlink;

    return root;
}

vfs_node_t* tmpfs_create_file(vfs_node_t* parent, const char* name)
{
    vfs_node_t* node = vfs_create_node(name, FS_TYPE_REGULAR);
    if (!node) return NULL;

    tmpfs_data_t* d = (tmpfs_data_t*)memory_zalloc(sizeof(tmpfs_data_t));
    if (!d) {
        memory_free(node);
        return NULL;
    }

    node->impl_data = d;
    node->read = tmpfs_read;
    node->write = tmpfs_write;
    node->open = tmpfs_open;
    node->close = tmpfs_close;

    if (parent) {
        vfs_add_child(parent, node);
    }
    return node;
}
