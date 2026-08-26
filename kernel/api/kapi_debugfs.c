#include "kapi_debugfs.h"
#include "kapi.h"

#include <arch/fs.h>
#include <fs.h>
#include <arch/memory.h>
#include <string.h>
#include <stdio.h>

struct kapi_debugfs_file {
    char                  name[256];
    char                  target[256];
    int                   type;
    uint32_t              mode;
    void*                 data;
    size_t                data_size;
    kapi_debugfs_fops_t*  fops;
    kapi_debugfs_file_t*  parent;
    int                   valid;
};

#define KAPI_DEBUGFS_MAX_FILES 512

static kapi_debugfs_file_t kapi_debugfs_table[KAPI_DEBUGFS_MAX_FILES];
static kapi_debugfs_file_t* kapi_debugfs_root = NULL;
static int kapi_debugfs_initialized = 0;

static kapi_debugfs_file_t* kapi_debugfs_alloc(void)
{
    for (int i = 0; i < KAPI_DEBUGFS_MAX_FILES; i++) {
        if (!kapi_debugfs_table[i].valid) {
            memset(&kapi_debugfs_table[i], 0, sizeof(kapi_debugfs_file_t));
            kapi_debugfs_table[i].valid = 1;
            return &kapi_debugfs_table[i];
        }
    }
    return NULL;
}

int kapi_debugfs_init(void)
{
    if (kapi_debugfs_initialized) {
        return KAPI_OK;
    }

    memset(kapi_debugfs_table, 0, sizeof(kapi_debugfs_table));

    kapi_debugfs_root = kapi_debugfs_alloc();
    if (!kapi_debugfs_root) {
        return KAPI_ERROR;
    }

    strncpy(kapi_debugfs_root->name, "debugfs", sizeof(kapi_debugfs_root->name) - 1);
    kapi_debugfs_root->type = KAPI_DEBUGFS_DIR;
    kapi_debugfs_root->parent = NULL;
    kapi_debugfs_initialized = 1;
    return KAPI_OK;
}

kapi_debugfs_file_t* kapi_debugfs_create_dir(const char* name,
                                              kapi_debugfs_file_t* parent)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_file_t* dir = kapi_debugfs_alloc();
    if (!dir) {
        return NULL;
    }

    strncpy(dir->name, name, sizeof(dir->name) - 1);
    dir->type = KAPI_DEBUGFS_DIR;
    dir->parent = parent ? parent : kapi_debugfs_root;

    char path[512];
    if (dir->parent && dir->parent != kapi_debugfs_root) {
        snprintf(path, sizeof(path), "%s/%s", dir->parent->name, name);
    } else {
        snprintf(path, sizeof(path), "/debug/%s", name);
    }

    (void)path;
    return dir;
}

kapi_debugfs_file_t* kapi_debugfs_create_file(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               void* data,
                                               kapi_debugfs_fops_t* fops)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_file_t* file = kapi_debugfs_alloc();
    if (!file) {
        return NULL;
    }

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->type = KAPI_DEBUGFS_FILE;
    file->mode = mode;
    file->data = data;
    file->fops = fops;
    file->parent = parent ? parent : kapi_debugfs_root;

    char path[512];
    if (file->parent && file->parent != kapi_debugfs_root) {
        snprintf(path, sizeof(path), "%s/%s", file->parent->name, name);
    } else {
        snprintf(path, sizeof(path), "/debug/%s", name);
    }

    (void)path;
    (void)mode;

    return file;
}

kapi_debugfs_file_t* kapi_debugfs_create_symlink(const char* name,
                                                  kapi_debugfs_file_t* parent,
                                                  const char* target)
{
    if (!name || !target) {
        return NULL;
    }

    kapi_debugfs_file_t* sym = kapi_debugfs_alloc();
    if (!sym) {
        return NULL;
    }

    strncpy(sym->name, name, sizeof(sym->name) - 1);
    strncpy(sym->target, target, sizeof(sym->target) - 1);
    sym->type = KAPI_DEBUGFS_SYML;
    sym->parent = parent ? parent : kapi_debugfs_root;
    return sym;
}

static int kapi_debugfs_u32_read(kapi_debugfs_file_t* file, char* buf,
                                  size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    if (*pos > 0) {
        return 0;
    }

    uint32_t val = *(uint32_t*)file->data;
    int len = snprintf(buf, count, "%u\n", val);
    *pos = (uint64_t)len;
    return len;
}

static int kapi_debugfs_u32_write(kapi_debugfs_file_t* file, const char* buf,
                                   size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    uint32_t val = 0;
    for (size_t i = 0; i < count && buf[i] >= '0' && buf[i] <= '9'; i++) {
        val = val * 10 + (uint32_t)(buf[i] - '0');
    }
    *(uint32_t*)file->data = val;
    *pos += count;
    return (int)count;
}

kapi_debugfs_file_t* kapi_debugfs_create_u32(const char* name, uint32_t mode,
                                              kapi_debugfs_file_t* parent,
                                              uint32_t* value)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_fops_t fops = {
        .read = kapi_debugfs_u32_read,
        .write = kapi_debugfs_u32_write,
        .private = NULL
    };

    kapi_debugfs_file_t* file = kapi_debugfs_alloc();
    if (!file) {
        return NULL;
    }

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->type = KAPI_DEBUGFS_FILE;
    file->mode = mode;
    file->data = value;
    file->fops = (kapi_debugfs_fops_t*)memory_alloc(sizeof(kapi_debugfs_fops_t));
    if (file->fops) {
        memcpy(file->fops, &fops, sizeof(fops));
    }
    file->parent = parent ? parent : kapi_debugfs_root;
    return file;
}

