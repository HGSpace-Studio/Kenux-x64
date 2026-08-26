#ifndef KAPI_WIRELESS_H
#define KAPI_WIRELESS_H

/*
 * Kenux Advanced OS Skeleton - Wireless (Wi-Fi) Subsystem
 *
 * Hardware abstraction for 802.11 devices, WPA2/WPA3 auth, scan/roam
 * management, and channel/PHY control. Sits above kapi_netdev.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_netdevice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_WIFI_SSID_MAX        32
#define KAPI_WIFI_BSSID_LEN       6
#define KAPI_WIFI_MAX_SCAN        256
#define KAPI_WIFI_MAX_IFACES      16

typedef struct kapi_wifi_dev    kapi_wifi_dev_t;
typedef struct kapi_wifi_net    kapi_wifi_net_t;

typedef enum {
    KAPI_WIFI_OK             = 0,
    KAPI_WIFI_EINVAL         = -1,
    KAPI_WIFI_ENOMEM         = -2,
    KAPI_WIFI_ENODEV        = -3,
    KAPI_WIFI_EBUSY         = -4,
    KAPI_WIFI_ENOTSUP       = -5,
    KAPI_WIFI_EAUTH         = -6,
    KAPI_WIFI_ENOTCONN      = -7
} kapi_wifi_err_t;

typedef enum {
    KAPI_WIFI_AUTH_OPEN   = 0,
    KAPI_WIFI_AUTH_WEP    = 1,
    KAPI_WIFI_AUTH_WPA2   = 2,
    KAPI_WIFI_AUTH_WPA3   = 3,
    KAPI_WIFI_AUTH_8021X  = 4
} kapi_wifi_auth_t;

typedef enum {
    KAPI_WIFI_BAND_2GHZ  = 0,
    KAPI_WIFI_BAND_5GHZ  = 1,
    KAPI_WIFI_BAND_6GHZ  = 2
} kapi_wifi_band_t;

typedef enum {
    KAPI_WIFI_PHY_LEGACY  = 0,
    KAPI_WIFI_PHY_HT      = 1,   /* 802.11n */
    KAPI_WIFI_PHY_VHT     = 2,   /* 802.11ac */
    KAPI_WIFI_PHY_HE      = 3    /* 802.11ax */
} kapi_wifi_phy_t;

struct kapi_wifi_net {
    char     ssid[KAPI_WIFI_SSID_MAX];
    uint8_t  bssid[KAPI_WIFI_BSSID_LEN];
    int32_t  rssi;
    uint32_t channel;
    kapi_wifi_band_t band;
    kapi_wifi_phy_t  phy;
    kapi_wifi_auth_t auth;
    uint32_t flags;
};

struct kapi_wifi_dev {
    kapi_netdev_t netdev;
    char          name[KAPI_NETDEV_NAME_LEN];
    int           supported_bands;
    kapi_wifi_auth_t max_auth;
    int           scanning;
    int           connected;
    kapi_wifi_net_t connected_net;
};

/* Subsystem lifecycle */
int kapi_wifi_init(void);
void kapi_wifi_exit(void);

/* Device (netdev) registration */
int kapi_wifi_register_dev(kapi_wifi_dev_t *dev, kapi_netdev_t netdev);
int kapi_wifi_unregister_dev(kapi_wifi_dev_t *dev);
kapi_wifi_dev_t *kapi_wifi_find_dev(const char *name);

/* Scanning */
int kapi_wifi_scan(kapi_wifi_dev_t *dev, int passive);
int kapi_wifi_get_scan_results(kapi_wifi_dev_t *dev,
                               kapi_wifi_net_t *out, uint32_t max,
                               uint32_t *count);

/* Connection management */
int kapi_wifi_connect(kapi_wifi_dev_t *dev, const kapi_wifi_net_t *net,
                      const char *passphrase);
int kapi_wifi_disconnect(kapi_wifi_dev_t *dev);
int kapi_wifi_roam(kapi_wifi_dev_t *dev, const uint8_t *target_bssid);

/* Channel / PHY control */
int kapi_wifi_set_channel(kapi_wifi_dev_t *dev, uint32_t channel,
                          kapi_wifi_band_t band);
int kapi_wifi_get_phy(kapi_wifi_dev_t *dev, kapi_wifi_phy_t *out);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_WIRELESS_H */
