#include "kapi_cdev.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

struct kapi_cdev {
    uint32_t major;
    uint32_t minor;
    char name[KAPI_CDEV_NAME_MAX];
    kapi_cdev_ops_t ops;
    int refcount;
    int registered;
    int valid;
};

static struct kapi_cdev kapi_cdev_table[KAPI_CDEV_MAX_DEVICES];
static int kapi_cdev_subsystem_initialized = 0;

int kapi_cdev_init_subsystem(void)
{
    if (kapi_cdev_subsystem_initialized) {
        return KAPI_OK;
    }

    memset(kapi_cdev_table, 0, sizeof(kapi_cdev_table));
    kapi_cdev_subsystem_initialized = 1;
    return KAPI_OK;
}

kapi_cdev_t kapi_cdev_alloc(void)
{
    int slot = -1;
    for (int i = 0; i < KAPI_CDEV_MAX_DEVICES; i++) {
        if (!kapi_cdev_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    struct kapi_cdev* dev = &kapi_cdev_table[slot];
    memset(dev, 0, sizeof(*dev));
    dev->valid = 1;
    dev->refcount = 1;

    return dev;
}

int kapi_cdev_init(kapi_cdev_t dev, const kapi_cdev_ops_t* ops)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (ops) {
        memcpy(&dev->ops, ops, sizeof(kapi_cdev_ops_t));
    } else {
        memset(&dev->ops, 0, sizeof(kapi_cdev_ops_t));
    }

    return KAPI_OK;
}

void kapi_cdev_free(kapi_cdev_t dev)
{
    if (!dev || !dev->valid) {
        return;
    }

    dev->valid = 0;
    dev->registered = 0;
    dev->refcount = 0;
    memset(&dev->ops, 0, sizeof(kapi_cdev_ops_t));
}

int kapi_cdev_add(kapi_cdev_t dev, uint32_t major, const char* name)
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

int kapi_cdev_del(kapi_cdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    dev->registered = 0;
    return KAPI_OK;
}

int kapi_cdev_register(kapi_cdev_t dev, uint32_t major, uint32_t count, const char* name, const kapi_cdev_ops_t* ops)
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
        memcpy(&dev->ops, ops, sizeof(kapi_cdev_ops_t));
    }

    dev->registered = 1;
    return KAPI_OK;
}

int kapi_cdev_unregister(kapi_cdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    dev->registered = 0;
    return KAPI_OK;
}

kapi_cdev_t kapi_cdev_get(uint32_t major, uint32_t minor)
{
    for (int i = 0; i < KAPI_CDEV_MAX_DEVICES; i++) {
        if (kapi_cdev_table[i].valid && kapi_cdev_table[i].registered &&
            kapi_cdev_table[i].major == major && kapi_cdev_table[i].minor == minor) {
            kapi_cdev_table[i].refcount++;
            return &kapi_cdev_table[i];
        }
    }

    return NULL;
}

void kapi_cdev_put(kapi_cdev_t dev)
{
    if (!dev || !dev->valid) {
        return;
    }

    if (dev->refcount > 0) {
        dev->refcount--;
    }
}

int kapi_cdev_open(kapi_cdev_t dev, int mode)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.open) {
        return dev->ops.open(dev, mode);
    }

    return KAPI_OK;
}

int kapi_cdev_release(kapi_cdev_t dev)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.release) {
        return dev->ops.release(dev);
    }

    return KAPI_OK;
}

int64_t kapi_cdev_read(kapi_cdev_t dev, void* buf, size_t count)
{
    if (!dev || !dev->valid || !buf || count == 0) {
        return KAPI_EINVAL;
    }

    if (dev->ops.read) {
        return dev->ops.read(dev, buf, count);
    }

    return KAPI_ENOSYS;
}

int64_t kapi_cdev_write(kapi_cdev_t dev, const void* buf, size_t count)
{
    if (!dev || !dev->valid || !buf || count == 0) {
        return KAPI_EINVAL;
    }

    if (dev->ops.write) {
        return dev->ops.write(dev, buf, count);
    }

    return KAPI_ENOSYS;
}

int kapi_cdev_ioctl(kapi_cdev_t dev, uint32_t cmd, void* arg)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    if (dev->ops.ioctl) {
        return dev->ops.ioctl(dev, cmd, arg);
    }

    return KAPI_ENOSYS;
}

int kapi_cdev_get_info(kapi_cdev_t dev, kapi_cdev_info_t* info)
{
    if (!dev || !dev->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    info->major = dev->major;
    info->minor = dev->minor;
    strncpy(info->name, dev->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->refcount = dev->refcount;
    info->registered = dev->registered;

    return KAPI_OK;
}

int kapi_cdev_count(void)
{
    int count = 0;
    for (int i = 0; i < KAPI_CDEV_MAX_DEVICES; i++) {
        if (kapi_cdev_table[i].valid && kapi_cdev_table[i].registered) {
            count++;
        }
    }
    return count;
}