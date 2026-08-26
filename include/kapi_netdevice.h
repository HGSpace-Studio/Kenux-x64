#ifndef KAPI_NETDEVICE_H
#define KAPI_NETDEVICE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_netdev kapi_netdev_t;

#define KAPI_NETDEV_NAME_LEN 16
#define KAPI_ETH_ALEN 6
#define KAPI_CONNECTIVITY_MAX_LINKS 4

#define KAPI_NETDEV_UP         (1 << 0)
#define KAPI_NETDEV_BROADCAST  (1 << 1)
#define KAPI_NETDEV_MULTICAST  (1 << 2)
#define KAPI_NETDEV_LOOPBACK   (1 << 3)
#define KAPI_NETDEV_POINTTOPOINT (1 << 4)
#define KAPI_NETDEV_PROMISC    (1 << 5)
#define KAPI_NETDEV_ALLMULTI   (1 << 6)
#define KAPI_NETDEV_NOARP      (1 << 7)

typedef struct {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t multicast;
    uint64_t collisions;
} kapi_netdev_stats_t;

typedef enum {
    KAPI_CONN_MEDIA_ETHERNET = 1,
    KAPI_CONN_MEDIA_WIFI = 2,
    KAPI_CONN_MEDIA_BLUETOOTH = 3,
    KAPI_CONN_MEDIA_NTP = 4
} kapi_connectivity_media_t;

typedef enum {
    KAPI_CONN_STATE_UNAVAILABLE = 0,
    KAPI_CONN_STATE_DISCONNECTED = 1,
    KAPI_CONN_STATE_CONNECTING = 2,
    KAPI_CONN_STATE_READY = 3,
    KAPI_CONN_STATE_CONNECTED = 4
} kapi_connectivity_state_t;

typedef struct {
    char name[KAPI_NETDEV_NAME_LEN];
    char label[32];
    uint32_t media;
    uint32_t state;
    uint32_t ip_addr;
    uint32_t gateway_ip;
    uint32_t netmask;
    uint32_t signal_percent;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint8_t mac[KAPI_ETH_ALEN];
    char detail[64];
} kapi_connectivity_link_t;

typedef struct {
    uint32_t link_count;
    uint32_t ethernet_connected;
    uint32_t internet_available;
    uint32_t wifi_ready;
    uint32_t bluetooth_ready;
    uint32_t bluetooth_mouse_connected;
    uint32_t ntp_ready;
    uint32_t ntp_synced;
    uint64_t last_ntp_sync_ms;
    kapi_connectivity_link_t links[KAPI_CONNECTIVITY_MAX_LINKS];
} kapi_connectivity_status_t;

kapi_netdev_t* kapi_netdev_open(const char* name);

int kapi_netdev_close(kapi_netdev_t* dev);

int kapi_netdev_up(kapi_netdev_t* dev);

int kapi_netdev_down(kapi_netdev_t* dev);

int kapi_netdev_get_mac(kapi_netdev_t* dev, uint8_t mac[KAPI_ETH_ALEN]);

int kapi_netdev_set_mac(kapi_netdev_t* dev, const uint8_t mac[KAPI_ETH_ALEN]);

int kapi_netdev_get_mtu(kapi_netdev_t* dev);

int kapi_netdev_set_mtu(kapi_netdev_t* dev, int mtu);

uint32_t kapi_netdev_get_flags(kapi_netdev_t* dev);

int kapi_netdev_set_flags(kapi_netdev_t* dev, uint32_t flags);

int kapi_netdev_set_promiscuous(kapi_netdev_t* dev, int enable);

int kapi_netdev_is_up(kapi_netdev_t* dev);

int kapi_netdev_get_stats(kapi_netdev_t* dev, kapi_netdev_stats_t* stats);

int kapi_netdev_send(kapi_netdev_t* dev, const void* data, size_t len);

int kapi_netdev_count(void);

int kapi_netdev_get_name(kapi_netdev_t* dev, char* name, size_t len);

int kapi_connectivity_get_status(kapi_connectivity_status_t* status);

int kapi_connectivity_mark_ntp_synced(uint64_t unix_ms);

int kapi_connectivity_set_bluetooth_mouse(uint32_t connected);

