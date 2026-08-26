

#ifndef ARCH_VIRTIO_H
#define ARCH_VIRTIO_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define VIRTIO_DEV_BLK      1
#define VIRTIO_DEV_NET      2
#define VIRTIO_DEV_CONSOLE  3
#define VIRTIO_DEV_GPU      16

#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_OK        4
#define VIRTIO_STATUS_FAILED    128

typedef struct virtio_mmio {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t device_id;
    volatile uint32_t vendor_id;
    volatile uint32_t device_features;
    volatile uint32_t device_features_sel;
    volatile uint32_t driver_features;
    volatile uint32_t driver_features_sel;
    volatile uint32_t config_guest_page_size;
    volatile uint32_t config_offset;
    volatile uint32_t config_generation;
    volatile uint32_t status;
    volatile uint32_t queue_select;
    volatile uint32_t queue_size;
    volatile uint32_t queue_msix_vector;
    volatile uint32_t queue_enable;
    volatile uint32_t queue_notify_off;
    volatile uint32_t queue_desc_low;
    volatile uint32_t queue_desc_high;
    volatile uint32_t queue_avail_low;
    volatile uint32_t queue_avail_high;
    volatile uint32_t queue_used_low;
    volatile uint32_t queue_used_high;
} virtio_mmio_t;

#define VIRTQ_DESC_F_NEXT    0x01
#define VIRTQ_DESC_F_WRITE   0x02
#define VIRTQ_DESC_F_INDIRECT 0x04

typedef struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} virtq_avail_t;

typedef struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];
} virtq_used_t;

typedef struct virtqueue {
    virtq_desc_t* descs;
    virtq_avail_t* avail;
    virtq_used_t*  used;

    uint16_t num;
    uint16_t free_head;
    uint16_t last_used_idx;
    uint16_t avail_idx;

    spinlock_t lock;
} virtqueue_t;

#define VIRTIO_BLK_F_RO      0x01
#define VIRTIO_BLK_F_SEG     0x02
#define VIRTIO_BLK_F_FLUSH   0x04
#define VIRTIO_BLK_F_BARRIER 0x08

#define VIRTIO_BLK_T_IN       0
#define VIRTIO_BLK_T_OUT      1
#define VIRTIO_BLK_T_FLUSH    4

typedef struct virtio_blk_config {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t blk_size;
    uint8_t  physical_block_exp;
    uint8_t  alignment_offset;
    uint8_t  min_io_size;
    uint8_t  opt_io_size;
    uint32_t wce;
    uint8_t  unused[3];
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t  write_zeroes_may_discard;
    uint8_t  unused1[3];
} virtio_blk_config_t;

typedef struct virtio_blk_req {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
    uint8_t  data[];
} virtio_blk_req_t;

typedef struct virtio_blk_device {
    virtio_mmio_t*      mmio;
    virtio_blk_config_t* config;
    virtqueue_t          vq;
    uint32_t             features;
    int                  ready;
    spinlock_t           lock;
} virtio_blk_device_t;

#define VIRTIO_NET_F_CSUM    0x01
#define VIRTIO_NET_F_GSO     0x02
#define VIRTIO_NET_F_MAC     0x04
#define VIRTIO_NET_F_STATUS  0x10
#define VIRTIO_NET_F_MRG_RXBUF 0x20

typedef struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t  duplex;
    uint8_t  rss_max_key_size;
    uint16_t rss_max_data_size;
    uint32_t supported_hash_types;
} virtio_net_config_t;

typedef struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} virtio_net_hdr_t;

typedef struct virtio_net_device {
    virtio_mmio_t*      mmio;
    virtio_net_config_t* config;
    virtqueue_t          rx_vq;
    virtqueue_t          tx_vq;
    uint32_t             features;
    int                  ready;
    spinlock_t           lock;
} virtio_net_device_t;

void virtio_init(void);

int virtio_blk_init(virtio_blk_device_t* dev, virtio_mmio_t* mmio);
int virtio_blk_read(virtio_blk_device_t* dev, uint64_t sector, void* buf, uint32_t nsects);
int virtio_blk_write(virtio_blk_device_t* dev, uint64_t sector, const void* buf, uint32_t nsects);

int virtio_net_init(virtio_net_device_t* dev, virtio_mmio_t* mmio);
int virtio_net_send(virtio_net_device_t* dev, const void* data, uint32_t len);
int virtio_net_recv(virtio_net_device_t* dev, void* buf, uint32_t* len);

#endif
