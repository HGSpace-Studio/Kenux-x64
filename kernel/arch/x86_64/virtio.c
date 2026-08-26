

#include <arch/virtio.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>
#include <slab.h>

static int __vq_alloc(virtqueue_t* vq, uint16_t num)
{

    vq->num = num;
    vq->descs = (virtq_desc_t*)kzalloc(sizeof(virtq_desc_t) * num);
    if (!vq->descs) return -1;

    vq->avail = (virtq_avail_t*)kzalloc(2 + 2 + 2 * num);
    if (!vq->avail) return -1;

    vq->used = (virtq_used_t*)kzalloc(2 + 2 + 8 * num);
    if (!vq->used) return -1;

    for (uint16_t i = 0; i < num; i++) {
        vq->descs[i].next = (i + 1) % num;
    }
    vq->free_head = 0;
    vq->avail_idx = 0;
    vq->last_used_idx = 0;

    spin_init(&vq->lock);
    return 0;
}

static uint16_t __vq_get_desc(virtqueue_t* vq)
{
    uint16_t idx = vq->free_head;
    vq->free_head = vq->descs[idx].next;
    return idx;
}

static void __vq_add_avail(virtqueue_t* vq, uint16_t desc_idx)
{
    uint16_t avail_idx = vq->avail_idx % vq->num;
    vq->avail->ring[avail_idx] = desc_idx;
    vq->avail_idx++;
    __sync_synchronize();
}

static void __vq_kick(virtqueue_t* vq, volatile uint32_t* notify)
{
    __sync_synchronize();
    (void)vq;
    *notify = 1;
}

static int __vq_get_used(virtqueue_t* vq, uint16_t* id, uint32_t* len)
{
    if (vq->last_used_idx == (uint16_t)vq->used->idx) return 0;

    uint16_t idx = vq->last_used_idx % vq->num;
    virtq_used_elem_t* elem = &vq->used->ring[idx];
    *id = elem->id;
    *len = elem->len;
    vq->last_used_idx++;
    return 1;
}

void virtio_init(void)
{
    uint64_t probe_addrs[] = {
        0xA000000,
        0xA003800,
        0xA004000,
        0xA004200,
    };
    for (size_t i = 0; i < sizeof(probe_addrs) / sizeof(probe_addrs[0]); i++) {
        virtio_mmio_t *mmio = (virtio_mmio_t *)probe_addrs[i];
        if (mmio->magic == 0x74726976) {
            mmio->status = VIRTIO_STATUS_ACK;
            mmio->status |= VIRTIO_STATUS_DRIVER;
        }
    }
}

int virtio_blk_init(virtio_blk_device_t* dev, virtio_mmio_t* mmio)
{
    if (!dev || !mmio) return -1;
    memset(dev, 0, sizeof(virtio_blk_device_t));
    dev->mmio = mmio;

    mmio->queue_select = 0;
    uint16_t queue_size = mmio->queue_size;
    if (queue_size == 0 || queue_size > 1024) queue_size = 128;

    if (__vq_alloc(&dev->vq, queue_size) != 0) return -1;

    mmio->queue_desc_low  = (uint32_t)(uint64_t)dev->vq.descs;
    mmio->queue_desc_high  = (uint32_t)((uint64_t)dev->vq.descs >> 32);
    mmio->queue_avail_low = (uint32_t)(uint64_t)dev->vq.avail;
    mmio->queue_avail_high = (uint32_t)((uint64_t)dev->vq.avail >> 32);
    mmio->queue_used_low  = (uint32_t)(uint64_t)dev->vq.used;
    mmio->queue_used_high  = (uint32_t)((uint64_t)dev->vq.used >> 32);

    mmio->queue_enable = 1;

    mmio->status |= VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_OK;

    dev->config = (virtio_blk_config_t*)((uint8_t*)mmio + mmio->config_offset);

    dev->ready = 1;
    spin_init(&dev->lock);
    return 0;
}

