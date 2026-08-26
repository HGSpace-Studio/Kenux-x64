

#ifndef ARCH_NET_H
#define ARCH_NET_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define ETH_ALEN        6
#define ETH_TYPE_IP     0x0800
#define ETH_TYPE_ARP    0x0806

typedef struct {
    uint8_t  dst[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_header_t;

#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO       8

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

#define TCP_FLAG_FIN    0x01
#define TCP_FLAG_SYN    0x02
#define TCP_FLAG_RST    0x04
#define TCP_FLAG_PSH    0x08
#define TCP_FLAG_ACK    0x10
#define TCP_FLAG_URG    0x20

#define TCP_CLOSED      0
#define TCP_SYN_SENT    1
#define TCP_SYN_RECV    2
#define TCP_ESTABLISHED 3
#define TCP_FIN_WAIT1   4
#define TCP_FIN_WAIT2   5
#define TCP_CLOSE_WAIT  6
#define TCP_CLOSING     7
#define TCP_LAST_ACK    8
#define TCP_TIME_WAIT   9
#define TCP_LISTEN      10

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define AF_UNIX         1
#define AF_INET         2

#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

typedef struct {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
} sockaddr_in_t;

#define TCP_MAX_SOCKETS     64
#define TCP_RX_BUF_SIZE     32768
#define TCP_TX_BUF_SIZE     65536
#define TCP_MSS             1460

typedef struct tcp_socket {
    int       state;
    uint32_t  local_ip;
    uint16_t  local_port;
    uint32_t  remote_ip;
    uint16_t  remote_port;

    uint32_t  snd_una;
    uint32_t  snd_nxt;
    uint32_t  rcv_nxt;
    uint16_t  snd_wnd;
    uint16_t  rcv_wnd;

    uint8_t   rx_buf[TCP_RX_BUF_SIZE];
    uint32_t  rx_head;
    uint32_t  rx_tail;

    uint8_t   tx_buf[TCP_TX_BUF_SIZE];
    uint32_t  tx_head;
    uint32_t  tx_tail;

    uint32_t  seq_base;
    uint32_t  ack_base;

    spinlock_t lock;
} tcp_socket_t;

#define UDP_MAX_SOCKETS     32
#define UDP_RX_BUF_SIZE     8192

typedef struct udp_socket {
    uint32_t  local_ip;
    uint16_t  local_port;
    uint32_t  remote_ip;
    uint16_t  remote_port;
    int       bound;
    spinlock_t lock;

    uint8_t   rx_buf[UDP_RX_BUF_SIZE];
    uint32_t  rx_head;
    uint32_t  rx_tail;

    uint32_t  peer_ip;
    uint16_t  peer_port;
    int       has_data;
    uint64_t  waiter_pid;
} udp_socket_t;

typedef struct socket {
    int domain;
    int type;
    int protocol;
    union {
        tcp_socket_t* tcp;
        udp_socket_t* udp;
    };
} socket_t;

typedef struct {
    uint8_t  mac[ETH_ALEN];
    uint32_t local_ip;
    uint32_t gateway_ip;
    uint32_t netmask;
    uint32_t dhcp_server_ip;
    uint32_t configured;
    uint32_t tx_registered;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} net_status_t;

void net_init(void);
void network_init(void);
void net_config_load(const char* config_data);
void net_get_status(net_status_t* status);

void net_rx_frame(const void* data, uint64_t len);
int  net_tx_frame(const void* data, uint64_t len);

void arp_send_request(uint32_t target_ip);
void arp_handle_packet(const arp_packet_t* arp, uint64_t len);
const uint8_t* arp_lookup(uint32_t ip);

void ip_send_packet(uint32_t dst_ip, uint8_t proto, const void* data, uint16_t len);
void ip_handle_packet(const ip_header_t* ip, uint64_t len);
uint16_t ip_checksum(const void* data, uint64_t len);

void icmp_send_echo_reply(uint32_t dst_ip, uint16_t id, uint16_t seq, const void* data, uint16_t len);
void icmp_handle_packet(const ip_header_t* ip, const icmp_header_t* icmp, uint64_t len);

void udp_send_packet(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                     const void* data, uint16_t len);
void udp_handle_packet(const ip_header_t* ip, const udp_header_t* udp, uint64_t len);

void tcp_send_packet(tcp_socket_t* sock, uint8_t flags, const void* data, uint16_t len);
void tcp_handle_packet(const ip_header_t* ip, const tcp_header_t* tcp, uint64_t len);

int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const sockaddr_in_t* addr, uint32_t addrlen);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, sockaddr_in_t* addr, uint32_t* addrlen);
int sys_connect(int sockfd, const sockaddr_in_t* addr, uint32_t addrlen);
int sys_send(int sockfd, const void* buf, uint64_t len, int flags);
int sys_recv(int sockfd, void* buf, uint64_t len, int flags);
int sys_sendto(int sockfd, const void* buf, uint64_t len, int flags,
               const sockaddr_in_t* dst_addr, uint32_t addrlen);
int sys_recvfrom(int sockfd, void* buf, uint64_t len, int flags,
                 sockaddr_in_t* src_addr, uint32_t* addrlen);
int sys_shutdown(int sockfd, int how);
int sys_close_socket(int sockfd);

#endif
