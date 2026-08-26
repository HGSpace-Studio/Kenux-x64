#ifndef ARCH_VLAN_H
#define ARCH_VLAN_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/net.h>

#define VLAN_MAX_ID            4094
#define VLAN_PRIORITY_SHIFT    13
#define VLAN_PRIORITY_MASK     0xE000
#define VLAN_VID_MASK          0x0FFF
#define VLAN_ETHERTYPE         0x8100
#define VLAN_TAG_INSERT_SIZE   4
#define VLAN_NAME_MAX          16
#define VLAN_MAX_DEVICES       64

#define VLAN_TAG_INSERT(pkt, len, max, vid, prio, proto)                \
    do {                                                                \
        uint8_t* _p = (uint8_t*)(pkt);                                   \
        uint64_t _l = (uint64_t)(len);                                   \
        uint64_t _m = (uint64_t)(max);                                   \
        if (_l + VLAN_TAG_INSERT_SIZE <= _m && _l >= sizeof(eth_header_t)) { \
            uint64_t _pl = _l - sizeof(eth_header_t);                     \
            memmove(_p + sizeof(eth_header_t) + VLAN_TAG_INSERT_SIZE,     \
                    _p + sizeof(eth_header_t), _pl);                      \
            uint16_t _tci = (uint16_t)((vid) & VLAN_VID_MASK) |          \
                             ((uint16_t)(prio) << VLAN_PRIORITY_SHIFT);   \
            _p[sizeof(eth_header_t)] = (uint8_t)(_tci >> 8);              \
            _p[sizeof(eth_header_t) + 1] = (uint8_t)(_tci & 0xff);       \
            _p[sizeof(eth_header_t) + 2] = (uint8_t)((proto) >> 8);     \
            _p[sizeof(eth_header_t) + 3] = (uint8_t)((proto) & 0xff);    \
            *(uint16_t*)(_p + 12) = 0x8100;                              \
            _l += VLAN_TAG_INSERT_SIZE;                                   \
            len = (typeof(len))_l;                                        \
        }                                                                 \
    } while(0)

#define VLAN_TAG_STRIP(pkt, len, vid_out, proto_out)                     \
    do {                                                                \
        const uint8_t* _p = (const uint8_t*)(pkt);                       \
        uint64_t _l = (uint64_t)(len);                                    \
        if (_l >= sizeof(eth_header_t) + VLAN_TAG_INSERT_SIZE &&         \
            *(uint16_t*)(_p + 12) == 0x8100) {                           \
            uint16_t _tci = ((uint16_t)_p[sizeof(eth_header_t)] << 8) |   \
                            (uint16_t)_p[sizeof(eth_header_t) + 1];      \
            uint16_t _ep = ((uint16_t)_p[sizeof(eth_header_t) + 2] << 8)| \
                           (uint16_t)_p[sizeof(eth_header_t) + 3];       \
            vid_out = _tci & VLAN_VID_MASK;                               \
            proto_out = _ep;                                               \
            memmove((uint8_t*)_p + sizeof(eth_header_t),                   \
                    _p + sizeof(eth_header_t) + VLAN_TAG_INSERT_SIZE,       \
                    _l - sizeof(eth_header_t) - VLAN_TAG_INSERT_SIZE);      \
            _l -= VLAN_TAG_INSERT_SIZE;                                   \
            len = (typeof(len))_l;                                         \
        }                                                                 \
    } while(0)

typedef struct vlan_device {
    char name[VLAN_NAME_MAX];
    char parent[VLAN_NAME_MAX];
    uint16_t vlan_id;
    uint8_t mac[ETH_ALEN];
    uint32_t flags;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_drops;
    uint32_t tx_drops;
    struct vlan_device* next;
} vlan_device_t;

typedef struct {
    vlan_device_t* devices;
    uint32_t count;
    spinlock_t lock;
} vlan_table_t;

void vlan_init(void);
vlan_device_t* vlan_add_device(const char* parent, uint16_t vlan_id, const char* name);
int vlan_del_device(const char* name);
vlan_device_t* vlan_find_device(const char* name);
vlan_device_t* vlan_find_by_vid(const char* parent, uint16_t vlan_id);
vlan_device_t* vlan_get_device_by_index(uint32_t index);
int vlan_rx(const uint8_t* frame, uint64_t len, const char* parent);
int vlan_tx(uint8_t* frame, uint64_t* len, uint64_t max_len,
             const char* vlan_name, uint16_t proto);
uint32_t vlan_device_count(void);

#endif
