#ifndef KAPI_BLKDEV_H
#define KAPI_BLKDEV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_BLKDEV_NAME_MAX      64
#define KAPI_BLKDEV_MAX_DEVICES   256
#define KAPI_BLKDEV_SECTOR_SIZE   512
#define KAPI_BLKDEV_MAX_REQUEST   256

#define KAPI_BLKDEV_READ          0
#define KAPI_BLKDEV_WRITE         1
#define KAPI_BLKDEV_FLUSH         2
#define KAPI_BLKDEV_DISCARD       3
#define KAPI_BLKDEV_SECURE_ERASE  4

typedef struct kapi_blkdev* kapi_blkdev_t;

typedef struct {
    uint64_t sector;
    uint64_t count;
    int      direction;
    void*    buffer;
    int      status;
} kapi_blkdev_request_t;

typedef struct {
    int (*open)(kapi_blkdev_t dev, int mode);
    int (*release)(kapi_blkdev_t dev);
    int (*request)(kapi_blkdev_t dev, kapi_blkdev_request_t* req);
    int (*ioctl)(kapi_blkdev_t dev, uint32_t cmd, void* arg);
    int (*getgeo)(kapi_blkdev_t dev, uint32_t* cylinders, uint32_t* heads, uint32_t* sectors);
} kapi_blkdev_ops_t;

typedef struct {
    uint32_t major;
    uint32_t minor;
    char     name[KAPI_BLKDEV_NAME_MAX];
    uint64_t capacity;
    uint32_t sector_size;
    uint32_t max_sectors;
    int      refcount;
    int      registered;
    int      readonly;
} kapi_blkdev_info_t;

kapi_blkdev_t kapi_blkdev_alloc(void);
int kapi_blkdev_init(kapi_blkdev_t dev, const kapi_blkdev_ops_t* ops);
void kapi_blkdev_free(kapi_blkdev_t dev);

int kapi_blkdev_add(kapi_blkdev_t dev, uint32_t major, const char* name);
int kapi_blkdev_del(kapi_blkdev_t dev);

int kapi_blkdev_register(kapi_blkdev_t dev, uint32_t major, uint32_t count, const char* name, const kapi_blkdev_ops_t* ops);
int kapi_blkdev_unregister(kapi_blkdev_t dev);

kapi_blkdev_t kapi_blkdev_get(uint32_t major, uint32_t minor);
kapi_blkdev_t kapi_blkdev_get_by_index(int index);
void kapi_blkdev_put(kapi_blkdev_t dev);

int kapi_blkdev_open(kapi_blkdev_t dev, int mode);
int kapi_blkdev_release(kapi_blkdev_t dev);
int kapi_blkdev_submit(kapi_blkdev_t dev, kapi_blkdev_request_t* req);
int kapi_blkdev_read_sectors(kapi_blkdev_t dev, uint64_t sector, uint64_t count, void* buf);
int kapi_blkdev_write_sectors(kapi_blkdev_t dev, uint64_t sector, uint64_t count, const void* buf);
int kapi_blkdev_flush(kapi_blkdev_t dev);
int kapi_blkdev_ioctl(kapi_blkdev_t dev, uint32_t cmd, void* arg);

int kapi_blkdev_get_info(kapi_blkdev_t dev, kapi_blkdev_info_t* info);
int kapi_blkdev_count(void);

int kapi_blkdev_init_subsystem(void);

#ifdef __cplusplus
}
#endif

#endif