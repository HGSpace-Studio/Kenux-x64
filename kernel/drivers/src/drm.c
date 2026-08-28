#include "drm.h"
#include <arch/memory.h>
#include <string.h>

void drm_device_init(drm_device_t* dev)
{
    if (!dev) return;
    memset(dev, 0, sizeof(drm_device_t));
    spin_init(&dev->lock);
    dev->obj_id_counter = 1;
}

int drm_crtc_set_mode(drm_device_t* dev, int crtc, const drm_mode_t* mode, uint32_t fb, uint32_t x, uint32_t y)
{
    if (!dev || !mode || crtc < 0 || crtc >= dev->crtc_count) return -1;
    spinlock_acquire(&dev->crtcs[crtc].lock);
    dev->crtcs[crtc].mode = *mode;
    dev->crtcs[crtc].x = x;
    dev->crtcs[crtc].y = y;
    dev->crtcs[crtc].framebuffer = (void*)(uintptr_t)fb;
    dev->crtcs[crtc].enabled = 1;
    spinlock_release(&dev->crtcs[crtc].lock);
    return 0;
}

int drm_crtc_disable(drm_device_t* dev, int crtc)
{
    if (!dev || crtc < 0 || crtc >= dev->crtc_count) return -1;
    spinlock_acquire(&dev->crtcs[crtc].lock);
    dev->crtcs[crtc].enabled = 0;
    dev->crtcs[crtc].framebuffer = NULL;
    spinlock_release(&dev->crtcs[crtc].lock);
    return 0;
}

int drm_connector_detect(drm_device_t* dev, int conn)
{
    if (!dev || conn < 0 || conn >= dev->connector_count) return -1;
    spinlock_acquire(&dev->connectors[conn].lock);
    int status = dev->connectors[conn].status;
    spinlock_release(&dev->connectors[conn].lock);
    return status;
}

int drm_connector_get_modes(drm_device_t* dev, int conn)
{
    if (!dev || conn < 0 || conn >= dev->connector_count) return -1;
    return dev->connectors[conn].mode_count;
}

int drm_encoder_set_crtc(drm_device_t* dev, int enc, int crtc)
{
    if (!dev || enc < 0 || enc >= dev->encoder_count) return -1;
    spinlock_acquire(&dev->encoders[enc].lock);
    dev->encoders[enc].crtc_id = crtc;
    spinlock_release(&dev->encoders[enc].lock);
    return 0;
}

static void virtio_gpu_send_cmd(virtio_gpu_dev_t* dev, void* cmd, uint32_t size)
{
    (void)dev; (void)cmd; (void)size;
}

static void virtio_gpu_recv_resp(virtio_gpu_dev_t* dev, void* resp, uint32_t size)
{
    (void)dev; (void)resp; (void)size;
}

int virtio_gpu_init(virtio_gpu_dev_t* dev, pci_device_t* pci_dev)
{
    if (!dev || !pci_dev) return -1;
    memset(dev, 0, sizeof(virtio_gpu_dev_t));
    spin_init(&dev->lock);

    dev->pci_dev = pci_dev;
    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    dev->mmio = (volatile uint8_t*)pci_map_bar(pci_dev, 0);
    if (!dev->mmio) return -2;

    drm_device_init(&dev->drm);

    virtio_gpu_ctrl_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.hdr_type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    virtio_gpu_send_cmd(dev, &hdr, sizeof(hdr));
    virtio_gpu_recv_resp(dev, &dev->display_info, sizeof(dev->display_info));

    if (dev->display_info.hdr.hdr_type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        for (uint32_t i = 0; i < dev->display_info.num_pmodes && i < DRM_MAX_CRTCS; i++) {
            virtio_gpu_display_one_t* pm = &dev->display_info.pmodes[i];
            drm_crtc_t* crtc = &dev->drm.crtcs[i];
            crtc->index = (int)i;
            crtc->fb_width = pm->width;
            crtc->fb_height = pm->height;
            crtc->fb_bpp = 32;
            crtc->fb_pitch = pm->width * 4;
            crtc->base.id = dev->drm.obj_id_counter++;
            dev->drm.crtc_count++;

            drm_connector_t* conn = &dev->drm.connectors[i];
            conn->index = (int)i;
            conn->type = DRM_CONNECTOR_VIRTUAL;
            conn->status = pm->enabled ? DRM_CONN_STATUS_CONNECTED : DRM_CONN_STATUS_DISCONNECTED;
            conn->base.id = dev->drm.obj_id_counter++;
            if (pm->enabled) {
                drm_mode_t* mode = &conn->modes[0];
                mode->hdisplay = pm->width;
                mode->vdisplay = pm->height;
                mode->clock = 0;
                mode->type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_BUILTIN;
                snprintf(mode->name, 32, "%dx%d", pm->width, pm->height);
                conn->mode_count = 1;
                conn->preferred_mode = 0;
            }
            dev->drm.connector_count++;

            drm_encoder_t* enc = &dev->drm.encoders[i];
            enc->index = (int)i;
            enc->possible_crtcs = 1 << i;
            enc->base.id = dev->drm.obj_id_counter++;
            dev->drm.encoder_count++;
        }
    }

    return 0;
}

