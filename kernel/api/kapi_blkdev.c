#include "kapi_blkdev.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

struct kapi_blkdev {
    uint32_t major;
    uint32_t minor;
    char name[KAPI_BLKDEV_NAME_MAX];
    kapi_blkdev_ops_t ops;
    uint64_t capacity;
    uint32_t sector_size;
    uint32_t max_sectors;
    int refcount;
    int registered;
    int readonly;
    int valid;
};

static struct kapi_blkdev kapi_blkdev_table[KAPI_BLKDEV_MAX_DEVICES];
static int kapi_blkdev_subsystem_initialized = 0;

int kapi_blkdev_init_subsystem(void)
{
    if (kapi_blkdev_subsystem_initialized) {
        return KAPI_OK;
    }

    memset(kapi_blkdev_table, 0, sizeof(kapi_blkdev_table));
    kapi_blkdev_subsystem_initialized = 1;
    return KAPI_OK;
}

kapi_blkdev_t kapi_blkdev_alloc(void)
{
    int slot = -1;
    for (int i = 0; i < KAPI_BLKDEV_MAX_DEVICES; i++) {
        if (!kapi_blkdev_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    struct kapi_blkdev* dev = &kapi_blkdev_table[slot];
    memset(dev, 0, sizeof(*dev));
    dev->valid = 1;
    dev->refcount = 1;
    dev->sector_size = KAPI_BLKDEV_SECTOR_SIZE;
    dev->max_sectors = KAPI_BLKDEV_MAX_REQUEST;

    return dev;
}

int kapi_blkdev_init(kapi_blkdev_t dev, const kapi_blkdev_ops_t* ops)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (ops) {
        memcpy(&dev->ops, ops, sizeof(kapi_blkdev_ops_t));
    } else {
        memset(&dev->ops, 0, sizeof(kapi_blkdev_ops_t));
    }

    return KAPI_OK;
}

void kapi_blkdev_free(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return;
    }

    dev->valid = 0;
    dev->registered = 0;
    dev->refcount = 0;
    memset(&dev->ops, 0, sizeof(kapi_blkdev_ops_t));
}

int kapi_blkdev_add(kapi_blkdev_t dev, uint32_t major, const char* name)
{
    if (!dev || !dev->valid || !name) {
        return KAPI_EINVAL;
    }

    dev->major = major;
    dev->minor = 0;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->registered = 1;

    return KAPI_OK;
}

int kapi_blkdev_del(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    dev->registered = 0;
    return KAPI_OK;
}

int kapi_blkdev_register(kapi_blkdev_t dev, uint32_t major, uint32_t count, const char* name, const kapi_blkdev_ops_t* ops)
{
    if (!dev || !dev->valid || !name) {
        return KAPI_EINVAL;
    }

    (void)count;

    dev->major = major;
    dev->minor = 0;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';

    if (ops) {
        memcpy(&dev->ops, ops, sizeof(kapi_blkdev_ops_t));
    }

    dev->registered = 1;
    return KAPI_OK;
}

int kapi_blkdev_unregister(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    dev->registered = 0;
    return KAPI_OK;
}

kapi_blkdev_t kapi_blkdev_get(uint32_t major, uint32_t minor)
{
    for (int i = 0; i < KAPI_BLKDEV_MAX_DEVICES; i++) {
        if (kapi_blkdev_table[i].valid && kapi_blkdev_table[i].registered &&
            kapi_blkdev_table[i].major == major && kapi_blkdev_table[i].minor == minor) {
            kapi_blkdev_table[i].refcount++;
            return &kapi_blkdev_table[i];
        }
    }

    return NULL;
}

kapi_blkdev_t kapi_blkdev_get_by_index(int index)
{
    if (index < 0) return NULL;
    int count = 0;
    for (int i = 0; i < KAPI_BLKDEV_MAX_DEVICES; i++) {
        if (kapi_blkdev_table[i].valid && kapi_blkdev_table[i].registered) {
            if (count == index) {
                kapi_blkdev_table[i].refcount++;
                return &kapi_blkdev_table[i];
            }
            count++;
        }
    }
    return NULL;
}

void kapi_blkdev_put(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return;
    }

    if (dev->refcount > 0) {
        dev->refcount--;
    }
}

int kapi_blkdev_open(kapi_blkdev_t dev, int mode)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.open) {
        return dev->ops.open(dev, mode);
    }

    return KAPI_OK;
}

int kapi_blkdev_release(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.release) {
        return dev->ops.release(dev);
    }

    return KAPI_OK;
}

int kapi_blkdev_submit(kapi_blkdev_t dev, kapi_blkdev_request_t* req)
{
    if (!dev || !dev->valid || !req) {
        return KAPI_EINVAL;
    }

    if (dev->ops.request) {
        return dev->ops.request(dev, req);
    }

    return KAPI_ENOSYS;
}

int kapi_blkdev_read_sectors(kapi_blkdev_t dev, uint64_t sector, uint64_t count, void* buf)
{
    if (!dev || !dev->valid || !buf || count == 0) {
        return KAPI_EINVAL;
    }

    kapi_blkdev_request_t req;
    memset(&req, 0, sizeof(req));
    req.sector = sector;
    req.count = count;
    req.direction = KAPI_BLKDEV_READ;
    req.buffer = buf;

    return kapi_blkdev_submit(dev, &req);
}

int kapi_blkdev_write_sectors(kapi_blkdev_t dev, uint64_t sector, uint64_t count, const void* buf)
{
    if (!dev || !dev->valid || !buf || count == 0) {
        return KAPI_EINVAL;
    }

    kapi_blkdev_request_t req;
    memset(&req, 0, sizeof(req));
    req.sector = sector;
    req.count = count;
    req.direction = KAPI_BLKDEV_WRITE;
    req.buffer = (void*)buf;

    return kapi_blkdev_submit(dev, &req);
}

int kapi_blkdev_flush(kapi_blkdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    kapi_blkdev_request_t req;
    memset(&req, 0, sizeof(req));
    req.direction = KAPI_BLKDEV_FLUSH;

    return kapi_blkdev_submit(dev, &req);
}

int kapi_blkdev_ioctl(kapi_blkdev_t dev, uint32_t cmd, void* arg)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.ioctl) {
        return dev->ops.ioctl(dev, cmd, arg);
    }

    return KAPI_ENOSYS;
}

int kapi_blkdev_get_info(kapi_blkdev_t dev, kapi_blkdev_info_t* info)
{
    if (!dev || !dev->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    info->major = dev->major;
    info->minor = dev->minor;
    strncpy(info->name, dev->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->capacity = dev->capacity;
    info->sector_size = dev->sector_size;
    info->max_sectors = dev->max_sectors;
    info->refcount = dev->refcount;
    info->registered = dev->registered;
    info->readonly = dev->readonly;

    return KAPI_OK;
}

int kapi_blkdev_count(void)
{
    int count = 0;
    for (int i = 0; i < KAPI_BLKDEV_MAX_DEVICES; i++) {
        if (kapi_blkdev_table[i].valid && kapi_blkdev_table[i].registered) {
            count++;
        }
    }
    return count;
}