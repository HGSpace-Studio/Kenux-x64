#ifndef ARCH_X86_64_IPV6_H
#define ARCH_X86_64_IPV6_H

#include <arch/types.h>

#define IPV6_ADDR_LEN       16
#define IPV6_HDR_LEN        40
#define ETH_TYPE_IPV6       0x86DD

#define IPV6_VERSION        6
#define IPV6_HOP_LIMIT      64

#define ICMP6_TYPE_ECHO_REQ     128
#define ICMP6_TYPE_ECHO_REPLY   129
#define ICMP6_TYPE_ROUTER_SOL   133
#define ICMP6_TYPE_ROUTER_ADV   134
#define ICMP6_TYPE_NEIGH_SOL   135
#define ICMP6_TYPE_NEIGH_ADV   136
#define ICMP6_TYPE_REDIRECT    137

#define IPV6_PROTO_HOPOPTS  0
#define IPV6_PROTO_ICMP6    58
#define IPV6_PROTO_TCP      6
#define IPV6_PROTO_UDP      17
#define IPV6_PROTO_ROUTING  43
#define IPV6_PROTO_FRAGMENT 44
#define IPV6_PROTO_ESP      50
#define IPV6_PROTO_AH       51

typedef struct {
    uint8_t addr[IPV6_ADDR_LEN];
} ipv6_addr_t;

typedef struct {
    uint32_t version_tc_flow;
    uint16_t payload_len;
    uint8_t  next_header;
    uint8_t  hop_limit;
    ipv6_addr_t src;
    ipv6_addr_t dst;
} __attribute__((packed)) ipv6_header_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp6_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint32_t reserved;
    ipv6_addr_t target;
    uint8_t  options[0];
} __attribute__((packed)) nd_neighbor_sol_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint32_t flags;
    ipv6_addr_t target;
    uint8_t  options[0];
} __attribute__((packed)) nd_neighbor_adv_t;

#define ND_CACHE_SIZE  32

typedef struct {
    ipv6_addr_t ip;
    uint8_t     mac[6];
    uint64_t    timestamp;
    int         valid;
} nd_cache_entry_t;

#define IPV6_ADDR_IS_LINKLOCAL(a) ((a)->addr[0] == 0xFE && ((a)->addr[1] & 0xC0) == 0x80)
#define IPV6_ADDR_IS_MULTICAST(a) ((a)->addr[0] == 0xFF)
#define IPV6_ADDR_IS_LOOPBACK(a) ((a)->addr[15] == 1 && \
    (a)->addr[0] == 0 && (a)->addr[1] == 0 && (a)->addr[2] == 0 && (a)->addr[3] == 0 && \
    (a)->addr[4] == 0 && (a)->addr[5] == 0 && (a)->addr[6] == 0 && (a)->addr[7] == 0 && \
    (a)->addr[8] == 0 && (a)->addr[9] == 0 && (a)->addr[10] == 0 && (a)->addr[11] == 0 && \
    (a)->addr[12] == 0 && (a)->addr[13] == 0 && (a)->addr[14] == 0)

void ipv6_init(void);
int ipv6_config_addr(const ipv6_addr_t* addr, int prefix_len);
int ipv6_add_route(const ipv6_addr_t* dst, int prefix_len, const ipv6_addr_t* gateway);
void ipv6_generate_link_local(const uint8_t* mac, ipv6_addr_t* out);
void ipv6_generate_slaac(const uint8_t* mac, const uint8_t* prefix, ipv6_addr_t* out);
int ipv6_send(const ipv6_addr_t* dst, uint8_t next_hdr, const void* payload, uint16_t len);
int ipv6_receive(const void* frame, uint16_t len);
int ipv6_icmp6_send(const ipv6_addr_t* dst, uint8_t type, uint8_t code,
                    const void* data, uint16_t len);
int ipv6_nd_resolve(const ipv6_addr_t* target, uint8_t* out_mac);
void ipv6_nd_process(const ipv6_addr_t* src, const void* nd_msg, uint16_t len);
const ipv6_addr_t* ipv6_get_local_addr(void);
int ipv6_get_prefix_len(void);

#endif