static int kapi_debugfs_u64_read(kapi_debugfs_file_t* file, char* buf,
                                  size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    if (*pos > 0) {
        return 0;
    }

    uint64_t val = *(uint64_t*)file->data;
    int len = snprintf(buf, count, "%llu\n", (unsigned long long)val);
    *pos = (uint64_t)len;
    return len;
}

static int kapi_debugfs_u64_write(kapi_debugfs_file_t* file, const char* buf,
                                   size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    uint64_t val = 0;
    for (size_t i = 0; i < count && buf[i] >= '0' && buf[i] <= '9'; i++) {
        val = val * 10 + (uint64_t)(buf[i] - '0');
    }
    *(uint64_t*)file->data = val;
    *pos += count;
    return (int)count;
}

kapi_debugfs_file_t* kapi_debugfs_create_u64(const char* name, uint32_t mode,
                                              kapi_debugfs_file_t* parent,
                                              uint64_t* value)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_fops_t fops = {
        .read = kapi_debugfs_u64_read,
        .write = kapi_debugfs_u64_write,
        .private = NULL
    };

    kapi_debugfs_file_t* file = kapi_debugfs_alloc();
    if (!file) {
        return NULL;
    }

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->type = KAPI_DEBUGFS_FILE;
    file->mode = mode;
    file->data = value;
    file->fops = (kapi_debugfs_fops_t*)memory_alloc(sizeof(kapi_debugfs_fops_t));
    if (file->fops) {
        memcpy(file->fops, &fops, sizeof(fops));
    }
    file->parent = parent ? parent : kapi_debugfs_root;
    return file;
}

static int kapi_debugfs_bool_read(kapi_debugfs_file_t* file, char* buf,
                                   size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    if (*pos > 0) {
        return 0;
    }

    int val = *(int*)file->data;
    const char* str = val ? "Y\n" : "N\n";
    int len = (int)strlen(str);
    if ((size_t)len < count) {
        memcpy(buf, str, (size_t)len);
    }
    *pos = (uint64_t)len;
    return len;
}

static int kapi_debugfs_bool_write(kapi_debugfs_file_t* file, const char* buf,
                                    size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    if (count > 0) {
        *(int*)file->data = (buf[0] == '1' || buf[0] == 'Y' || buf[0] == 'y') ? 1 : 0;
    }
    *pos += count;
    return (int)count;
}

kapi_debugfs_file_t* kapi_debugfs_create_bool(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               int* value)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_fops_t fops = {
        .read = kapi_debugfs_bool_read,
        .write = kapi_debugfs_bool_write,
        .private = NULL
    };

    kapi_debugfs_file_t* file = kapi_debugfs_alloc();
    if (!file) {
        return NULL;
    }

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->type = KAPI_DEBUGFS_FILE;
    file->mode = mode;
    file->data = value;
    file->fops = (kapi_debugfs_fops_t*)memory_alloc(sizeof(kapi_debugfs_fops_t));
    if (file->fops) {
        memcpy(file->fops, &fops, sizeof(fops));
    }
    file->parent = parent ? parent : kapi_debugfs_root;
    return file;
}

static int kapi_debugfs_blob_read(kapi_debugfs_file_t* file, char* buf,
                                   size_t count, uint64_t* pos)
{
    if (!file || !file->data || !buf) {
        return KAPI_EINVAL;
    }

    if (*pos >= file->data_size) {
        return 0;
    }

    size_t remaining = file->data_size - (size_t)*pos;
    size_t to_copy = count < remaining ? count : remaining;
    memcpy(buf, (uint8_t*)file->data + *pos, to_copy);
    *pos += to_copy;
    return (int)to_copy;
}

kapi_debugfs_file_t* kapi_debugfs_create_blob(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               void* data, size_t size)
{
    if (!name) {
        return NULL;
    }

    kapi_debugfs_fops_t fops = {
        .read = kapi_debugfs_blob_read,
        .write = NULL,
        .private = NULL
    };

    kapi_debugfs_file_t* file = kapi_debugfs_alloc();
    if (!file) {
        return NULL;
    }

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->type = KAPI_DEBUGFS_FILE;
    file->mode = mode;
    file->data = data;
    file->data_size = size;
    file->fops = (kapi_debugfs_fops_t*)memory_alloc(sizeof(kapi_debugfs_fops_t));
    if (file->fops) {
        memcpy(file->fops, &fops, sizeof(fops));
    }
    file->parent = parent ? parent : kapi_debugfs_root;
    return file;
}

void kapi_debugfs_remove(kapi_debugfs_file_t* file)
{
    if (!file || !file->valid) {
        return;
    }

    if (file->fops) {
        memory_free(file->fops);
        file->fops = NULL;
    }

    file->valid = 0;
}

void kapi_debugfs_remove_recursive(kapi_debugfs_file_t* file)
{
    if (!file || !file->valid) {
        return;
    }

    for (int i = 0; i < KAPI_DEBUGFS_MAX_FILES; i++) {
        if (kapi_debugfs_table[i].valid &&
            kapi_debugfs_table[i].parent == file) {
            kapi_debugfs_remove_recursive(&kapi_debugfs_table[i]);
        }
    }

    kapi_debugfs_remove(file);
}