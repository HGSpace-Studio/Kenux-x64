/*
 * Kenux Advanced OS Skeleton - VPN / Tunnel implementation
 *
 * Skeleton: tunnel table + state machine. Crypto / handshake backends
 * for WireGuard, OpenVPN, and IPsec are TODO.
 */

#include "kapi_vpn.h"
#include "kapi.h"

#include <string.h>

static kapi_vpn_tunnel_t kapi_vpn_table[KAPI_VPN_MAX_TUNNELS];
static int kapi_vpn_initialized = 0;

static kapi_vpn_tunnel_t *vpn_slot_alloc(void)
{
    for (int i = 0; i < KAPI_VPN_MAX_TUNNELS; i++) {
        if (!kapi_vpn_table[i].registered) {
            return &kapi_vpn_table[i];
        }
    }
    return NULL;
}

int kapi_vpn_init(void)
{
    if (kapi_vpn_initialized) {
        return KAPI_VPN_OK;
    }
    memset(kapi_vpn_table, 0, sizeof(kapi_vpn_table));
    kapi_vpn_initialized = 1;
    return KAPI_VPN_OK;
}

void kapi_vpn_exit(void)
{
    memset(kapi_vpn_table, 0, sizeof(kapi_vpn_table));
    kapi_vpn_initialized = 0;
}

int kapi_vpn_create(const char *name, kapi_vpn_type_t type)
{
    if (!name) {
        return KAPI_VPN_EINVAL;
    }
    if (kapi_vpn_find(name)) {
        return KAPI_VPN_EEXIST;
    }
    kapi_vpn_tunnel_t *t = vpn_slot_alloc();
    if (!t) {
        return KAPI_VPN_ENOMEM;
    }
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, KAPI_VPN_NAME_MAX - 1);
    t->type = type;
    t->state = KAPI_VPN_STATE_DOWN;
    t->mtu = KAPI_VPN_MTU_DEFAULT;
    t->registered = 1;
    /* TODO: allocate virtual netdev */
    return KAPI_VPN_OK;
}

int kapi_vpn_destroy(const char *name)
{
    kapi_vpn_tunnel_t *t = kapi_vpn_find(name);
    if (!t) {
        return KAPI_VPN_ENOENT;
    }
    if (t->state == KAPI_VPN_STATE_UP) {
        return KAPI_VPN_EBUSY;
    }
    memset(t, 0, sizeof(*t));
    return KAPI_VPN_OK;
}

int kapi_vpn_up(const char *name)
{
    kapi_vpn_tunnel_t *t = kapi_vpn_find(name);
    if (!t) {
        return KAPI_VPN_ENOENT;
    }
    t->state = KAPI_VPN_STATE_CONNECTING;
    /* TODO: initiate handshake for tunnel type */
    t->state = KAPI_VPN_STATE_UP;
    return KAPI_VPN_OK;
}

int kapi_vpn_down(const char *name)
{
    kapi_vpn_tunnel_t *t = kapi_vpn_find(name);
    if (!t) {
        return KAPI_VPN_ENOENT;
    }
    t->state = KAPI_VPN_STATE_DOWN;
    return KAPI_VPN_OK;
}

kapi_vpn_tunnel_t *kapi_vpn_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_VPN_MAX_TUNNELS; i++) {
        if (kapi_vpn_table[i].registered &&
            strncmp(kapi_vpn_table[i].name, name,
                    KAPI_VPN_NAME_MAX) == 0) {
            return &kapi_vpn_table[i];
        }
    }
    return NULL;
}

int kapi_vpn_set_peer(const char *name, const kapi_vpn_peer_t *peer)
{
    kapi_vpn_tunnel_t *t = kapi_vpn_find(name);
    if (!t || !peer) {
        return KAPI_VPN_EINVAL;
    }
    t->peer = *peer;
    return KAPI_VPN_OK;
}

int kapi_vpn_generate_keypair(uint8_t *public_out, uint8_t *private_out)
{
    if (!public_out || !private_out) {
        return KAPI_VPN_EINVAL;
    }
    /* TODO: Curve25519 keypair generation (WireGuard) */
    memset(public_out, 0, KAPI_VPN_PSK_LEN);
    memset(private_out, 0, KAPI_VPN_PSK_LEN);
    return KAPI_VPN_OK;
}

int kapi_vpn_encapsulate(kapi_vpn_tunnel_t *t, const void *inner,
                         size_t len, void *outer, size_t *out_len)
{
    if (!t || !inner || !outer || !out_len) {
        return KAPI_VPN_EINVAL;
    }
    if (t->state != KAPI_VPN_STATE_UP) {
        return KAPI_VPN_ENOTSUP;
    }
    /* TODO: encrypt + add tunnel headers per type */
    if (*out_len < len) {
        return KAPI_VPN_ENOMEM;
    }
    memcpy(outer, inner, len);
    *out_len = len;
    return KAPI_VPN_OK;
}

int kapi_vpn_decapsulate(kapi_vpn_tunnel_t *t, const void *outer,
                         size_t len, void *inner, size_t *out_len)
{
    if (!t || !outer || !inner || !out_len) {
        return KAPI_VPN_EINVAL;
    }
    if (t->state != KAPI_VPN_STATE_UP) {
        return KAPI_VPN_ENOTSUP;
    }
    /* TODO: decrypt + strip tunnel headers per type */
    if (*out_len < len) {
        return KAPI_VPN_ENOMEM;
    }
    memcpy(inner, outer, len);
    *out_len = len;
    return KAPI_VPN_OK;
}
