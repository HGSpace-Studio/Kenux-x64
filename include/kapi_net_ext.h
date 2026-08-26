#ifndef KAPI_NET_EXT_H
#define KAPI_NET_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_AF_UNSPEC     0
#define KAPI_AF_UNIX       1
#define KAPI_AF_INET       2
#define KAPI_AF_AX25       3
#define KAPI_AF_IPX        4
#define KAPI_AF_APPLETALK  5
#define KAPI_AF_NETROM     6
#define KAPI_AF_BRIDGE     7
#define KAPI_AF_ATMPVC     8
#define KAPI_AF_X25        9
#define KAPI_AF_INET6      10
#define KAPI_AF_ROSE       11
#define KAPI_AF_DECnet     12
#define KAPI_AF_NETBEUI    13
#define KAPI_AF_SECURITY   14
#define KAPI_AF_KEY        15
#define KAPI_AF_NETLINK    16
#define KAPI_AF_PACKET     17
#define KAPI_ASH          18
#define KAPI_AF_ECONET     19
#define KAPI_AF_ATMSVC     20
#define KAPI_AF_RDS        21
#define KAPI_AF_SNA        22
#define KAPI_AF_IRDA       23
#define KAPI_AF_PPPOX      24
#define KAPI_AF_WANPIPE    25
#define KAPI_AF_LLC        26
#define KAPI_AF_IB         27
#define KAPI_AF_MPLS       28
#define KAPI_AF_CAN        29
#define KAPI_AF_TIPC       30
#define KAPI_AF_BLUETOOTH  31
#define KAPI_AF_IUCV       32
#define KAPI_AF_RXRPC      33
#define KAPI_AF_ISDN       34
#define KAPI_AF_PHONET     35
#define KAPI_AF_IEEE802154 36
#define KAPI_AF_CAIF       37
#define KAPI_AF_ALG        38
#define KAPI_AF_NFC        39
#define KAPI_AF_VSOCK      40
#define KAPI_AF_KCM        41
#define KAPI_AF_QIPCRTR    42
#define KAPI_AF_SMC        43
#define KAPI_AF_XDP        44

#define KAPI_SOCK_STREAM     1
#define KAPI_SOCK_DGRAM      2
#define KAPI_SOCK_RAW        3
#define KAPI_SOCK_RDM        4
#define KAPI_SOCK_SEQPACKET  5
#define KAPI_SOCK_DCCP       6
#define KAPI_SOCK_PACKET     10
#define KAPI_SOCK_CLOEXEC    0x80000
#define KAPI_SOCK_NONBLOCK   0x800

#define KAPI_MSG_OOB        0x0001
#define KAPI_MSG_PEEK       0x0002
#define KAPI_MSG_DONTROUTE  0x0004
#define KAPI_MSG_CTRUNC     0x0008
#define KAPI_MSG_PROXY      0x0010
#define KAPI_MSG_TRUNC      0x0020
#define KAPI_MSG_DONTWAIT   0x0040
#define KAPI_MSG_EOR        0x0080
#define KAPI_MSG_WAITALL    0x0100
#define KAPI_MSG_FIN        0x0200
#define KAPI_MSG_SYN        0x0400
#define KAPI_MSG_CONFIRM    0x0800
#define KAPI_MSG_RST        0x1000
#define KAPI_MSG_ERRQUEUE   0x2000
#define KAPI_MSG_NOSIGNAL   0x4000
#define KAPI_MSG_MORE       0x8000
#define KAPI_MSG_WAITFORONE 0x10000
#define KAPI_MSG_BATCH      0x40000
#define KAPI_MSG_ZEROCOPY   0x4000000
#define KAPI_MSG_FASTOPEN   0x20000000

#define KAPI_SHUT_RD   0
#define KAPI_SHUT_WR   1
#define KAPI_SHUT_RDWR 2