void virtio_gpu_shutdown(virtio_gpu_dev_t* dev)
{
    if (!dev) return;
    for (uint32_t i = 0; i < dev->fb_count; i++) {
        virtio_gpu_ctrl_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.hdr_type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
        virtio_gpu_resource_create_2d_t cmd;
        cmd.hdr = hdr;
        cmd.resource_id = i + 1;
        virtio_gpu_send_cmd(dev, &cmd, sizeof(cmd));
    }
}

int virtio_gpu_create_fb(virtio_gpu_dev_t* dev, uint32_t width, uint32_t height, uint32_t* resource_id)
{
    if (!dev || !resource_id) return -1;
    spinlock_acquire(&dev->lock);

    uint32_t res_id = ++dev->resource_id_counter;
    *resource_id = res_id;

    virtio_gpu_resource_create_2d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.hdr_type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd.resource_id = res_id;
    cmd.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    cmd.width = width;
    cmd.height = height;
    virtio_gpu_send_cmd(dev, &cmd, sizeof(cmd));

    void* fb = memory_alloc_aligned((uint32_t)(width * height * 4), 4096);
    if (!fb) { spinlock_release(&dev->lock); return -2; }

    if (dev->fb_count < 16) dev->framebuffers[dev->fb_count++] = fb;

    virtio_gpu_resource_attach_backing_t attach;
    memset(&attach, 0, sizeof(attach));
    attach.hdr.hdr_type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach.resource_id = res_id;
    attach.nr_entries = 1;

    virtio_gpu_mem_entry_t entry;
    entry.addr = (uint64_t)(uintptr_t)fb;
    entry.length = width * height * 4;
    entry.padding = 0;

    uint8_t buf[sizeof(attach) + sizeof(entry)];
    memcpy(buf, &attach, sizeof(attach));
    memcpy(buf + sizeof(attach), &entry, sizeof(entry));
    virtio_gpu_send_cmd(dev, buf, sizeof(buf));

    spinlock_release(&dev->lock);
    return 0;
}

int virtio_gpu_set_scanout(virtio_gpu_dev_t* dev, uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);

    virtio_gpu_set_scanout_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.hdr_type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd.scanout_id = scanout;
    cmd.resource_id = resource_id;
    cmd.x = 0;
    cmd.y = 0;
    cmd.width = width;
    cmd.height = height;
    virtio_gpu_send_cmd(dev, &cmd, sizeof(cmd));

    spinlock_release(&dev->lock);
    return 0;
}

int virtio_gpu_transfer(virtio_gpu_dev_t* dev, uint32_t resource_id, const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!dev || !data) return -1;
    spinlock_acquire(&dev->lock);

    if (resource_id > 0 && resource_id <= dev->fb_count) {
        void* fb = dev->framebuffers[resource_id - 1];
        if (fb) {
            uint32_t fb_width = dev->display_info.pmodes[0].width;
            for (uint32_t row = 0; row < h; row++) {
                uint32_t* dst = (uint32_t*)fb + ((y + row) * fb_width + x);
                const uint32_t* src = (const uint32_t*)data + (row * w);
                memcpy(dst, src, w * 4);
            }
        }
    }

    virtio_gpu_transfer_to_host_2d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.hdr_type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd.resource_id = resource_id;
    cmd.x = x;
    cmd.y = y;
    cmd.width = w;
    cmd.height = h;
    cmd.offset = 0;
    virtio_gpu_send_cmd(dev, &cmd, sizeof(cmd));

    spinlock_release(&dev->lock);
    return 0;
}

int virtio_gpu_flush(virtio_gpu_dev_t* dev, uint32_t resource_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);

    virtio_gpu_transfer_to_host_2d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.hdr_type = VIRTIO_GPU_CMD_FLUSH;
    cmd.resource_id = resource_id;
    cmd.x = x;
    cmd.y = y;
    cmd.width = w;
    cmd.height = h;
    virtio_gpu_send_cmd(dev, &cmd, sizeof(cmd));

    spinlock_release(&dev->lock);
    return 0;
}

void virtio_gpu_irq_handler(int irq, void* ctx)
{
    virtio_gpu_dev_t* dev = (virtio_gpu_dev_t*)ctx;
    if (!dev) return;
    (void)irq;
}