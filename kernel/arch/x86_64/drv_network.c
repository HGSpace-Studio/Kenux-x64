#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>
#include <stdio.h>

extern int e1000_driver_init(void);
extern int e1000_get_mac(uint8_t* mac_out);
extern int e1000_is_link_up(void);
extern int e1000_tx_packet_from_driver(const void* data, uint64_t len);

#define NDIS_MAX_ADAPTERS   8
#define RX_QUEUE_SIZE       64
#define MAX_PACKET_SIZE     1514
#define DEFAULT_MTU         1500

typedef struct _RX_QUEUE_ENTRY {
    uint8_t  data[MAX_PACKET_SIZE];
    uint32_t length;
    int      valid;
} RX_QUEUE_ENTRY;

static RX_QUEUE_ENTRY g_rx_queues[NDIS_MAX_ADAPTERS][RX_QUEUE_SIZE];
static uint32_t       g_rx_head[NDIS_MAX_ADAPTERS];
static uint32_t       g_rx_tail[NDIS_MAX_ADAPTERS];
static uint32_t       g_rx_count[NDIS_MAX_ADAPTERS];

static NDIS_ADAPTER g_adapter_pool[NDIS_MAX_ADAPTERS];
static int          g_adapter_count;
static int          g_network_initialized;
static int          g_e1000_available;
static int          g_e1000_adapter_id;
static spinlock_t   g_net_lock = SPINLOCK_INIT;

static NDIS_ADAPTER* ndis_lookup(uint32_t adapter_id) {
    if (adapter_id >= (uint32_t)NDIS_MAX_ADAPTERS) return NULL;
    if (g_adapter_pool[adapter_id].name[0] == 0) return NULL;
    return &g_adapter_pool[adapter_id];
}

static void ndis_reset_rx_queue(uint32_t adapter_id) {
    memset(g_rx_queues[adapter_id], 0, sizeof(g_rx_queues[adapter_id]));
    g_rx_head[adapter_id]  = 0;
    g_rx_tail[adapter_id]  = 0;
    g_rx_count[adapter_id] = 0;
}

static int e1000_net_tx(const void* data, uint64_t len) {
    if (!g_e1000_available) return -1;
    extern int e1000_tx_packet_from_driver(const void* data, uint64_t len);
    return e1000_tx_packet_from_driver(data, len);
}

int network_driver_init(void) {
    memset(g_adapter_pool, 0, sizeof(g_adapter_pool));
    for (int i = 0; i < NDIS_MAX_ADAPTERS; i++) {
        ndis_reset_rx_queue((uint32_t)i);
    }
    g_adapter_count       = 0;
    g_network_initialized = 0;
    g_e1000_available     = 0;
    g_e1000_adapter_id    = -1;

    spin_init(&g_net_lock);

    if (e1000_driver_init() == 0) {
        uint8_t mac[6];
        if (e1000_get_mac(mac) == 0) {
            int id = ndis_register_adapter("Intel Gigabit Ethernet", mac, DEFAULT_MTU);
            if (id >= 0) {
                g_e1000_adapter_id = id;
                g_e1000_available = 1;
                g_adapter_pool[id].link_speed = 1000000000;
                g_adapter_pool[id].link_up = e1000_is_link_up();
                
                extern void net_register_tx(int (*tx_func)(const void*, uint64_t));
                net_register_tx(e1000_net_tx);
            }
        }
    }

    if (!g_e1000_available) {
        const uint8_t loopback_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
        int id = ndis_register_adapter("Loopback Adapter", loopback_mac, DEFAULT_MTU);
        if (id >= 0) {
            g_adapter_pool[id].link_speed = 100000000;
            g_adapter_pool[id].link_up    = 1;
        }
    }

    g_network_initialized = 1;
    return 0;
}

int ndis_register_adapter(const char* name, const uint8_t mac[6], uint32_t mtu) {
    int slot;
    NDIS_ADAPTER* dev;
    size_t nlen;

    if (name == NULL || mac == NULL) return -1;
    if (mtu == 0) mtu = DEFAULT_MTU;

    for (slot = 0; slot < NDIS_MAX_ADAPTERS; slot++) {
        if (g_adapter_pool[slot].name[0] == 0) break;
    }
    if (slot >= NDIS_MAX_ADAPTERS) return -1;

    dev = &g_adapter_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    memcpy(dev->mac_addr, mac, 6);

    dev->mtu         = mtu;
    dev->link_speed  = 0;
    dev->link_up     = 0;
    dev->promiscuous = 0;
    dev->rx_packets  = 0;
    dev->tx_packets  = 0;
    dev->rx_bytes    = 0;
    dev->tx_bytes    = 0;
    dev->rx_errors   = 0;
    dev->tx_errors   = 0;

    ndis_reset_rx_queue((uint32_t)slot);

    g_adapter_count++;
    return slot;
}