#define KAPI_SOL_SOCKET   1
#define KAPI_SO_DEBUG     1
#define KAPI_SO_REUSEADDR 2
#define KAPI_SO_TYPE      3
#define KAPI_SO_ERROR     4
#define KAPI_SO_DONTROUTE 5
#define KAPI_SO_BROADCAST 6
#define KAPI_SO_SNDBUF    7
#define KAPI_SO_RCVBUF    8
#define KAPI_SO_KEEPALIVE 9
#define KAPI_SO_OOBINLINE 10
#define KAPI_SO_LINGER    13
#define KAPI_SO_RCVLOWAT  16
#define KAPI_SNDLOWAT     17
#define KAPI_RCVTIMEO     20
#define KAPI_SNDTIMEO     21
#define KAPI_SO_ACCEPTCONN 30
#define KAPI_SO_PROTOCOL  38

typedef uint16_t kapi_sa_family_t;
typedef uint32_t kapi_in_addr_t;
typedef uint8_t kapi_in6_addr_t[16];

struct kapi_sockaddr {
    kapi_sa_family_t sa_family;
    char sa_data[14];
};

struct kapi_sockaddr_in {
    kapi_sa_family_t sin_family;
    uint16_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct kapi_sockaddr_in6 {
    kapi_sa_family_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

struct kapi_linger {
    int l_onoff;
    int l_linger;
};

typedef struct {
    int domain;
    int type;
    int protocol;
    int state;
    int flags;
    int backlog;
    int send_buf_size;
    int recv_buf_size;
    int send_timeout;
    int recv_timeout;
    int linger_enabled;
    int linger_time;
    bool reuse_addr;
    bool keep_alive;
    bool broadcast;
    bool oob_inline;
    bool dont_route;
    bool non_blocking;
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t errors;
    uint64_t connects;
    uint64_t accepts;
} kapi_socket_info_t;

typedef struct {
    char name[64];
    int index;
    int flags;
    int mtu;
    int type;
    int family;
    uint8_t mac_addr[6];
    uint32_t ipv4_addr;
    struct in6_addr ipv6_addr;
    uint32_t netmask;
    uint32_t broadcast;
    uint32_t ptp_peer;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
    uint64_t collisions;
    uint64_t multicast_tx;
    uint64_t multicast_rx;
    bool is_up;
    bool is_running;
    bool is_loopback;
    bool is_promisc;
    bool is_multicast;
    bool is_allmulti;
    bool noarp;
    bool promiscuous;
} kapi_netif_info_t;

typedef struct {
    uint32_t src_addr;
    uint16_t src_port;
    uint32_t dst_addr;
    uint16_t dst_port;
    int protocol;
    size_t length;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t window;
    uint16_t flags;
    int ttl;
    int tos;
    void* data;
} kapi_packet_info_t;

typedef void (*kapi_packet_handler_t)(int socket, const kapi_packet_info_t* packet, void* user_data);
typedef void (*kapi_connection_handler_t)(int client_socket, uint32_t addr, uint16_t port, void* user_data);
typedef void (*kapi_error_handler_t)(int socket, int error_code, void* user_data);

int kapi_socket(int domain, int type, int protocol);

int kapi_socketpair(int domain, int type, int protocol, int sv[2]);

int kapi_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

int kapi_listen(int sockfd, int backlog);

int kapi_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

int kapi_accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags);

int kapi_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

ssize_t kapi_send(int sockfd, const void* buf, size_t len, int flags);

ssize_t kapi_recv(int sockfd, void* buf, size_t len, int flags);

ssize_t kapi_sendto(int sockfd, const void* buf, size_t len, int flags,
                    const struct sockaddr* dest_addr, socklen_t addrlen);

ssize_t kapi_recvfrom(int sockfd, void* buf, size_t len, int flags,
                      struct sockaddr* src_addr, socklen_t* addrlen);

ssize_t kapi_sendmsg(int sockfd, const struct msghdr* msg, int flags);

ssize_t kapi_recvmsg(int sockfd, struct msghdr* msg, int flags);

int kapi_getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen);

