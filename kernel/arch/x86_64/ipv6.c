#include <arch/ipv6.h>
#include <arch/net.h>
#include <arch/memory.h>
#include <string.h>

static ipv6_addr_t local_ipv6;
static ipv6_addr_t gateway_ipv6;
static int ipv6_prefix_len = 64;
static int ipv6_configured = 0;

static nd_cache_entry_t nd_cache[ND_CACHE_SIZE];
static spinlock_t ipv6_lock = SPINLOCK_INIT;

static uint16_t ipv6_checksum(const ipv6_addr_t* src, const ipv6_addr_t* dst,
                               uint8_t next_hdr, const void* data, uint16_t len)
{
    uint32_t sum = 0;
    const uint16_t* p;

    p = (const uint16_t*)src;
    for (int i = 0; i < 8; i++) sum += p[i];

    p = (const uint16_t*)dst;
    for (int i = 0; i < 8; i++) sum += p[i];

    sum += (uint16_t)next_hdr;
    sum += len;

    p = (const uint16_t*)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) sum += *(const uint8_t*)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static int ipv6_addr_equal(const ipv6_addr_t* a, const ipv6_addr_t* b)
{
    return memcmp(a->addr, b->addr, IPV6_ADDR_LEN) == 0;
}

static int ipv6_addr_is_zero(const ipv6_addr_t* a)
{
    for (int i = 0; i < IPV6_ADDR_LEN; i++) {
        if (a->addr[i] != 0) return 0;
    }
    return 1;
}

void ipv6_init(void)
{
    memset(&local_ipv6, 0, sizeof(local_ipv6));
    memset(&gateway_ipv6, 0, sizeof(gateway_ipv6));
    memset(nd_cache, 0, sizeof(nd_cache));
    ipv6_configured = 0;
    ipv6_prefix_len = 64;
}

int ipv6_config_addr(const ipv6_addr_t* addr, int prefix_len)
{
    if (!addr || prefix_len < 0 || prefix_len > 128) return -1;
    memcpy(&local_ipv6, addr, sizeof(ipv6_addr_t));
    ipv6_prefix_len = prefix_len;
    ipv6_configured = 1;
    return 0;
}

int ipv6_add_route(const ipv6_addr_t* dst, int prefix_len, const ipv6_addr_t* gateway)
{
    if (!dst || prefix_len < 0 || prefix_len > 128) return -1;
    if (gateway) memcpy(&gateway_ipv6, gateway, sizeof(ipv6_addr_t));
    return 0;
}

void ipv6_generate_link_local(const uint8_t* mac, ipv6_addr_t* out)
{
    if (!out) return;
    memset(out, 0, sizeof(ipv6_addr_t));
    out->addr[0] = 0xFE;
    out->addr[1] = 0x80;
    out->addr[8]  = mac[0] ^ 0x02;
    out->addr[9]  = mac[1];
    out->addr[10] = mac[2];
    out->addr[11] = 0xFF;
    out->addr[12] = 0xFE;
    out->addr[13] = mac[3];
    out->addr[14] = mac[4];
    out->addr[15] = mac[5];
}

void ipv6_generate_slaac(const uint8_t* mac, const uint8_t* prefix, ipv6_addr_t* out)
{
    if (!out) return;
    if (prefix) {
        memcpy(out->addr, prefix, 8);
    } else {
        out->addr[0] = 0xFD;
        for (int i = 1; i < 8; i++) out->addr[i] = 0;
    }
    out->addr[8]  = mac[0] ^ 0x02;
    out->addr[9]  = mac[1];
    out->addr[10] = mac[2];
    out->addr[11] = 0xFF;
    out->addr[12] = 0xFE;
    out->addr[13] = mac[3];
    out->addr[14] = mac[4];
    out->addr[15] = mac[5];
}

int ipv6_send(const ipv6_addr_t* dst, uint8_t next_hdr, const void* payload, uint16_t len)
{
    if (!dst || !payload) return -1;

    uint8_t frame[sizeof(eth_header_t) + IPV6_HDR_LEN + len];
    memset(frame, 0, sizeof(frame));

    eth_header_t* eth = (eth_header_t*)frame;
    ipv6_header_t* ip6 = (ipv6_header_t*)(frame + sizeof(eth_header_t));

    uint8_t dst_mac[6];
    if (IPV6_ADDR_IS_MULTICAST(dst)) {
        dst_mac[0] = 0x33;
        dst_mac[1] = 0x33;
        dst_mac[2] = dst->addr[12];
        dst_mac[3] = dst->addr[13];
        dst_mac[4] = dst->addr[14];
        dst_mac[5] = dst->addr[15];
    } else {
        if (ipv6_nd_resolve(dst, dst_mac) != 0) {
            dst_mac[0] = 0x33; dst_mac[1] = 0x33;
            dst_mac[2] = dst->addr[12]; dst_mac[3] = dst->addr[13];
            dst_mac[4] = dst->addr[14]; dst_mac[5] = dst->addr[15];
        }
    }

    memcpy(eth->dst, dst_mac, 6);
    eth->type = 0xDD86;

    ip6->version_tc_flow = (6 << 28);
    ip6->payload_len = len;
    ip6->next_header = next_hdr;
    ip6->hop_limit = IPV6_HOP_LIMIT;
    memcpy(&ip6->src, &local_ipv6, sizeof(ipv6_addr_t));
    memcpy(&ip6->dst, dst, sizeof(ipv6_addr_t));

    memcpy(frame + sizeof(eth_header_t) + IPV6_HDR_LEN, payload, len);

    return sizeof(frame);
}

