#include <arch/virtio_gpu.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <string.h>

#define VIRTIO_GPU_MAX_DEVICES 4

static virtio_gpu_t virtio_gpu_devices[VIRTIO_GPU_MAX_DEVICES];
static uint8_t virtio_gpu_count = 0;

static inline uint32_t virtio_gpu_read32(virtio_gpu_t* dev, uint32_t reg)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    return *ptr;
}

static inline void virtio_gpu_write32(virtio_gpu_t* dev, uint32_t reg, uint32_t val)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    *ptr = val;
}

void virtio_gpu_init(void)
{
    memset(virtio_gpu_devices, 0, sizeof(virtio_gpu_devices));
    virtio_gpu_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t id = pci_read_config(b, d, f, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (vendor != VIRTIO_GPU_VENDOR_ID || device != VIRTIO_GPU_DEVICE_ID)
                    continue;
                if (virtio_gpu_count >= VIRTIO_GPU_MAX_DEVICES) return;

                virtio_gpu_t* dev = &virtio_gpu_devices[virtio_gpu_count];
                dev->bus = (uint8_t)b;
                dev->device = (uint8_t)d;
                dev->function = (uint8_t)f;
                dev->vendor_id = vendor;
                dev->device_id = device;
                dev->irq = pci_read_config(b, d, f, 0x3C) & 0xFF;
                spin_init(&dev->lock);

                uint32_t bar = pci_read_config(b, d, f, 0x10);
                dev->mmio_base = (uint64_t)(bar & 0xFFFFFFF0);
                uint32_t bar_hi = pci_read_config(b, d, f, 0x14);
                dev->mmio_base |= ((uint64_t)bar_hi << 32);

                uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_IO_SPACE;
                pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                dev->next_resource_id = 1;
                dev->scanout_count = 0;

                for (int i = 0; i < VIRTIO_GPU_MAX_RESOURCES; i++) {
                    dev->resources[i].resource_id = 0;
                }

                dev->initialized = 1;
                virtio_gpu_count++;
            }
        }
    }
}

virtio_gpu_t* virtio_gpu_get_device(uint8_t index)
{
    if (index >= virtio_gpu_count) return NULL;
    return &virtio_gpu_devices[index];
}

int virtio_gpu_get_display_info(virtio_gpu_t* dev)
{
    if (!dev || !dev->initialized) return -1;

    dev->scanout_count = 1;
    dev->scanouts[0].width = 1024;
    dev->scanouts[0].height = 768;
    dev->scanouts[0].resource_id = 0;
    dev->scanouts[0].x = 0;
    dev->scanouts[0].y = 0;

    return 0;
}

uint32_t virtio_gpu_create_resource_2d(virtio_gpu_t* dev, uint32_t width, uint32_t height, uint32_t format)
{
    if (!dev || !dev->initialized) return 0;

    spinlock_acquire(&dev->lock);

    uint32_t res_id = dev->next_resource_id++;
    if (res_id > VIRTIO_GPU_MAX_RESOURCES) {
        spinlock_release(&dev->lock);
        return 0;
    }

    int idx = -1;
    for (int i = 0; i < VIRTIO_GPU_MAX_RESOURCES; i++) {
        if (dev->resources[i].resource_id == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        spinlock_release(&dev->lock);
        return 0;
    }

    dev->resources[idx].resource_id = res_id;
    dev->resources[idx].width = width;
    dev->resources[idx].height = height;
    dev->resources[idx].format = format;
    dev->resources[idx].backing = NULL;
    dev->resources[idx].backing_phys = 0;
    dev->resources[idx].backing_size = 0;

    spinlock_release(&dev->lock);
    return res_id;
}

int virtio_gpu_unref_resource(virtio_gpu_t* dev, uint32_t resource_id)
{
    if (!dev || !dev->initialized || resource_id == 0) return -1;

    spinlock_acquire(&dev->lock);

    for (int i = 0; i < VIRTIO_GPU_MAX_RESOURCES; i++) {
        if (dev->resources[i].resource_id == resource_id) {
            if (dev->resources[i].backing) {
                memory_free(dev->resources[i].backing);
            }
            memset(&dev->resources[i], 0, sizeof(virtio_gpu_resource_t));
            spinlock_release(&dev->lock);
            return 0;
        }
    }

    spinlock_release(&dev->lock);
    return -2;
}

int virtio_gpu_attach_backing(virtio_gpu_t* dev, uint32_t resource_id, void* data, uint32_t size)
{
    if (!dev || !dev->initialized || resource_id == 0 || !data) return -1;

    spinlock_acquire(&dev->lock);

    for (int i = 0; i < VIRTIO_GPU_MAX_RESOURCES; i++) {
        if (dev->resources[i].resource_id == resource_id) {
            dev->resources[i].backing = data;
            dev->resources[i].backing_phys = (uint64_t)(uintptr_t)data;
            dev->resources[i].backing_size = size;
            spinlock_release(&dev->lock);
            return 0;
        }
    }

    spinlock_release(&dev->lock);
    return -2;
}

int virtio_gpu_detach_backing(virtio_gpu_t* dev, uint32_t resource_id)
{
    if (!dev || !dev->initialized || resource_id == 0) return -1;

    spinlock_acquire(&dev->lock);

    for (int i = 0; i < VIRTIO_GPU_MAX_RESOURCES; i++) {
        if (dev->resources[i].resource_id == resource_id) {
            dev->resources[i].backing = NULL;
            dev->resources[i].backing_phys = 0;
            dev->resources[i].backing_size = 0;
            spinlock_release(&dev->lock);
            return 0;
        }
    }

    spinlock_release(&dev->lock);
    return -2;
}

int virtio_gpu_set_scanout(virtio_gpu_t* dev, uint32_t scanout_id, uint32_t resource_id,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!dev || !dev->initialized) return -1;
    if (scanout_id >= VIRTIO_GPU_MAX_SCANOUTS) return -2;

    spinlock_acquire(&dev->lock);
    dev->scanouts[scanout_id].resource_id = resource_id;
    dev->scanouts[scanout_id].x = x;
    dev->scanouts[scanout_id].y = y;
    dev->scanouts[scanout_id].width = width;
    dev->scanouts[scanout_id].height = height;
    if (scanout_id >= dev->scanout_count) dev->scanout_count = scanout_id + 1;
    spinlock_release(&dev->lock);
    return 0;
}

int virtio_gpu_transfer_to_host(virtio_gpu_t* dev, uint32_t resource_id,
                                uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!dev || !dev->initialized || resource_id == 0) return -1;
    (void)x; (void)y; (void)width; (void)height;
    return 0;
}

int virtio_gpu_flush_resource(virtio_gpu_t* dev, uint32_t resource_id,
                              uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!dev || !dev->initialized || resource_id == 0) return -1;
    (void)x; (void)y; (void)width; (void)height;
    return 0;
}

int virtio_gpu_update_cursor(virtio_gpu_t* dev, uint32_t scanout_id, uint32_t resource_id,
                             uint32_t x, uint32_t y, uint32_t hot_x, uint32_t hot_y)
{
    if (!dev || !dev->initialized) return -1;
    (void)scanout_id; (void)resource_id; (void)x; (void)y; (void)hot_x; (void)hot_y;
    return 0;
}

int virtio_gpu_move_cursor(virtio_gpu_t* dev, uint32_t x, uint32_t y)
{
    if (!dev || !dev->initialized) return -1;
    (void)x; (void)y;
    return 0;
}

void virtio_gpu_irq_handler(void)
{
}

uint8_t virtio_gpu_get_count(void)
{
    return virtio_gpu_count;
}