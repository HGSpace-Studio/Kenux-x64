#ifndef KAPI_KOBJECT_H
#define KAPI_KOBJECT_H

#include <stdint.h>
#include <stddef.h>

typedef long ssize_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_kobject     kapi_kobject_t;
typedef struct kapi_kset        kapi_kset_t;
typedef struct kapi_kobj_type   kapi_kobj_type_t;
typedef struct kapi_attribute   kapi_attribute_t;

typedef void (*kapi_kobj_release_fn)(kapi_kobject_t* kobj);

typedef ssize_t (*kapi_kobj_show_fn)(kapi_kobject_t* kobj,
                                      kapi_attribute_t* attr, char* buf);
typedef ssize_t (*kapi_kobj_store_fn)(kapi_kobject_t* kobj,
                                       kapi_attribute_t* attr,
                                       const char* buf, size_t count);

struct kapi_kobj_type {
    kapi_kobj_release_fn release;
    kapi_kobj_show_fn    show;
    kapi_kobj_store_fn   store;
    void*                private;
};

struct kapi_attribute {
    char               name[64];
    uint32_t           mode;
    kapi_kobj_show_fn  show;
    kapi_kobj_store_fn store;
};

#define KAPI_KOBJECT_NAME_LEN 64

void kapi_kobject_init(kapi_kobject_t* kobj, kapi_kobj_type_t* ktype);

int kapi_kobject_add(kapi_kobject_t* kobj, kapi_kobject_t* parent,
                     const char* fmt, ...);

void kapi_kobject_del(kapi_kobject_t* kobj);

kapi_kobject_t* kapi_kobject_get(kapi_kobject_t* kobj);

void kapi_kobject_put(kapi_kobject_t* kobj);

int kapi_kobject_set_name(kapi_kobject_t* kobj, const char* fmt, ...);

const char* kapi_kobject_get_name(kapi_kobject_t* kobj);

int kapi_kobject_create_dir(kapi_kobject_t* parent, const char* name,
                             kapi_kobject_t** kobj);

int kapi_kobject_create_file(kapi_kobject_t* parent, const char* name,
                              uint32_t mode, kapi_kobj_show_fn show,
                              kapi_kobj_store_fn store);

int kapi_kobject_remove_file(kapi_kobject_t* parent, const char* name);

int kapi_sysfs_init(void);

#ifdef __cplusplus
}
#endif

#endif