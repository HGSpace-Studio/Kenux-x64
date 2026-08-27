#ifndef KERNEL_DRIVERS_DRM_H
#define KERNEL_DRIVERS_DRM_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define DRM_MAX_CRTCS       4
#define DRM_MAX_ENCODERS    8
#define DRM_MAX_CONNECTORS  8
#define DRM_MAX_PLANES      8
#define DRM_MAX_MODES       32

typedef struct {
    uint16_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;
    uint32_t flags;
    uint32_t type;
    char     name[32];
} drm_mode_t;

#define DRM_MODE_FLAG_PHSYNC   0x01
#define DRM_MODE_FLAG_NHSYNC   0x02
#define DRM_MODE_FLAG_PVSYNC   0x04
#define DRM_MODE_FLAG_NVSYNC   0x08
#define DRM_MODE_FLAG_INTERLACE 0x10
#define DRM_MODE_FLAG_DBLSCAN  0x20

#define DRM_MODE_TYPE_BUILTIN  0x01
#define DRM_MODE_TYPE_PREFERRED 0x02

#define DRM_CONNECTOR_VGA      0
#define DRM_CONNECTOR_DVII     1
#define DRM_CONNECTOR_DVID     2
#define DRM_CONNECTOR_DVIA     3
#define DRM_CONNECTOR_COMPOSITE 4
#define DRM_CONNECTOR_SVIDEO   5
#define DRM_CONNECTOR_LVDS     6
#define DRM_CONNECTOR_HDMIA    7
#define DRM_CONNECTOR_HDMIB    8
#define DRM_CONNECTOR_DISPLAYPORT 9
#define DRM_CONNECTOR_VIRTUAL  10

#define DRM_CONN_STATUS_CONNECTED    1
#define DRM_CONN_STATUS_DISCONNECTED 2
#define DRM_CONN_STATUS_UNKNOWN      3

typedef struct drm_object drm_object_t;
struct drm_object {
    uint32_t id;
    uint32_t type;
    void*    priv;
};

typedef struct {
    drm_object_t     base;
    int              index;
    int              enabled;
    uint32_t         x;
    uint32_t         y;
    drm_mode_t       mode;
    void*            framebuffer;
    uint32_t         fb_width;
    uint32_t         fb_height;
    uint32_t         fb_pitch;
    uint32_t         fb_bpp;
    spinlock_t       lock;
} drm_crtc_t;

typedef struct {
    drm_object_t     base;
    int              index;
    int              type;
    int              crtc_id;
    int              possible_crtcs;
    int              status;
    drm_mode_t       modes[DRM_MAX_MODES];
    int              mode_count;
    int              preferred_mode;
    uint32_t         edid[128];
    spinlock_t       lock;
} drm_connector_t;

typedef struct {
    drm_object_t     base;
    int              index;
    int              crtc_id;
    int              connector_id;
    int              possible_crtcs;
    int              possible_clones;
    spinlock_t       lock;
} drm_encoder_t;

typedef struct {
    drm_object_t     base;
    int              index;
    int              crtc_id;
    uint32_t         format;
    spinlock_t       lock;
} drm_plane_t;

typedef struct {
    drm_crtc_t       crtcs[DRM_MAX_CRTCS];
    int              crtc_count;
    drm_connector_t  connectors[DRM_MAX_CONNECTORS];
    int              connector_count;
    drm_encoder_t    encoders[DRM_MAX_ENCODERS];
    int              encoder_count;
    drm_plane_t      planes[DRM_MAX_PLANES];
    int              plane_count;
    uint32_t         obj_id_counter;
    spinlock_t       lock;
} drm_device_t;

void  drm_device_init(drm_device_t* dev);
int   drm_crtc_set_mode(drm_device_t* dev, int crtc, const drm_mode_t* mode, uint32_t fb, uint32_t x, uint32_t y);
int   drm_crtc_disable(drm_device_t* dev, int crtc);
int   drm_connector_detect(drm_device_t* dev, int conn);
int   drm_connector_get_modes(drm_device_t* dev, int conn);
int   drm_encoder_set_crtc(drm_device_t* dev, int enc, int crtc);

#define VIRTIO_GPU_PCI_VENDOR  0x1AF4
#define VIRTIO_GPU_PCI_DEVICE  0x1050

#define VIRTIO_GPU_CTRL_QUEUE    0
#define VIRTIO_GPU_CURSOR_QUEUE  1

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO  0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF    0x0102
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107
#define VIRTIO_GPU_CMD_SET_SCANOUT      0x0108
#define VIRTIO_GPU_CMD_FLUSH            0x0109
#define VIRTIO_GPU_CMD_UPDATE_CURSOR    0x0110
#define VIRTIO_GPU_CMD_MOVE_CURSOR      0x0111

#define VIRTIO_GPU_RESP_OK_NODATA       0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2

typedef struct {
    uint32_t hdr_type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} virtio_gpu_ctrl_hdr_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t scanout_id;
    uint32_t resource_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_set_scanout_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t resource_id;
    uint32_t offset;
} virtio_gpu_transfer_to_host_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_resource_attach_backing_t;

typedef struct {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t enabled;
    uint32_t flags;
} virtio_gpu_display_one_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[16];
    uint32_t num_pmodes;
} virtio_gpu_resp_display_info_t;

typedef struct {
    pci_device_t*        pci_dev;
    volatile uint8_t*    mmio;
    uint32_t             resource_id_counter;
    uint32_t             fence_id_counter;
    virtio_gpu_resp_display_info_t display_info;
    drm_device_t         drm;
    void*                framebuffers[16];
    uint32_t             fb_count;
    spinlock_t           lock;
} virtio_gpu_dev_t;

int  virtio_gpu_init(virtio_gpu_dev_t* dev, pci_device_t* pci_dev);
void virtio_gpu_shutdown(virtio_gpu_dev_t* dev);
int  virtio_gpu_create_fb(virtio_gpu_dev_t* dev, uint32_t width, uint32_t height, uint32_t* resource_id);
int  virtio_gpu_set_scanout(virtio_gpu_dev_t* dev, uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height);
int  virtio_gpu_transfer(virtio_gpu_dev_t* dev, uint32_t resource_id, const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
int  virtio_gpu_flush(virtio_gpu_dev_t* dev, uint32_t resource_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void virtio_gpu_irq_handler(int irq, void* ctx);

#endif