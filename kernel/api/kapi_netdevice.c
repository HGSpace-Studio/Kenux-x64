#include "kapi.h"

#include <arch/net.h>
#include <string.h>

struct kapi_netdev {
    char name[KAPI_NETDEV_NAME_LEN];
    uint32_t flags;
    uint32_t mtu;
    uint32_t media;
};

static struct kapi_netdev g_netdevs[3] = {
    { "eth0",  KAPI_NETDEV_UP | KAPI_NETDEV_BROADCAST | KAPI_NETDEV_MULTICAST, 1500, KAPI_CONN_MEDIA_ETHERNET },
    { "wlan0", 0, 1500, KAPI_CONN_MEDIA_WIFI },
    { "bt0",   0, 128,  KAPI_CONN_MEDIA_BLUETOOTH },
};

static uint32_t g_bluetooth_mouse_connected = 0;
static uint32_t g_ntp_synced = 0;
static uint64_t g_last_ntp_sync_ms = 0;

static void kapi_copy_string(char* dst, const char* src, size_t len)
{
    size_t i = 0;
    if (!dst || len == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int kapi_name_equal(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static void kapi_fill_eth_link(kapi_connectivity_link_t* link)
{
    net_status_t ns;
    net_get_status(&ns);

    memset(link, 0, sizeof(*link));
    kapi_copy_string(link->name, "eth0", sizeof(link->name));
    kapi_copy_string(link->label, "Ethernet / DHCP", sizeof(link->label));
    link->media = KAPI_CONN_MEDIA_ETHERNET;
    link->state = ns.tx_registered
        ? (ns.configured ? KAPI_CONN_STATE_CONNECTED : KAPI_CONN_STATE_CONNECTING)
        : KAPI_CONN_STATE_DISCONNECTED;
    link->ip_addr = ns.local_ip;
    link->gateway_ip = ns.gateway_ip;
    link->netmask = ns.netmask;
    link->rx_bytes = ns.rx_bytes;
    link->tx_bytes = ns.tx_bytes;
    link->rx_packets = ns.rx_frames;
    link->tx_packets = ns.tx_frames;
    memcpy(link->mac, ns.mac, KAPI_ETH_ALEN);
    if (!ns.tx_registered) {
        kapi_copy_string(link->detail, "No Ethernet TX driver registered", sizeof(link->detail));
    } else if (!ns.configured) {
        kapi_copy_string(link->detail, "DHCP discovery in progress", sizeof(link->detail));
    } else {
        kapi_copy_string(link->detail, "IP configured by DHCP/static config", sizeof(link->detail));
    }
}

static void kapi_fill_wifi_link(kapi_connectivity_link_t* link)
{
    memset(link, 0, sizeof(*link));
    kapi_copy_string(link->name, "wlan0", sizeof(link->name));
    kapi_copy_string(link->label, "Wi-Fi", sizeof(link->label));
    link->media = KAPI_CONN_MEDIA_WIFI;
    link->state = KAPI_CONN_STATE_READY;
    kapi_copy_string(link->detail, "KAPI ready; 802.11 driver not attached", sizeof(link->detail));
}

static void kapi_fill_bluetooth_link(kapi_connectivity_link_t* link)
{
    memset(link, 0, sizeof(*link));
    kapi_copy_string(link->name, "bt0", sizeof(link->name));
    kapi_copy_string(link->label, "Bluetooth HID", sizeof(link->label));
    link->media = KAPI_CONN_MEDIA_BLUETOOTH;
    link->state = g_bluetooth_mouse_connected ? KAPI_CONN_STATE_CONNECTED : KAPI_CONN_STATE_READY;
    kapi_copy_string(link->detail,
        g_bluetooth_mouse_connected ? "Bluetooth mouse connected" : "KAPI ready; HCI/HID driver not attached",
        sizeof(link->detail));
}

int kapi_netdevice_init(void)
{
    g_netdevs[0].flags = KAPI_NETDEV_UP | KAPI_NETDEV_BROADCAST | KAPI_NETDEV_MULTICAST;
    g_netdevs[1].flags = 0;
    g_netdevs[2].flags = 0;
    g_bluetooth_mouse_connected = 0;
    g_ntp_synced = 0;
    g_last_ntp_sync_ms = 0;
    return KAPI_OK;
}

kapi_netdev_t* kapi_netdev_open(const char* name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < 3; i++) {
        if (kapi_name_equal(name, g_netdevs[i].name)) {
            return &g_netdevs[i];
        }
    }
    return NULL;
}

int kapi_netdev_close(kapi_netdev_t* dev)
{
    return dev ? KAPI_OK : KAPI_EINVAL;
}

int kapi_netdev_up(kapi_netdev_t* dev)
{
    if (!dev) return KAPI_EINVAL;
    dev->flags |= KAPI_NETDEV_UP;
    return KAPI_OK;
}

int kapi_netdev_down(kapi_netdev_t* dev)
{
    if (!dev) return KAPI_EINVAL;
    dev->flags &= ~KAPI_NETDEV_UP;
    return KAPI_OK;
}

int kapi_netdev_get_mac(kapi_netdev_t* dev, uint8_t mac[KAPI_ETH_ALEN])
{
    if (!dev || !mac) return KAPI_EINVAL;
    if (dev->media == KAPI_CONN_MEDIA_ETHERNET) {
        net_status_t ns;
        net_get_status(&ns);
        memcpy(mac, ns.mac, KAPI_ETH_ALEN);
    } else {
        memset(mac, 0, KAPI_ETH_ALEN);
    }
    return KAPI_OK;
}

int kapi_netdev_set_mac(kapi_netdev_t* dev, const uint8_t mac[KAPI_ETH_ALEN])
{
    (void)mac;
    return dev ? KAPI_ENOSYS : KAPI_EINVAL;
}

int kapi_netdev_get_mtu(kapi_netdev_t* dev)
{
    return dev ? (int)dev->mtu : KAPI_EINVAL;
}

int kapi_netdev_set_mtu(kapi_netdev_t* dev, int mtu)
{
    if (!dev || mtu <= 0) return KAPI_EINVAL;
    dev->mtu = (uint32_t)mtu;
    return KAPI_OK;
}

uint32_t kapi_netdev_get_flags(kapi_netdev_t* dev)
{
    return dev ? dev->flags : 0;
}

int kapi_netdev_set_flags(kapi_netdev_t* dev, uint32_t flags)
{
    if (!dev) return KAPI_EINVAL;
    dev->flags = flags;
    return KAPI_OK;
}

int kapi_netdev_set_promiscuous(kapi_netdev_t* dev, int enable)
{
    if (!dev) return KAPI_EINVAL;
    if (enable) dev->flags |= KAPI_NETDEV_PROMISC;
    else dev->flags &= ~KAPI_NETDEV_PROMISC;
    return KAPI_OK;
}

int kapi_netdev_is_up(kapi_netdev_t* dev)
{
    return (dev && (dev->flags & KAPI_NETDEV_UP)) ? 1 : 0;
}

int kapi_netdev_get_stats(kapi_netdev_t* dev, kapi_netdev_stats_t* stats)
{
    if (!dev || !stats) return KAPI_EINVAL;
    memset(stats, 0, sizeof(*stats));
    if (dev->media == KAPI_CONN_MEDIA_ETHERNET) {
        net_status_t ns;
        net_get_status(&ns);
        stats->rx_packets = ns.rx_frames;
        stats->tx_packets = ns.tx_frames;
        stats->rx_bytes = ns.rx_bytes;
        stats->tx_bytes = ns.tx_bytes;
    }
    return KAPI_OK;
}

int kapi_netdev_send(kapi_netdev_t* dev, const void* data, size_t len)
{
    if (!dev || !data || len == 0) return KAPI_EINVAL;
    if (dev->media != KAPI_CONN_MEDIA_ETHERNET) return KAPI_ENOSYS;
    return net_tx_frame(data, (uint64_t)len);
}

int kapi_netdev_count(void)
{
    return 3;
}

int kapi_netdev_get_name(kapi_netdev_t* dev, char* name, size_t len)
{
    if (!dev || !name || len == 0) return KAPI_EINVAL;
    kapi_copy_string(name, dev->name, len);
    return KAPI_OK;
}

int kapi_connectivity_get_status(kapi_connectivity_status_t* status)
{
    if (!status) return KAPI_EINVAL;
    memset(status, 0, sizeof(*status));

    status->link_count = 3;
    status->wifi_ready = 1;
    status->bluetooth_ready = 1;
    status->ntp_ready = 1;
    status->ntp_synced = g_ntp_synced;
    status->last_ntp_sync_ms = g_last_ntp_sync_ms;
    status->bluetooth_mouse_connected = g_bluetooth_mouse_connected;

    kapi_fill_eth_link(&status->links[0]);
    kapi_fill_wifi_link(&status->links[1]);
    kapi_fill_bluetooth_link(&status->links[2]);

    status->ethernet_connected =
        (status->links[0].state == KAPI_CONN_STATE_CONNECTED) ? 1U : 0U;
    status->internet_available = status->ethernet_connected;

    return KAPI_OK;
}

int kapi_connectivity_mark_ntp_synced(uint64_t unix_ms)
{
    g_ntp_synced = 1;
    g_last_ntp_sync_ms = unix_ms;
    return KAPI_OK;
}

int kapi_connectivity_set_bluetooth_mouse(uint32_t connected)
{
    g_bluetooth_mouse_connected = connected ? 1U : 0U;
    return KAPI_OK;
}

int kapi_net_config(kapi_net_config_t* config)
{
    if (!config) return KAPI_EINVAL;
    memset(config, 0, sizeof(*config));

    net_status_t ns;
    net_get_status(&ns);
    config->flags = KAPI_NET_CONFIG_FLAG_PRESENT;
    if (ns.tx_registered) config->flags |= KAPI_NET_CONFIG_FLAG_ACTIVE;
    if (ns.configured) config->flags |= KAPI_NET_CONFIG_FLAG_DHCP;
    config->source = ns.configured ? KAPI_NET_CONFIG_SOURCE_DHCP : KAPI_NET_CONFIG_SOURCE_NONE;
    config->local_ip = ns.local_ip;
    config->subnet_mask = ns.netmask;
    config->gateway_ip = ns.gateway_ip;
    memcpy(config->mac, ns.mac, KAPI_ETH_ALEN);
    return KAPI_OK;
}

int kapi_net_dhcp_renew(uint32_t timeout_ms, kapi_net_dhcp_t* result)
{
    if (!result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    result->timeout_ms = timeout_ms;
    kapi_net_config(&result->config);
    result->status = (result->config.flags & KAPI_NET_CONFIG_FLAG_ACTIVE)
        ? KAPI_NET_STATUS_OK : KAPI_NET_STATUS_NO_DEVICE;
    return (result->status == KAPI_NET_STATUS_OK) ? KAPI_OK : KAPI_ENOSYS;
}

int kapi_net_ping(uint32_t target_ip, uint32_t timeout_ms, kapi_net_ping_t* result)
{
    if (!result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    result->target_ip = target_ip;
    result->timeout_ms = timeout_ms;
    result->sent = 1;
    result->status = KAPI_NET_STATUS_NO_DEVICE;
    return KAPI_ENOSYS;
}

int kapi_net_dns_resolve(const char* name, uint32_t timeout_ms, kapi_net_dns_t* result)
{
    if (!name || !result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    kapi_copy_string(result->name, name, sizeof(result->name));
    result->timeout_ms = timeout_ms;
    result->status = KAPI_NET_STATUS_DNS_NO_ANSWER;
    return KAPI_ENOSYS;
}