int kapi_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);

int kapi_getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

int kapi_getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

int kapi_shutdown(int sockfd, int how);

int kapi_close(int sockfd);

uint16_t kapi_htons(uint16_t hostshort);

uint32_t kapi_htonl(uint32_t hostlong);

uint16_t kapi_ntohs(uint16_t netshort);

uint32_t kapi_ntohl(uint32_t netlong);

int kapi_inet_pton(int af, const char* src, void* dst);

const char* kapi_inet_ntop(int af, const void* src, char* dst, socklen_t size);

char* kapi_inet_ntoa(struct in_addr in);

struct in_addr kapi_inet_makeaddr(int net, int host);

unsigned long kapi_inet_lnaof(struct in_addr in);

unsigned long kapi_inet_netof(struct in_addr in);

struct in_addr kapi_inet_network(const char* cp);

int kapi_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
                struct timeval* timeout);

int kapi_pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
                 const struct timespec* timeout, const sigset_t* sigmask);

int kapi_poll(struct pollfd* fds, nfds_t nfds, int timeout);

int kapi_ppoll(struct pollfd* fds, nfds_t nfds, const struct timespec* tmo_p,
               const sigset_t* sigmask);

int kapi_epoll_create1(int flags);

int kapi_epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);

int kapi_epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

int kapi_epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout,
                     const sigset_t* sigmask);

int kapi_get_socket_info(int sockfd, kapi_socket_info_t* info);

int kapi_set_nonblocking(int sockfd, bool nonblocking);

int kapi_set_reuseaddr(int sockfd, bool reuse);

int kapi_set_keepalive(int sockfd, bool keepalive, int idle, int interval, int count);

int kapi_set_broadcast(int sockfd, bool broadcast);

int kapi_set_linger(int sockfd, bool enabled, int seconds);

int kapi_set_sndbuf(int sockfd, int size);

int kapi_set_rcvbuf(int sockfd, int size);

int kapi_set_timeout(int sockfd, int send_timeout, int recv_timeout);

int kapi_tcp_nodelay(int sockfd, bool nodelay);

int kapi_tcp_cork(int sockfd, bool cork);

int kapi_tcp_keepidle(int sockfd, int seconds);

int kapi_tcp_keepintvl(int sockfd, int seconds);

int kapi_tcp_keepcnt(int sockfd, int probes);

int kapi_tcp_quickack(int sockfd, bool quickack);

int kapi_tcp_congestion(int sockfd, const char* name);

int kapi_tcp_window_clamp(int sockfd, int window);

int kapi_udp_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

int kapi_udp_disconnect(int sockfd);

int kapi_get_ifcount(void);

int kapi_get_iflist(kapi_netif_info_t* ifs, int count);

int kapi_get_ifinfo(const char* name, kapi_netif_info_t* info);

int kapi_set_ifup(const char* name);

int kapi_set_ifdown(const char* name);

int kapi_set_ifaddr(const char* name, uint32_t addr);

int kapi_set_ifnetmask(const char* name, uint32_t mask);

int kapi_set_ifbroadcast(const char* name, uint32_t addr);

int kapi_set_ifmtu(const char* name, int mtu);

int kapi_set_ifflags(const char* name, int flags);

int kapi_set_ifmac(const char* name, const uint8_t mac[6]);

int kapi_set_promisc(const char* name, bool enable);

int kapi_register_packet_handler(int sockfd, kapi_packet_handler_t handler, void* data);

int kapi_unregister_packet_handler(int sockfd);

int kapi_register_connection_handler(int server_fd, kapi_connection_handler_t handler, void* data);

int kapi_unregister_connection_handler(int server_fd);

int kapi_register_error_handler(int sockfd, kapi_error_handler_t handler, void* data);

int kapi_unregister_error_handler(int sockfd);

#ifdef __cplusplus
}
#endif

#endif