/* ===== Fused LeonOS net.h config/dhcp/ping/dns layer ===== */
#define KAPI_NET_STATUS_OK                   0U
#define KAPI_NET_STATUS_NO_DEVICE            1U
#define KAPI_NET_STATUS_ARP_TIMEOUT          2U
#define KAPI_NET_STATUS_ECHO_TIMEOUT         3U
#define KAPI_NET_STATUS_BAD_ARGUMENT         4U
#define KAPI_NET_STATUS_TX_FAILED            5U
#define KAPI_NET_STATUS_DHCP_TIMEOUT         6U
#define KAPI_NET_STATUS_DHCP_FAILED          7U
#define KAPI_NET_STATUS_DNS_TIMEOUT          8U
#define KAPI_NET_STATUS_DNS_FAILED           9U
#define KAPI_NET_STATUS_DNS_NO_ANSWER       10U
#define KAPI_NET_STATUS_TCP_TIMEOUT         11U
#define KAPI_NET_STATUS_TCP_RESET           12U
#define KAPI_NET_STATUS_TCP_FAILED          13U
#define KAPI_NET_STATUS_HTTP_FAILED         14U
#define KAPI_NET_STATUS_SOCKET_LIMIT        16U
#define KAPI_NET_STATUS_SOCKET_BAD_HANDLE   17U
#define KAPI_NET_STATUS_SOCKET_NOT_CONNECTED 18U
#define KAPI_NET_STATUS_SOCKET_CLOSED       19U
#define KAPI_NET_STATUS_PROTOCOL_UNSUPPORTED 20U
#define KAPI_NET_STATUS_TLS_FAILED          21U

#define KAPI_NET_DEFAULT_TIMEOUT_MS 1000U
#define KAPI_NET_MAX_TIMEOUT_MS    10000U
#define KAPI_NET_DNS_MAX_ADDRESSES 4U

#define KAPI_NET_CONFIG_SOURCE_NONE   0U
#define KAPI_NET_CONFIG_SOURCE_STATIC 1U
#define KAPI_NET_CONFIG_SOURCE_DHCP   2U

#define KAPI_NET_CONFIG_FLAG_PRESENT 0x00000001U
#define KAPI_NET_CONFIG_FLAG_ACTIVE  0x00000002U
#define KAPI_NET_CONFIG_FLAG_DHCP   0x00000004U

typedef struct {
    uint32_t flags;
    uint32_t source;
    uint32_t local_ip;
    uint32_t subnet_mask;
    uint32_t gateway_ip;
    uint32_t dns_ip;
    uint32_t dhcp_server_ip;
    uint32_t lease_seconds;
    uint8_t  mac[6];
    uint8_t  reserved_mac[2];
} kapi_net_config_t;

typedef struct {
    uint32_t          timeout_ms;
    uint32_t          status;
    kapi_net_config_t config;
} kapi_net_dhcp_t;

typedef struct {
    uint32_t target_ip;
    uint32_t timeout_ms;
    uint32_t sequence;
    uint32_t status;
    uint32_t rtt_ms;
    uint32_t sent;
    uint32_t received;
    uint32_t reserved;
} kapi_net_ping_t;

typedef struct {
    char     name[128];
    uint32_t timeout_ms;
    uint32_t status;
    uint32_t address_count;
    uint32_t addresses[KAPI_NET_DNS_MAX_ADDRESSES];
} kapi_net_dns_t;

int kapi_net_config(kapi_net_config_t* config);
int kapi_net_dhcp_renew(uint32_t timeout_ms, kapi_net_dhcp_t* result);
int kapi_net_ping(uint32_t target_ip, uint32_t timeout_ms,
                  kapi_net_ping_t* result);
int kapi_net_dns_resolve(const char* name, uint32_t timeout_ms,
                         kapi_net_dns_t* result);

/* LeonOS compat aliases */
#define KAPI_Net_Config(p)           kapi_net_config((p))
#define KAPI_Net_DHCPRenew(t,r)      kapi_net_dhcp_renew((t),(r))
#define KAPI_Net_Ping(ip,t,r)        kapi_net_ping((ip),(t),(r))
#define KAPI_Net_DNSResolve(n,t,r)   kapi_net_dns_resolve((n),(t),(r))
typedef kapi_net_config_t  KAPI_NET_CONFIG;
typedef kapi_net_dhcp_t    KAPI_NET_DHCP;
typedef kapi_net_ping_t    KAPI_NET_PING;
typedef kapi_net_dns_t     KAPI_NET_DNS;

#ifdef __cplusplus
}
#endif

#endif
