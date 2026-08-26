#ifndef KAPI_CDEV_H
#define KAPI_CDEV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CDEV_NAME_MAX    64
#define KAPI_CDEV_MAX_DEVICES 256

typedef struct kapi_cdev* kapi_cdev_t;

typedef struct {
    int (*open)(kapi_cdev_t dev, int mode);
    int (*release)(kapi_cdev_t dev);
    int64_t (*read)(kapi_cdev_t dev, void* buf, size_t count);
    int64_t (*write)(kapi_cdev_t dev, const void* buf, size_t count);
    int (*ioctl)(kapi_cdev_t dev, uint32_t cmd, void* arg);
    int64_t (*llseek)(kapi_cdev_t dev, int64_t offset, int whence);
    int (*mmap)(kapi_cdev_t dev, void* addr, size_t size, int prot);
    int (*poll)(kapi_cdev_t dev, uint32_t events);
} kapi_cdev_ops_t;

typedef struct {
    uint32_t major;
    uint32_t minor;
    char     name[KAPI_CDEV_NAME_MAX];
    int      refcount;
    int      registered;
} kapi_cdev_info_t;

kapi_cdev_t kapi_cdev_alloc(void);
int kapi_cdev_init(kapi_cdev_t dev, const kapi_cdev_ops_t* ops);
void kapi_cdev_free(kapi_cdev_t dev);

int kapi_cdev_add(kapi_cdev_t dev, uint32_t major, const char* name);
int kapi_cdev_del(kapi_cdev_t dev);

int kapi_cdev_register(kapi_cdev_t dev, uint32_t major, uint32_t count, const char* name, const kapi_cdev_ops_t* ops);
int kapi_cdev_unregister(kapi_cdev_t dev);

kapi_cdev_t kapi_cdev_get(uint32_t major, uint32_t minor);
void kapi_cdev_put(kapi_cdev_t dev);

int kapi_cdev_open(kapi_cdev_t dev, int mode);
int kapi_cdev_release(kapi_cdev_t dev);
int64_t kapi_cdev_read(kapi_cdev_t dev, void* buf, size_t count);
int64_t kapi_cdev_write(kapi_cdev_t dev, const void* buf, size_t count);
int kapi_cdev_ioctl(kapi_cdev_t dev, uint32_t cmd, void* arg);

int kapi_cdev_get_info(kapi_cdev_t dev, kapi_cdev_info_t* info);
int kapi_cdev_count(void);

int kapi_cdev_init_subsystem(void);

#ifdef __cplusplus
}
#endif

#endif