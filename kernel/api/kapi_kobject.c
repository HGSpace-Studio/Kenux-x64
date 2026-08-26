#include "kapi_kobject.h"
#include "kapi.h"

#include <arch/fs.h>
#include <fs.h>
#include <arch/memory.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

struct kapi_kobject {
    char               name[KAPI_KOBJECT_NAME_LEN];
    kapi_kobject_t*    parent;
    kapi_kobj_type_t*  ktype;
    kapi_kset_t*       kset;
    int                refcount;
    int                valid;
    int                in_sysfs;
};

struct kapi_kset {
    kapi_kobject_t     kobj;
    int                valid;
};

#define KAPI_SYSFS_MAX_ATTRS 256

typedef struct {
    char               name[64];
    uint32_t           mode;
    kapi_kobj_show_fn  show;
    kapi_kobj_store_fn store;
    kapi_kobject_t*    parent;
    int                valid;
} kapi_sysfs_attr_t;

static kapi_sysfs_attr_t kapi_sysfs_attrs[KAPI_SYSFS_MAX_ATTRS];
static int kapi_sysfs_initialized = 0;

static kapi_kobject_t* kapi_sysfs_root = NULL;

static kapi_sysfs_attr_t* kapi_sysfs_find_attr(kapi_kobject_t* parent,
                                                const char* name)
{
    for (int i = 0; i < KAPI_SYSFS_MAX_ATTRS; i++) {
        if (kapi_sysfs_attrs[i].valid &&
            kapi_sysfs_attrs[i].parent == parent &&
            strcmp(kapi_sysfs_attrs[i].name, name) == 0) {
            return &kapi_sysfs_attrs[i];
        }
    }
    return NULL;
}

int kapi_sysfs_init(void)
{
    if (kapi_sysfs_initialized) {
        return KAPI_OK;
    }

    memset(kapi_sysfs_attrs, 0, sizeof(kapi_sysfs_attrs));

    kapi_sysfs_root = (kapi_kobject_t*)memory_alloc(sizeof(kapi_kobject_t));
    if (!kapi_sysfs_root) {
        return KAPI_ENOMEM;
    }

    memset(kapi_sysfs_root, 0, sizeof(*kapi_sysfs_root));
    strncpy(kapi_sysfs_root->name, "sys", KAPI_KOBJECT_NAME_LEN - 1);
    kapi_sysfs_root->refcount = 1;
    kapi_sysfs_root->valid = 1;
    kapi_sysfs_root->in_sysfs = 1;

    kapi_sysfs_initialized = 1;
    return KAPI_OK;
}

void kapi_kobject_init(kapi_kobject_t* kobj, kapi_kobj_type_t* ktype)
{
    if (!kobj) {
        return;
    }

    memset(kobj, 0, sizeof(*kobj));
    kobj->ktype = ktype;
    kobj->refcount = 1;
    kobj->valid = 1;
}

int kapi_kobject_add(kapi_kobject_t* kobj, kapi_kobject_t* parent,
                     const char* fmt, ...)
{
    if (!kobj || !kobj->valid) {
        return KAPI_EINVAL;
    }

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(kobj->name, KAPI_KOBJECT_NAME_LEN, fmt, args);
        va_end(args);
    }

    kobj->parent = parent ? parent : kapi_sysfs_root;
    kobj->in_sysfs = 1;

    if (kobj->parent && kobj->parent->in_sysfs) {
        char path[512];
        if (kobj->parent == kapi_sysfs_root) {
            snprintf(path, sizeof(path), "/sys/%s", kobj->name);
        } else {
            snprintf(path, sizeof(path), "/sys/%s/%s",
                     kobj->parent->name, kobj->name);
        }
        (void)path;
    }

    return KAPI_OK;
}

void kapi_kobject_del(kapi_kobject_t* kobj)
{
    if (!kobj || !kobj->valid) {
        return;
    }

    for (int i = 0; i < KAPI_SYSFS_MAX_ATTRS; i++) {
        if (kapi_sysfs_attrs[i].valid &&
            kapi_sysfs_attrs[i].parent == kobj) {
            kapi_sysfs_attrs[i].valid = 0;
        }
    }

    kobj->in_sysfs = 0;

    if (kobj->ktype && kobj->ktype->release) {
        kobj->ktype->release(kobj);
    }
}

