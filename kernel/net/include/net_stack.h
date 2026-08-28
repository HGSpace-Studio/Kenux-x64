#ifndef KERNEL_NET_STACK_H
#define KERNEL_NET_STACK_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define NET_PROTO_IPV4     0x0800
#define NET_PROTO_ARP      0x0806
#define NET_PROTO_IPV6     0x86DD
#define NET_PROTO_ICMP     1
#define NET_PROTO_TCP      6
#define NET_PROTO_UDP      17
#define NET_PROTO_ICMPV6   58

#define NET_IFACE_MAX      8
#define NET_SOCKET_MAX     4096
#define NET_PACKET_MAX     65536
#define NET_PORT_MIN       49152
#define NET_PORT_MAX       65535

#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RECV    3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT_1  5
#define TCP_STATE_FIN_WAIT_2  6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

#define TCP_FLAG_FIN    0x01
#define TCP_FLAG_SYN    0x02
#define TCP_FLAG_RST    0x04
#define TCP_FLAG_PSH    0x08
#define TCP_FLAG_ACK    0x10
#define TCP_FLAG_URG    0x20

#define ICMP_TYPE_ECHO_REPLY    0
#define ICMP_TYPE_ECHO_REQUEST  8
#define ICMP_TYPE_DEST_UNREACH  3
#define ICMP_TYPE_TIME_EXCEED   11
#define ICMP_TYPE_REDIRECT      5
#define ICMP_TYPE_PARAM_PROBLEM 12
#define ICMP_TYPE_TIMESTAMP     13
#define ICMP_TYPE_TIMESTAMP_REPLY 14

#define ICMPV6_TYPE_ECHO_REQUEST 128
#define ICMPV6_TYPE_ECHO_REPLY   129
#define ICMPV6_TYPE_DEST_UNREACH 1
#define ICMPV6_TYPE_PKT_TOO_BIG  2
#define ICMPV6_TYPE_TIME_EXCEED  3
#define ICMPV6_TYPE_PARAM_PROBLEM 4
#define ICMPV6_TYPE_ND_SOLICIT   135
#define ICMPV6_TYPE_ND_ADVERT    136

typedef struct {
    uint32_t addr;
} ipv4_addr_t;

typedef struct {
    uint8_t addr[16];
} ipv6_addr_t;

typedef struct {
    uint8_t  mac[6];
} mac_addr_t;

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint32_t flow_label;
    uint16_t payload_length;
    uint8_t  next_header;
    uint8_t  hop_limit;
    uint8_t  src_addr[16];
    uint8_t  dst_addr[16];
} ipv6_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    int       index;
    mac_addr_t mac;
    ipv4_addr_t ipv4;
    ipv4_addr_t ipv4_mask;
    ipv4_addr_t ipv4_gw;
    ipv6_addr_t ipv6;
    uint8_t    ipv6_prefix_len;
    int        has_ipv4;
    int        has_ipv6;
    int        up;
    int        mtu;
    void*      driver_ctx;
    int (*send)(void* ctx, const void* data, uint32_t len);
    spinlock_t lock;
} net_iface_t;

typedef struct {
    int       used;
    int       domain;
    int       type;
    int       protocol;
    int       state;
    net_iface_t* iface;
    ipv4_addr_t local_addr;
    ipv4_addr_t remote_addr;
    ipv6_addr_t local_addr6;
    ipv6_addr_t remote_addr6;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t window_size;
    uint32_t mss;
    uint32_t cwnd;
    uint32_t ssthresh;
    uint32_t rtt;
    uint32_t rtt_var;
    uint32_t retransmit_timeout;
    uint32_t retransmit_count;
    void*    recv_buffer;
    uint32_t recv_buf_size;
    uint32_t recv_buf_pos;
    uint32_t recv_data_len;
    void*    send_buffer;
    uint32_t send_buf_size;
    uint32_t send_buf_pos;
    uint32_t send_data_len;
    spinlock_t lock;
} net_socket_t;

void net_init(void);
net_iface_t* net_get_iface(int index);
int net_register_iface(int index, mac_addr_t* mac, void* driver_ctx,
                       int (*send)(void*, const void*, uint32_t));
int net_iface_set_ipv4(int index, uint32_t addr, uint32_t mask, uint32_t gw);
int net_iface_set_ipv6(int index, const uint8_t* addr, uint8_t prefix_len);
int net_iface_up(int index);
int net_iface_down(int index);

int net_send_packet(net_iface_t* iface, const void* data, uint32_t len);
int net_receive_packet(net_iface_t* iface, const void* data, uint32_t len);

int net_ipv4_send(net_iface_t* iface, uint32_t src, uint32_t dst, uint8_t proto,
                  const void* payload, uint32_t payload_len);
int net_ipv6_send(net_iface_t* iface, const uint8_t* src, const uint8_t* dst,
                  uint8_t proto, const void* payload, uint32_t payload_len);

int net_icmp_send_echo(net_iface_t* iface, uint32_t dst, uint16_t id, uint16_t seq,
                       const void* data, uint32_t data_len);
int net_icmpv6_send_echo(net_iface_t* iface, const uint8_t* dst, uint16_t id, uint16_t seq,
                          const void* data, uint32_t data_len);

int net_tcp_connect(net_socket_t* sock, uint32_t remote_addr, uint16_t remote_port);
int net_tcp_listen(net_socket_t* sock, uint16_t port);
net_socket_t* net_tcp_accept(net_socket_t* sock);
int net_tcp_send(net_socket_t* sock, const void* data, uint32_t len);
int net_tcp_recv(net_socket_t* sock, void* buf, uint32_t len);
int net_tcp_close(net_socket_t* sock);

int net_udp_send(net_socket_t* sock, uint32_t remote_addr, uint16_t remote_port,
                 const void* data, uint32_t len);
int net_udp_recv(net_socket_t* sock, void* buf, uint32_t len,
                 uint32_t* from_addr, uint16_t* from_port);
int net_udp_bind(net_socket_t* sock, uint16_t port);
int net_udp_close(net_socket_t* sock);

net_socket_t* net_socket_create(int domain, int type, int protocol);
void net_socket_destroy(net_socket_t* sock);

uint16_t net_checksum(const void* data, uint32_t len);
uint16_t net_checksum_combine(uint16_t c1, uint16_t c2);
uint32_t net_htonl(uint32_t val);
uint16_t net_htons(uint16_t val);

#endif