static int __blk_submit(virtio_blk_device_t* dev, uint32_t type, uint64_t sector,
                        void* data, uint32_t len)
{
    virtqueue_t* vq = &dev->vq;
    spin_lock(&vq->lock);

    uint16_t hdr_idx = __vq_get_desc(vq);
    uint16_t data_idx = __vq_get_desc(vq);
    uint16_t status_idx = __vq_get_desc(vq);

    virtio_blk_req_t* req = (virtio_blk_req_t*)kzalloc(sizeof(virtio_blk_req_t));
    if (!req) { spin_unlock(&vq->lock); return -1; }
    req->type = type;
    req->sector = sector;

    vq->descs[hdr_idx].addr = (uint64_t)req;
    vq->descs[hdr_idx].len = sizeof(virtio_blk_req_t);
    vq->descs[hdr_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[hdr_idx].next = data_idx;

    vq->descs[data_idx].addr = (uint64_t)data;
    vq->descs[data_idx].len = len;
    vq->descs[data_idx].flags = (type == VIRTIO_BLK_T_OUT) ? 0 : VIRTQ_DESC_F_WRITE;
    vq->descs[data_idx].flags |= VIRTQ_DESC_F_NEXT;
    vq->descs[data_idx].next = status_idx;

    uint8_t* status = (uint8_t*)kzalloc(1);
    vq->descs[status_idx].addr = (uint64_t)status;
    vq->descs[status_idx].len = 1;
    vq->descs[status_idx].flags = VIRTQ_DESC_F_WRITE;
    vq->descs[status_idx].next = 0;

    __vq_add_avail(vq, hdr_idx);

    volatile uint32_t* notify = (volatile uint32_t*)((uint8_t*)dev->mmio +
        dev->mmio->queue_notify_off + 0x100);
    __vq_kick(vq, notify);

    spin_unlock(&vq->lock);

    uint16_t used_id;
    uint32_t used_len;
    for (int i = 0; i < 1000000; i++) {
        if (__vq_get_used(vq, &used_id, &used_len)) {
            kfree(req);
            kfree(status);
            return (*status == 0) ? 0 : -1;
        }
    }

    kfree(req);
    kfree(status);
    return -1;
}

int virtio_blk_read(virtio_blk_device_t* dev, uint64_t sector, void* buf, uint32_t nsects)
{
    if (!dev || !dev->ready) return -1;
    return __blk_submit(dev, VIRTIO_BLK_T_IN, sector, buf, nsects * 512);
}

int virtio_blk_write(virtio_blk_device_t* dev, uint64_t sector, const void* buf, uint32_t nsects)
{
    if (!dev || !dev->ready) return -1;
    return __blk_submit(dev, VIRTIO_BLK_T_OUT, sector, (void*)buf, nsects * 512);
}

int virtio_net_init(virtio_net_device_t* dev, virtio_mmio_t* mmio)
{
    if (!dev || !mmio) return -1;
    memset(dev, 0, sizeof(virtio_net_device_t));
    dev->mmio = mmio;

    mmio->queue_select = 0;
    uint16_t rx_size = mmio->queue_size;
    if (rx_size == 0 || rx_size > 1024) rx_size = 256;

    if (__vq_alloc(&dev->rx_vq, rx_size) != 0) return -1;

    mmio->queue_select = 1;
    uint16_t tx_size = mmio->queue_size;
    if (tx_size == 0 || tx_size > 1024) tx_size = 256;

    if (__vq_alloc(&dev->tx_vq, tx_size) != 0) return -1;

    mmio->queue_select = 0;
    mmio->queue_desc_low  = (uint32_t)(uint64_t)dev->rx_vq.descs;
    mmio->queue_desc_high  = (uint32_t)((uint64_t)dev->rx_vq.descs >> 32);
    mmio->queue_avail_low = (uint32_t)(uint64_t)dev->rx_vq.avail;
    mmio->queue_avail_high = (uint32_t)((uint64_t)dev->rx_vq.avail >> 32);
    mmio->queue_used_low  = (uint32_t)(uint64_t)dev->rx_vq.used;
    mmio->queue_used_high  = (uint32_t)((uint64_t)dev->rx_vq.used >> 32);
    mmio->queue_enable = 1;

    mmio->queue_select = 1;
    mmio->queue_desc_low  = (uint32_t)(uint64_t)dev->tx_vq.descs;
    mmio->queue_desc_high  = (uint32_t)((uint64_t)dev->tx_vq.descs >> 32);
    mmio->queue_avail_low = (uint32_t)(uint64_t)dev->tx_vq.avail;
    mmio->queue_avail_high = (uint32_t)((uint64_t)dev->tx_vq.avail >> 32);
    mmio->queue_used_low  = (uint32_t)(uint64_t)dev->tx_vq.used;
    mmio->queue_used_high  = (uint32_t)((uint64_t)dev->tx_vq.used >> 32);
    mmio->queue_enable = 1;

    mmio->status |= VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_OK;

    dev->config = (virtio_net_config_t*)((uint8_t*)mmio + mmio->config_offset);
    dev->ready = 1;
    spin_init(&dev->lock);
    return 0;
}

int virtio_net_send(virtio_net_device_t* dev, const void* data, uint32_t len)
{
    if (!dev || !dev->ready || len > 1514) return -1;

    virtqueue_t* vq = &dev->tx_vq;
    spin_lock(&vq->lock);

    uint16_t hdr_idx = __vq_get_desc(vq);
    uint16_t data_idx = __vq_get_desc(vq);

    virtio_net_hdr_t* hdr = (virtio_net_hdr_t*)kzalloc(sizeof(virtio_net_hdr_t));
    hdr->flags = 0;
    hdr->gso_type = 0;

    vq->descs[hdr_idx].addr = (uint64_t)hdr;
    vq->descs[hdr_idx].len = sizeof(virtio_net_hdr_t);
    vq->descs[hdr_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[hdr_idx].next = data_idx;

    vq->descs[data_idx].addr = (uint64_t)data;
    vq->descs[data_idx].len = len;
    vq->descs[data_idx].flags = 0;
    vq->descs[data_idx].next = 0;

    __vq_add_avail(vq, hdr_idx);

    volatile uint32_t* notify = (volatile uint32_t*)((uint8_t*)dev->mmio +
        dev->mmio->queue_notify_off + 0x100 + 0x40);
    __vq_kick(vq, notify);

    spin_unlock(&vq->lock);

    kfree(hdr);
    return 0;
}

int virtio_net_recv(virtio_net_device_t* dev, void* buf, uint32_t* len)
{
    if (!dev || !dev->ready) return -1;

    virtqueue_t* vq = &dev->rx_vq;
    uint16_t used_id;
    uint32_t used_len;

    if (!__vq_get_used(vq, &used_id, &used_len)) {
        return 0;
    }

    virtq_desc_t* desc = &vq->descs[used_id];
    if (desc->len > 0 && buf) {
        uint32_t copy = desc->len < *len ? desc->len : *len;
        memcpy(buf, (void*)desc->addr, copy);
        *len = copy;
    }

    return 1;
}