kapi_kobject_t* kapi_kobject_get(kapi_kobject_t* kobj)
{
    if (!kobj || !kobj->valid) {
        return NULL;
    }

    kobj->refcount++;
    return kobj;
}

void kapi_kobject_put(kapi_kobject_t* kobj)
{
    if (!kobj) {
        return;
    }

    kobj->refcount--;
    if (kobj->refcount <= 0) {
        if (kobj->ktype && kobj->ktype->release) {
            kobj->ktype->release(kobj);
        }
        kobj->valid = 0;
    }
}

int kapi_kobject_set_name(kapi_kobject_t* kobj, const char* fmt, ...)
{
    if (!kobj || !kobj->valid || !fmt) {
        return KAPI_EINVAL;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(kobj->name, KAPI_KOBJECT_NAME_LEN, fmt, args);
    va_end(args);
    return KAPI_OK;
}

const char* kapi_kobject_get_name(kapi_kobject_t* kobj)
{
    if (!kobj || !kobj->valid) {
        return NULL;
    }
    return kobj->name;
}

int kapi_kobject_create_dir(kapi_kobject_t* parent, const char* name,
                             kapi_kobject_t** kobj)
{
    if (!name || !kobj) {
        return KAPI_EINVAL;
    }

    kapi_kobject_t* new_kobj = (kapi_kobject_t*)memory_alloc(sizeof(kapi_kobject_t));
    if (!new_kobj) {
        return KAPI_ENOMEM;
    }

    memset(new_kobj, 0, sizeof(*new_kobj));
    strncpy(new_kobj->name, name, KAPI_KOBJECT_NAME_LEN - 1);
    new_kobj->refcount = 1;
    new_kobj->valid = 1;
    new_kobj->parent = parent ? parent : kapi_sysfs_root;

    if (new_kobj->parent && new_kobj->parent->in_sysfs) {
        char path[512];
        if (new_kobj->parent == kapi_sysfs_root) {
            snprintf(path, sizeof(path), "/sys/%s", name);
        } else {
            snprintf(path, sizeof(path), "/sys/%s/%s",
                     new_kobj->parent->name, name);
        }
        (void)path;
        new_kobj->in_sysfs = 1;
    }

    *kobj = new_kobj;
    return KAPI_OK;
}

int kapi_kobject_create_file(kapi_kobject_t* parent, const char* name,
                              uint32_t mode, kapi_kobj_show_fn show,
                              kapi_kobj_store_fn store)
{
    if (!parent || !name) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SYSFS_MAX_ATTRS; i++) {
        if (!kapi_sysfs_attrs[i].valid) {
            strncpy(kapi_sysfs_attrs[i].name, name,
                    sizeof(kapi_sysfs_attrs[i].name) - 1);
            kapi_sysfs_attrs[i].mode = mode;
            kapi_sysfs_attrs[i].show = show;
            kapi_sysfs_attrs[i].store = store;
            kapi_sysfs_attrs[i].parent = parent;
            kapi_sysfs_attrs[i].valid = 1;

            if (parent->in_sysfs) {
                char path[512];
                if (parent == kapi_sysfs_root) {
                    snprintf(path, sizeof(path), "/sys/%s", name);
                } else {
                    snprintf(path, sizeof(path), "/sys/%s/%s",
                             parent->name, name);
                }
                (void)path;
                (void)mode;
            }

            return KAPI_OK;
        }
    }
    return KAPI_ERROR;
}

int kapi_kobject_remove_file(kapi_kobject_t* parent, const char* name)
{
    if (!parent || !name) {
        return KAPI_EINVAL;
    }

    kapi_sysfs_attr_t* attr = kapi_sysfs_find_attr(parent, name);
    if (!attr) {
        return KAPI_ENOENT;
    }

    attr->valid = 0;
    return KAPI_OK;
}