int ndis_send_packet(uint32_t adapter_id, const void* data, uint32_t length) {
    NDIS_ADAPTER* dev;

    if (data == NULL || length == 0) return -1;
    dev = ndis_lookup(adapter_id);
    if (dev == NULL) return -1;
    if (!dev->link_up) return -1;

    dev->tx_packets++;
    dev->tx_bytes += length;

    if (g_e1000_available && (int)adapter_id == g_e1000_adapter_id) {
        return e1000_net_tx(data, length);
    }

    if (strcmp(dev->name, "Loopback Adapter") == 0) {
        if (g_rx_count[adapter_id] < RX_QUEUE_SIZE) {
            RX_QUEUE_ENTRY* entry = &g_rx_queues[adapter_id][g_rx_tail[adapter_id]];
            uint32_t copy_len = length < MAX_PACKET_SIZE ? length : MAX_PACKET_SIZE;
            memcpy(entry->data, data, copy_len);
            entry->length = copy_len;
            entry->valid = 1;
            g_rx_tail[adapter_id] = (g_rx_tail[adapter_id] + 1) % RX_QUEUE_SIZE;
            g_rx_count[adapter_id]++;
        }
        return (int)length;
    }

    return (int)length;
}

int ndis_receive_packet(uint32_t adapter_id, void* buf, uint32_t buf_size) {
    NDIS_ADAPTER* dev;
    RX_QUEUE_ENTRY* entry;
    uint32_t to_copy;

    if (buf == NULL || buf_size == 0) return -1;
    dev = ndis_lookup(adapter_id);
    if (dev == NULL) return -1;

    if (g_rx_count[adapter_id] == 0) return 0;

    entry = &g_rx_queues[adapter_id][g_rx_head[adapter_id]];

    to_copy = (entry->length < buf_size) ? entry->length : buf_size;
    memcpy(buf, entry->data, to_copy);

    dev->rx_packets++;
    dev->rx_bytes += entry->length;

    entry->valid = 0;
    g_rx_head[adapter_id] = (g_rx_head[adapter_id] + 1u) % RX_QUEUE_SIZE;
    g_rx_count[adapter_id]--;

    return (int)to_copy;
}

int ndis_inject_rx_packet(uint32_t adapter_id, const void* data, uint32_t length) {
    NDIS_ADAPTER* dev;
    RX_QUEUE_ENTRY* entry;
    uint32_t to_copy;

    if (data == NULL || length == 0) return -1;
    dev = ndis_lookup(adapter_id);
    if (dev == NULL) return -1;
    if (g_rx_count[adapter_id] >= (uint32_t)RX_QUEUE_SIZE) return -1;

    entry = &g_rx_queues[adapter_id][g_rx_tail[adapter_id]];

    to_copy = (length < (uint32_t)MAX_PACKET_SIZE) ? length : (uint32_t)MAX_PACKET_SIZE;
    memcpy(entry->data, data, to_copy);
    entry->length = to_copy;
    entry->valid  = 1;

    g_rx_tail[adapter_id] = (g_rx_tail[adapter_id] + 1u) % RX_QUEUE_SIZE;
    g_rx_count[adapter_id]++;

    return 0;
}

NDIS_ADAPTER* ndis_get_adapter(uint32_t adapter_id) {
    return ndis_lookup(adapter_id);
}

int ndis_get_adapter_count(void) {
    return g_adapter_count;
}

int ndis_set_promiscuous(uint32_t adapter_id, int enable) {
    NDIS_ADAPTER* dev;
    dev = ndis_lookup(adapter_id);
    if (dev == NULL) return -1;
    dev->promiscuous = enable ? 1 : 0;
    return 0;
}

void ndis_get_stats(uint32_t adapter_id, uint64_t* rx_pkts, uint64_t* tx_pkts,
                    uint64_t* rx_bytes, uint64_t* tx_bytes) {
    NDIS_ADAPTER* dev;
    dev = ndis_lookup(adapter_id);
    if (dev == NULL) {
        if (rx_pkts  != NULL) *rx_pkts  = 0;
        if (tx_pkts  != NULL) *tx_pkts  = 0;
        if (rx_bytes != NULL) *rx_bytes = 0;
        if (tx_bytes != NULL) *tx_bytes = 0;
        return;
    }
    if (rx_pkts  != NULL) *rx_pkts  = dev->rx_packets;
    if (tx_pkts  != NULL) *tx_pkts  = dev->tx_packets;
    if (rx_bytes != NULL) *rx_bytes = dev->rx_bytes;
    if (tx_bytes != NULL) *tx_bytes = dev->tx_bytes;
}