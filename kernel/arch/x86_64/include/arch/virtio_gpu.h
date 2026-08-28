#ifndef ARCH_X86_64_VIRTIO_GPU_H
#define ARCH_X86_64_VIRTIO_GPU_H

#include <arch/types.h>
#include <arch/pci.h>
#include <arch/spinlock.h>

#define VIRTIO_GPU_VENDOR_ID    0x1AF4
#define VIRTIO_GPU_DEVICE_ID    0x1050

#define VIRTIO_GPU_CTRL_QUEUE  0
#define VIRTIO_GPU_CURSOR_QUEUE 1

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO      0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D    0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF        0x0102
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D  0x0103
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0104
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0105
#define VIRTIO_GPU_CMD_SET_SCANOUT           0x0106
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH        0x0107
#define VIRTIO_GPU_CMD_UPDATE_CURSOR         0x0200
#define VIRTIO_GPU_CMD_MOVE_CURSOR           0x0201
#define VIRTIO_GPU_CMD_CURSOR_INFO           0x0202

#define VIRTIO_GPU_RESP_OK_NODATA           0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO     0x1101
#define VIRTIO_GPU_RESP_OK_RESOURCE_UUID    0x1102
#define VIRTIO_GPU_RESP_OK_CURSOR_INFO      0x1103
#define VIRTIO_GPU_RESP_ERR_UNSPEC          0x1200

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM   2
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM   1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM   3
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM   4

#define VIRTIO_GPU_MAX_SCANOUTS  16
#define VIRTIO_GPU_MAX_RESOURCES 256

typedef struct {
    uint32_t hdr_type;
    uint32_t flags;
    uint32_t fence_id;
    uint32_t ctx_id;
    uint8_t  _pad[8];
} virtio_gpu_ctrl_hdr_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        uint32_t width;
        uint32_t height;
        uint32_t flags;
        uint32_t _pad;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} virtio_gpu_resp_display_info_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_cmd_resource_create_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
} virtio_gpu_cmd_resource_unref_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint64_t offset;
} virtio_gpu_cmd_transfer_to_host_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_cmd_resource_attach_backing_t;

typedef struct {
    uint64_t addr;
    uint32_t length;
    uint32_t _pad;
} virtio_gpu_mem_entry_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
} virtio_gpu_cmd_resource_detach_backing_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t scanout_id;
    uint32_t resource_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_cmd_set_scanout_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_cmd_resource_flush_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t scanout_id;
    uint32_t resource_id;
    uint32_t x;
    uint32_t y;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t _pad;
} virtio_gpu_cmd_update_cursor_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t x;
    uint32_t y;
} virtio_gpu_cmd_move_cursor_t;

typedef struct {
    uint32_t resource_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    void*    backing;
    uint64_t backing_phys;
    uint32_t backing_size;
} virtio_gpu_resource_t;

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint64_t  mmio_base;
    uint8_t   irq;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint32_t  next_resource_id;
    uint32_t  scanout_count;
    struct {
        uint32_t width;
        uint32_t height;
        uint32_t resource_id;
        uint32_t x;
        uint32_t y;
    } scanouts[VIRTIO_GPU_MAX_SCANOUTS];
    virtio_gpu_resource_t resources[VIRTIO_GPU_MAX_RESOURCES];
    void*     ctrl_queue;
    void*     cursor_queue;
    spinlock_t lock;
    int       initialized;
} virtio_gpu_t;

void virtio_gpu_init(void);
virtio_gpu_t* virtio_gpu_get_device(uint8_t index);
int virtio_gpu_get_display_info(virtio_gpu_t* dev);
uint32_t virtio_gpu_create_resource_2d(virtio_gpu_t* dev, uint32_t width, uint32_t height, uint32_t format);
int virtio_gpu_unref_resource(virtio_gpu_t* dev, uint32_t resource_id);
int virtio_gpu_attach_backing(virtio_gpu_t* dev, uint32_t resource_id, void* data, uint32_t size);
int virtio_gpu_detach_backing(virtio_gpu_t* dev, uint32_t resource_id);
int virtio_gpu_set_scanout(virtio_gpu_t* dev, uint32_t scanout_id, uint32_t resource_id,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_transfer_to_host(virtio_gpu_t* dev, uint32_t resource_id,
                                uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_flush_resource(virtio_gpu_t* dev, uint32_t resource_id,
                              uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_update_cursor(virtio_gpu_t* dev, uint32_t scanout_id, uint32_t resource_id,
                             uint32_t x, uint32_t y, uint32_t hot_x, uint32_t hot_y);
int virtio_gpu_move_cursor(virtio_gpu_t* dev, uint32_t x, uint32_t y);
void virtio_gpu_irq_handler(void);
uint8_t virtio_gpu_get_count(void);

#endif