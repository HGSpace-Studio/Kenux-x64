#ifndef KAPI_VPN_H
#define KAPI_VPN_H

/*
 * Kenux Advanced OS Skeleton - VPN / Tunnel Subsystem
 *
 * Supports WireGuard, OpenVPN-style TLS, and IPsec tunnels, exposing
 * virtual netdev interfaces. Skeleton: API + data structures.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_netdevice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_VPN_NAME_MAX        32
#define KAPI_VPN_MAX_TUNNELS     16
#define KAPI_VPN_PSK_LEN        32
#define KAPI_VPN_MTU_DEFAULT    1420

typedef struct kapi_vpn_tunnel kapi_vpn_tunnel_t;

typedef enum {
    KAPI_VPN_OK             = 0,
    KAPI_VPN_EINVAL         = -1,
    KAPI_VPN_ENOMEM         = -2,
    KAPI_VPN_ENOENT        = -3,
    KAPI_VPN_EEXIST        = -4,
    KAPI_VPN_EBUSY         = -5,
    KAPI_VPN_EAUTH         = -6,
    KAPI_VPN_ENOTSUP       = -7
} kapi_vpn_err_t;

typedef enum {
    KAPI_VPN_TYPE_WIREGUARD = 0,
    KAPI_VPN_TYPE_OPENVPN   = 1,
    KAPI_VPN_TYPE_IPSEC     = 2
} kapi_vpn_type_t;

typedef enum {
    KAPI_VPN_STATE_DOWN     = 0,
    KAPI_VPN_STATE_CONNECTING = 1,
    KAPI_VPN_STATE_UP       = 2,
    KAPI_VPN_STATE_ERROR    = 3
} kapi_vpn_state_t;

typedef struct {
    uint8_t  public_key[KAPI_VPN_PSK_LEN];
    uint8_t  preshared_key[KAPI_VPN_PSK_LEN];
    uint16_t keepalive_sec;
    uint32_t allowed_ips[8];   /* simplistic CIDR list */
} kapi_vpn_peer_t;

struct kapi_vpn_tunnel {
    char            name[KAPI_VPN_NAME_MAX];
    kapi_vpn_type_t type;
    kapi_vpn_state_t state;
    kapi_netdev_t   netdev;
    uint32_t        local_ip;
    uint32_t        remote_ip;
    uint32_t        mtu;
    kapi_vpn_peer_t peer;
    int             registered;
};

/* Subsystem lifecycle */
int kapi_vpn_init(void);
void kapi_vpn_exit(void);

/* Tunnel management */
int kapi_vpn_create(const char *name, kapi_vpn_type_t type);
int kapi_vpn_destroy(const char *name);
int kapi_vpn_up(const char *name);
int kapi_vpn_down(const char *name);
kapi_vpn_tunnel_t *kapi_vpn_find(const char *name);

/* Peer / keying */
int kapi_vpn_set_peer(const char *name, const kapi_vpn_peer_t *peer);
int kapi_vpn_generate_keypair(uint8_t *public_out, uint8_t *private_out);

/* Data path: encapsulate/decapsulate on the underlying netdev */
int kapi_vpn_encapsulate(kapi_vpn_tunnel_t *t, const void *inner,
                         size_t len, void *outer, size_t *out_len);
int kapi_vpn_decapsulate(kapi_vpn_tunnel_t *t, const void *outer,
                         size_t len, void *inner, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_VPN_H */