int ipv6_receive(const void* frame, uint16_t len)
{
    if (!frame || len < sizeof(eth_header_t) + IPV6_HDR_LEN) return -1;

    const ipv6_header_t* ip6 = (const ipv6_header_t*)
        ((const uint8_t*)frame + sizeof(eth_header_t));

    int version = (ip6->version_tc_flow >> 28) & 0xF;
    if (version != 6) return -2;

    const void* payload = (const uint8_t*)ip6 + IPV6_HDR_LEN;
    uint16_t plen = ip6->payload_len;

    switch (ip6->next_header) {
    case IPV6_PROTO_ICMP6:
        ipv6_nd_process(&ip6->src, payload, plen);
        break;
    case IPV6_PROTO_TCP:
        break;
    case IPV6_PROTO_UDP:
        break;
    }

    return 0;
}

int ipv6_icmp6_send(const ipv6_addr_t* dst, uint8_t type, uint8_t code,
                    const void* data, uint16_t len)
{
    uint16_t total = sizeof(icmp6_header_t) + len;
    uint8_t buf[total];
    icmp6_header_t* hdr = (icmp6_header_t*)buf;
    hdr->type = type;
    hdr->code = code;
    hdr->checksum = 0;
    hdr->id = 0;
    hdr->seq = 0;
    if (data && len > 0) memcpy(buf + sizeof(icmp6_header_t), data, len);

    hdr->checksum = ipv6_checksum(&local_ipv6, dst, IPV6_PROTO_ICMP6, buf, total);
    if (hdr->checksum == 0) hdr->checksum = 0xFFFF;

    return ipv6_send(dst, IPV6_PROTO_ICMP6, buf, total);
}

int ipv6_nd_resolve(const ipv6_addr_t* target, uint8_t* out_mac)
{
    if (!target || !out_mac) return -1;

    spinlock_acquire(&ipv6_lock);
    for (int i = 0; i < ND_CACHE_SIZE; i++) {
        if (nd_cache[i].valid && ipv6_addr_equal(&nd_cache[i].ip, target)) {
            memcpy(out_mac, nd_cache[i].mac, 6);
            spinlock_release(&ipv6_lock);
            return 0;
        }
    }
    spinlock_release(&ipv6_lock);

    nd_neighbor_sol_t sol;
    memset(&sol, 0, sizeof(sol));
    sol.type = ICMP6_TYPE_NEIGH_SOL;
    sol.code = 0;
    memcpy(&sol.target, target, sizeof(ipv6_addr_t));

    ipv6_addr_t sol_dst;
    memset(&sol_dst, 0, sizeof(sol_dst));
    sol_dst.addr[0] = 0xFF;
    sol_dst.addr[1] = 0x02;
    sol_dst.addr[11] = 0x01;
    memcpy(&sol_dst.addr[12], &target->addr[12], 4);

    ipv6_icmp6_send(&sol_dst, ICMP6_TYPE_NEIGH_SOL, 0, &sol, sizeof(sol));
    return -2;
}

void ipv6_nd_process(const ipv6_addr_t* src, const void* nd_msg, uint16_t len)
{
    if (!nd_msg || len < 2) return;

    const uint8_t* p = (const uint8_t*)nd_msg;
    uint8_t type = p[0];

    if (type == ICMP6_TYPE_NEIGH_ADV) {
        const nd_neighbor_adv_t* adv = (const nd_neighbor_adv_t*)nd_msg;
        spinlock_acquire(&ipv6_lock);
        int slot = -1;
        for (int i = 0; i < ND_CACHE_SIZE; i++) {
            if (nd_cache[i].valid && ipv6_addr_equal(&nd_cache[i].ip, &adv->target)) {
                slot = i;
                break;
            }
            if (!nd_cache[i].valid && slot < 0) slot = i;
        }
        if (slot >= 0) {
            memcpy(&nd_cache[slot].ip, &adv->target, sizeof(ipv6_addr_t));
            memcpy(nd_cache[slot].mac, src->addr + 10, 6);
            nd_cache[slot].valid = 1;
        }
        spinlock_release(&ipv6_lock);
    }
}

const ipv6_addr_t* ipv6_get_local_addr(void)
{
    return &local_ipv6;
}

int ipv6_get_prefix_len(void)
{
    return ipv6_prefix_len;
}