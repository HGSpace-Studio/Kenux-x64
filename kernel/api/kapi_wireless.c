/*
 * Kenux Advanced OS Skeleton - Wireless (Wi-Fi) implementation
 *
 * Skeleton: device table + scan/connect state machine. Hardware
 * abstraction (scan, auth, channel) is TODO pending driver backends.
 */

#include "kapi_wireless.h"
#include "kapi.h"

#include <string.h>

static kapi_wifi_dev_t kapi_wifi_dev_table[KAPI_WIFI_MAX_IFACES];
static int kapi_wifi_initialized = 0;

int kapi_wifi_init(void)
{
    if (kapi_wifi_initialized) {
        return KAPI_WIFI_OK;
    }
    memset(kapi_wifi_dev_table, 0, sizeof(kapi_wifi_dev_table));
    kapi_wifi_initialized = 1;
    return KAPI_WIFI_OK;
}

void kapi_wifi_exit(void)
{
    memset(kapi_wifi_dev_table, 0, sizeof(kapi_wifi_dev_table));
    kapi_wifi_initialized = 0;
}

int kapi_wifi_register_dev(kapi_wifi_dev_t *dev, kapi_netdev_t netdev)
{
    if (!dev) {
        return KAPI_WIFI_EINVAL;
    }
    for (int i = 0; i < KAPI_WIFI_MAX_IFACES; i++) {
        if (!kapi_wifi_dev_table[i].name[0]) {
            dev->netdev = netdev;
            kapi_wifi_dev_table[i] = *dev;
            return KAPI_WIFI_OK;
        }
    }
    return KAPI_WIFI_ENODEV;
}

int kapi_wifi_unregister_dev(kapi_wifi_dev_t *dev)
{
    if (!dev) {
        return KAPI_WIFI_EINVAL;
    }
    for (int i = 0; i < KAPI_WIFI_MAX_IFACES; i++) {
        if (strncmp(kapi_wifi_dev_table[i].name, dev->name,
                    KAPI_NETDEV_NAME_LEN) == 0) {
            memset(&kapi_wifi_dev_table[i], 0,
                   sizeof(kapi_wifi_dev_table[i]));
            return KAPI_WIFI_OK;
        }
    }
    return KAPI_WIFI_ENOENT;
}

kapi_wifi_dev_t *kapi_wifi_find_dev(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_WIFI_MAX_IFACES; i++) {
        if (strncmp(kapi_wifi_dev_table[i].name, name,
                    KAPI_NETDEV_NAME_LEN) == 0) {
            return &kapi_wifi_dev_table[i];
        }
    }
    return NULL;
}

int kapi_wifi_scan(kapi_wifi_dev_t *dev, int passive)
{
    if (!dev) {
        return KAPI_WIFI_EINVAL;
    }
    /* TODO: issue hardware scan; passive flag selects probe vs listen */
    dev->scanning = 1;
    (void)passive;
    return KAPI_WIFI_OK;
}

int kapi_wifi_get_scan_results(kapi_wifi_dev_t *dev,
                               kapi_wifi_net_t *out, uint32_t max,
                               uint32_t *count)
{
    if (!dev || !out || !count) {
        return KAPI_WIFI_EINVAL;
    }
    *count = 0;
    dev->scanning = 0;
    /* TODO: return cached BSS list from driver */
    return KAPI_WIFI_OK;
}

int kapi_wifi_connect(kapi_wifi_dev_t *dev, const kapi_wifi_net_t *net,
                      const char *passphrase)
{
    if (!dev || !net) {
        return KAPI_WIFI_EINVAL;
    }
    /* TODO: 4-way handshake (WPA2) / SAE (WPA3) via driver */
    (void)passphrase;
    dev->connected = 1;
    dev->connected_net = *net;
    return KAPI_WIFI_OK;
}

int kapi_wifi_disconnect(kapi_wifi_dev_t *dev)
{
    if (!dev) {
        return KAPI_WIFI_EINVAL;
    }
    dev->connected = 0;
    memset(&dev->connected_net, 0, sizeof(dev->connected_net));
    return KAPI_WIFI_OK;
}

int kapi_wifi_roam(kapi_wifi_dev_t *dev, const uint8_t *target_bssid)
{
    if (!dev || !target_bssid) {
        return KAPI_WIFI_EINVAL;
    }
    /* TODO: PMK cache + fast transition */
    memcpy(dev->connected_net.bssid, target_bssid, KAPI_WIFI_BSSID_LEN);
    return KAPI_WIFI_OK;
}

int kapi_wifi_set_channel(kapi_wifi_dev_t *dev, uint32_t channel,
                          kapi_wifi_band_t band)
{
    if (!dev) {
        return KAPI_WIFI_EINVAL;
    }
    dev->connected_net.channel = channel;
    dev->connected_net.band = band;
    /* TODO: program hardware PHY channel */
    return KAPI_WIFI_OK;
}

int kapi_wifi_get_phy(kapi_wifi_dev_t *dev, kapi_wifi_phy_t *out)
{
    if (!dev || !out) {
        return KAPI_WIFI_EINVAL;
    }
    *out = dev->connected_net.phy;
    return KAPI_WIFI_OK;
}
