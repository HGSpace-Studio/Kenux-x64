#include <arch/net.h>
#include <arch/memory.h>
#include <arch/spinlock.h>
#include <string.h>
#include <arch/time.h>

#define NET_MAX_SOCKETS        1024
#define NET_MAX_CONNECTIONS   4096
#define NET_MAX_LISTEN_BACKLOG 128
#define NET_RECV_BUFFER_SIZE  (256 * 1024)
#define NET_SEND_BUFFER_SIZE  (256 * 1024)
#define NET_MAX_PACKET_QUEUE  512
#define NET_TCP_WINDOW_SIZE   (64 * 1024)
#define NET_MAX_RETRIES       10
#define NET_TIMEOUT_MS        30000

typedef enum {
    SOCKET_STATE_FREE = 0,
    SOCKET_STATE_CREATED,
    SOCKET_STATE_BOUND,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSING,
    SOCKET_STATE_CLOSED
} socket_state_t;

typedef struct tcp_segment {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t window_size;
    uint16_t flags;
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20
    uint16_t urgent_ptr;
    uint8_t* data;
    size_t data_len;
    uint32_t checksum;
} tcp_segment_t;

typedef struct connection_info {
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;
    socket_state_t state;
    uint32_t send_seq;
    uint32_t recv_seq;
    uint32_t send_window;
    uint32_t recv_window;
    uint64_t last_activity_time;
    uint64_t connection_time;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    int retry_count;
    spinlock_t lock;
    void* user_data;
} connection_info_t;

typedef struct socket_info {
    int fd;
    int domain;
    int type;
    int protocol;
    socket_state_t state;
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;
    int backlog;
    bool non_blocking;
    bool reuse_addr;
    bool keep_alive;
    int send_timeout_ms;
    int recv_timeout_ms;
    size_t send_buf_size;
    size_t recv_buf_size;
    uint8_t* send_buffer;
    uint8_t* recv_buffer;
    size_t send_buf_pos;
    size_t recv_buf_pos;
    connection_info_t* connection;
    int listen_fds[NET_MAX_LISTEN_BACKLOG];
    int listen_count;
    spinlock_t lock;
    void (*on_connect)(struct socket_info*);
    void (*on_data)(struct socket_info*, const void*, size_t);
    void (*on_disconnect)(struct socket_info*);
    void (*on_error)(struct socket_info*, int);
    uint64_t create_time;
    uint64_t last_io_time;
    uint64_t total_send_bytes;
    uint64_t total_recv_bytes;
    uint64_t send_count;
    uint64_t recv_count;
    char name[64];
} socket_info_t;

typedef struct packet_queue_entry {
    uint8_t* data;
    size_t length;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
    uint64_t timestamp;
    struct packet_queue_entry* next;
} packet_queue_entry_t;

typedef struct network_stats {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t sockets_created;
    uint64_t sockets_destroyed;
    uint64_t connections_established;
    uint64_t connections_closed;
    uint64_t connections_refused;
    uint64_t connections_timeout;
    uint64_t retransmissions;
    uint64_t checksum_errors;
    uint64_t dropped_packets;
    uint64_t queue_overflows;
    double avg_latency_ns;
    double max_latency_ns;
    uint64_t current_sockets;
    uint64_t active_connections;
    uint64_t memory_used;
    int cpu_utilization;
} network_stats_t;

static socket_info_t sockets[NET_MAX_SOCKETS];
static packet_queue_entry_t* packet_queue_head = NULL;
static packet_queue_entry_t* packet_queue_tail = NULL;
static network_stats_t net_stats;
static spinlock_t net_global_lock;
static spinlock_t packet_queue_lock;
static int next_fd = 3;
static int initialized = 0;

void network_init(void)
{
    if (initialized) return;

    spin_init(&net_global_lock);
    spin_init(&packet_queue_lock);

    memset(sockets, 0, sizeof(sockets));
    memset(&net_stats, 0, sizeof(network_stats_t));

    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        sockets[i].fd = -1;
        spin_init(&sockets[i].lock);
    }

    initialized = 1;
}

int socket_create(int domain, int type, int protocol)
{
    if (!initialized) return -EINVAL;

    if (domain != AF_INET && domain != AF_INET6) {
        return -EAFNOSUPPORT;
    }

    if (type != SOCK_STREAM && type != SOCK_DGRAM &&
        type != SOCK_RAW && type != SOCK_SEQPACKET) {
        return -ESOCKTNOSUPPORT;
    }

    spin_lock(&net_global_lock);

    int fd = -1;
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (sockets[i].fd == -1) {
            fd = next_fd++;
            break;
        }
    }

    if (fd < 0) {
        spin_unlock(&net_global_lock);
        return -EMFILE;
    }

    socket_info_t* sock = &sockets[fd % NET_MAX_SOCKETS];
    memset(sock, 0, sizeof(socket_info_t));

    sock->fd = fd;
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = SOCKET_STATE_CREATED;
    sock->local_addr = INADDR_ANY;
    sock->local_port = 0;
    sock->remote_addr = INADDR_ANY;
    sock->remote_port = 0;
    sock->backlog = 0;
    sock->non_blocking = false;
    sock->reuse_addr = false;
    sock->keep_alive = false;
    sock->send_timeout_ms = NET_TIMEOUT_MS;
    sock->recv_timeout_ms = NET_TIMEOUT_MS;
    sock->send_buf_size = NET_SEND_BUFFER_SIZE;
    sock->recv_buf_size = NET_RECV_BUFFER_SIZE;
    sock->send_buf_pos = 0;
    sock->recv_buf_pos = 0;
    sock->connection = NULL;
    sock->listen_count = 0;
    sock->create_time = get_current_time_ns();
    sock->last_io_time = 0;
    sock->total_send_bytes = 0;
    sock->total_recv_bytes = 0;
    sock->send_count = 0;
    sock->recv_count = 0;
    snprintf(sock->name, sizeof(sock->name), "socket_%d", fd);

    spin_init(&sock->lock);

    sock->send_buffer = kzalloc(NET_SEND_BUFFER_SIZE);
    sock->recv_buffer = kzalloc(NET_RECV_BUFFER_SIZE);

    if (!sock->send_buffer || !sock->recv_buffer) {
        if (sock->send_buffer) kfree(sock->send_buffer);
        if (sock->recv_buffer) kfree(sock->recv_buffer);
        memset(sock, 0, sizeof(socket_info_t));
        sock->fd = -1;
        spin_unlock(&net_global_lock);
        return -ENOMEM;
    }

    net_stats.sockets_created++;
    net_stats.current_sockets++;
    net_stats.memory_used += NET_SEND_BUFFER_SIZE + NET_RECV_BUFFER_SIZE;

    spin_unlock(&net_global_lock);
    return fd;
}

int socket_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (!initialized || !addr || addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->state != SOCKET_STATE_CREATED) {
        spin_unlock(&sock->lock);
        return -EINVAL;
    }

    const struct sockaddr_in* addr_in = (const struct sockaddr_in*)addr;

    if (addr_in->sin_family != AF_INET) {
        spin_unlock(&sock->lock);
        return -EAFNOSUPPORT;
    }

    sock->local_addr = addr_in->sin_addr.s_addr;
    sock->local_port = ntohs(addr_in->sin_port);
    sock->state = SOCKET_STATE_BOUND;

    spin_unlock(&sock->lock);
    return 0;
}

int socket_listen(int sockfd, int backlog)
{
    if (!initialized) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->type != SOCK_STREAM || sock->state != SOCKET_STATE_BOUND) {
        spin_unlock(&sock->lock);
        return -EOPNOTSUPP;
    }

    if (backlog <= 0) backlog = SOMAXCONN;
    if (backlog > NET_MAX_LISTEN_BACKLOG) backlog = NET_MAX_LISTEN_BACKLOG;

    sock->backlog = backlog;
    sock->listen_count = 0;
    sock->state = SOCKET_STATE_LISTENING;

    spin_unlock(&sock->lock);
    return 0;
}

int socket_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    if (!initialized) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* server_sock = &sockets[sockfd];
    spin_lock(&server_sock->lock);

    if (server_sock->state != SOCKET_STATE_LISTENING) {
        spin_unlock(&server_sock->lock);
        return -EINVAL;
    }

    while (server_sock->listen_count == 0) {
        spin_unlock(&server_sock->lock);

        if (server_sock->non_blocking) {
            return -EAGAIN;
        }

        sleep_ms(1);
        spin_lock(&server_sock->lock);
    }

    int client_fd = server_sock->listen_fds[--server_sock->listen_count];

    spin_unlock(&server_sock->lock);

    if (client_fd < 0 || client_fd >= NET_MAX_SOCKETS ||
        sockets[client_fd].fd == -1) {
        return -ECONNABORTED;
    }

    socket_info_t* client_sock = &sockets[client_fd];

    if (addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* client_addr = (struct sockaddr_in*)addr;
        client_addr->sin_family = AF_INET;
        client_addr->sin_addr.s_addr = client_sock->remote_addr;
        client_addr->sin_port = htons(client_sock->remote_port);
        *addrlen = sizeof(struct sockaddr_in);
    }

    client_sock->state = SOCKET_STATE_CONNECTED;
    net_stats.connections_established++;
    net_stats.active_connections++;

    if (server_sock->on_connect) {
        server_sock->on_connect(client_sock);
    }

    return client_fd;
}

int socket_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (!initialized || !addr || addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->type != SOCK_STREAM || sock->state != SOCKET_STATE_CREATED &&
        sock->state != SOCKET_STATE_BOUND) {
        spin_unlock(&sock->lock);
        return -EOPNOTSUPP;
    }

    const struct sockaddr_in* addr_in = (const struct sockaddr_in*)addr;

    if (addr_in->sin_family != AF_INET) {
        spin_unlock(&sock->lock);
        return -EAFNOSUPPORT;
    }

    sock->remote_addr = addr_in->sin_addr.s_addr;
    sock->remote_port = ntohs(addr_in->sin_port);
    sock->state = SOCKET_STATE_CONNECTING;

    connection_info_t* conn = kzalloc(sizeof(connection_info_t));
    if (!conn) {
        sock->state = SOCKET_STATE_CREATED;
        spin_unlock(&sock->lock);
        return -ENOMEM;
    }

    conn->local_addr = sock->local_addr;
    conn->local_port = sock->local_port;
    conn->remote_addr = sock->remote_addr;
    conn->remote_port = sock->remote_port;
    conn->state = SOCKET_STATE_CONNECTING;
    conn->send_seq = generate_random_uint32();
    conn->recv_seq = 0;
    conn->send_window = NET_TCP_WINDOW_SIZE;
    conn->recv_window = NET_TCP_WINDOW_SIZE;
    conn->last_activity_time = get_current_time_ns();
    conn->connection_time = conn->last_activity_time;
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    conn->retry_count = 0;
    spin_init(&conn->lock);

    sock->connection = conn;

    spin_unlock(&sock->lock);

    tcp_segment_t syn_seg;
    memset(&syn_seg, 0, sizeof(tcp_segment_t));
    syn_seg.seq_num = conn->send_seq;
    syn_seg.flags = TCP_FLAG_SYN;

    if (tcp_send_segment(sock, &syn_seg) < 0) {
        spin_lock(&sock->lock);
        sock->state = SOCKET_STATE_CREATED;
        kfree(conn);
        sock->connection = NULL;
        spin_unlock(&sock->lock);
        return -ECONNREFUSED;
    }

    uint64_t start_time = get_current_time_ns();
    while (true) {
        spin_lock(&sock->lock);

        if (sock->state == SOCKET_STATE_CONNECTED) {
            spin_unlock(&sock->lock);
            break;
        }

        if (sock->state == SOCKET_STATE_CLOSED) {
            kfree(conn);
            sock->connection = NULL;
            spin_unlock(&sock->lock);
            return -ECONNREFUSED;
        }

        uint64_t elapsed = (get_current_time_ns() - start_time) / 1000000ULL;
        if (elapsed > sock->send_timeout_ms) {
            conn->retry_count++;
            if (conn->retry_count > NET_MAX_RETRIES) {
                sock->state = SOCKET_STATE_CLOSED;
                kfree(conn);
                sock->connection = NULL;
                spin_unlock(&sock->lock);
                return -ETIMEDOUT;
            }
            tcp_send_segment(sock, &syn_seg);
            start_time = get_current_time_ns();
        }

        spin_unlock(&sock->lock);
        sleep_ms(1);
    }

    net_stats.connections_established++;
    net_stats.active_connections++;

    if (sock->on_connect) {
        sock->on_connect(sock);
    }

    return 0;
}

ssize_t socket_send(int sockfd, const void* buf, size_t len, int flags)
{
    if (!initialized || !buf || len == 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->state != SOCKET_STATE_CONNECTED) {
        spin_unlock(&sock->lock);
        return -ENOTCONN;
    }

    uint64_t start_time = get_current_time_ns();

    size_t remaining = len;
    const uint8_t* data = (const uint8_t*)buf;

    while (remaining > 0) {
        size_t space = sock->send_buf_size - sock->send_buf_pos;
        size_t to_copy = (remaining < space) ? remaining : space;

        memcpy(sock->send_buffer + sock->send_buf_pos, data, to_copy);
        sock->send_buf_pos += to_copy;
        data += to_copy;
        remaining -= to_copy;

        if (sock->send_buf_pos >= sock->send_buf_size || (flags & MSG_DONTWAIT) == 0) {
            tcp_segment_t seg;
            memset(&seg, 0, sizeof(tcp_segment_t));

            if (sock->connection) {
                seg.seq_num = sock->connection->send_seq;
                sock->connection->send_seq += sock->send_buf_pos;
            }

            seg.flags = TCP_FLAG_PSH | TCP_FLAG_ACK;
            seg.data = sock->send_buffer;
            seg.data_len = sock->send_buf_pos;

            ssize_t sent = tcp_send_segment(sock, &seg);
            if (sent < 0) {
                sock->send_buf_pos -= to_copy;
                spin_unlock(&sock->lock);
                return sent;
            }

            sock->total_send_bytes += sock->send_buf_pos;
            sock->send_buf_pos = 0;
        }
    }

    sock->send_count++;
    sock->last_io_time = get_current_time_ns();

    uint64_t elapsed = get_current_time_ns() - start_time;
    net_stats.avg_latency_ns =
        (net_stats.avg_latency_ns + elapsed) / 2;
    if (elapsed > net_stats.max_latency_ns) {
        net_stats.max_latency_ns = elapsed;
    }

    net_stats.packets_sent++;
    net_stats.bytes_sent += len;

    spin_unlock(&sock->lock);
    return len;
}

ssize_t socket_recv(int sockfd, void* buf, size_t len, int flags)
{
    if (!initialized || !buf || len == 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->state != SOCKET_STATE_CONNECTED) {
        spin_unlock(&sock->lock);
        return -ENOTCONN;
    }

    uint64_t start_time = get_current_time_ns();

    while (sock->recv_buf_pos == 0) {
        spin_unlock(&sock->lock);

        if (sock->non_blocking || (flags & MSG_DONTWAIT)) {
            return -EAGAIN;
        }

        if (get_current_time_ns() - sock->last_io_time >
            (uint64_t)sock->recv_timeout_ms * 1000000ULL) {
            return -ETIMEDOUT;
        }

        sleep_ms(1);
        spin_lock(&sock->lock);
    }

    size_t to_read = (len < sock->recv_buf_pos) ? len : sock->recv_buf_pos;
    memcpy(buf, sock->recv_buffer, to_read);

    memmove(sock->recv_buffer, sock->recv_buffer + to_read,
           sock->recv_buf_pos - to_read);
    sock->recv_buf_pos -= to_read;

    sock->recv_count++;
    sock->last_io_time = get_current_time_ns();
    sock->total_recv_bytes += to_read;

    uint64_t elapsed = get_current_time_ns() - start_time;
    net_stats.avg_latency_ns =
        (net_stats.avg_latency_ns + elapsed) / 2;

    net_stats.packets_received++;
    net_stats.bytes_received += to_read;

    if (sock->on_data) {
        sock->on_data(sock, buf, to_read);
    }

    spin_unlock(&sock->lock);
    return to_read;
}

ssize_t socket_sendto(int sockfd, const void* buf, size_t len, int flags,
                     const struct sockaddr* dest_addr, socklen_t addrlen)
{
    if (!initialized || !buf || len == 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (dest_addr && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in* addr_in = (const struct sockaddr_in*)dest_addr;
        sock->remote_addr = addr_in->sin_addr.s_addr;
        sock->remote_port = ntohs(addr_in->sin_port);
    }

    if (sock->type == SOCK_DGRAM) {
        ssize_t result = udp_send_packet(sock, buf, len);
        if (result > 0) {
            sock->total_send_bytes += result;
            sock->send_count++;
            sock->last_io_time = get_current_time_ns();
            net_stats.packets_sent++;
            net_stats.bytes_sent += result;
        }
        spin_unlock(&sock->lock);
        return result;
    }

    spin_unlock(&sock->lock);
    return socket_send(sockfd, buf, len, flags);
}

ssize_t socket_recvfrom(int sockfd, void* buf, size_t len, int flags,
                       struct sockaddr* src_addr, socklen_t* addrlen)
{
    if (!initialized || !buf || len == 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->type == SOCK_DGRAM) {
        uint32_t from_ip;
        uint16_t from_port;

        ssize_t result = udp_recv_packet(sock, buf, len, &from_ip, &from_port);
        if (result > 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)src_addr;
            addr_in->sin_family = AF_INET;
            addr_in->sin_addr.s_addr = from_ip;
            addr_in->sin_port = htons(from_port);
            *addrlen = sizeof(struct sockaddr_in);

            sock->total_recv_bytes += result;
            sock->recv_count++;
            sock->last_io_time = get_current_time_ns();
            net_stats.packets_received++;
            net_stats.bytes_received += result;
        }
        spin_unlock(&sock->lock);
        return result;
    }

    spin_unlock(&sock->lock);
    return socket_recv(sockfd, buf, len, flags);
}

int socket_close(int sockfd)
{
    if (!initialized) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    if (sock->state == SOCKET_STATE_CLOSED || sock->state == SOCKET_STATE_CLOSING) {
        spin_unlock(&sock->lock);
        return 0;
    }

    if (sock->state == SOCKET_STATE_CONNECTED && sock->type == SOCK_STREAM) {
        tcp_segment_t fin_seg;
        memset(&fin_seg, 0, sizeof(tcp_segment_t));

        if (sock->connection) {
            fin_seg.seq_num = sock->connection->send_seq;
        }
        fin_seg.flags = TCP_FLAG_FIN | TCP_FLAG_ACK;

        tcp_send_segment(sock, &fin_seg);
        sock->state = SOCKET_STATE_CLOSING;

        net_stats.connections_closed--;
        net_stats.active_connections--;

        if (sock->on_disconnect) {
            sock->on_disconnect(sock);
        }
    } else {
        sock->state = SOCKET_STATE_CLOSED;
    }

    if (sock->connection) {
        kfree(sock->connection);
        sock->connection = NULL;
    }

    if (sock->send_buffer) {
        kfree(sock->send_buffer);
        net_stats.memory_used -= NET_SEND_BUFFER_SIZE;
    }

    if (sock->recv_buffer) {
        kfree(sock->recv_buffer);
        net_stats.memory_used -= NET_RECV_BUFFER_SIZE;
    }

    net_stats.sockets_destroyed++;
    net_stats.current_sockets--;

    int saved_fd = sock->fd;
    memset(sock, 0, sizeof(socket_info_t));
    sock->fd = -1;

    spin_unlock(&sock->lock);
    return 0;
}

int socket_setsockopt(int sockfd, int level, int optname,
                      const void* optval, socklen_t optlen)
{
    if (!initialized || !optval || optlen <= 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    switch (level) {
        case SOL_SOCKET:
            switch (optname) {
                case SO_REUSEADDR:
                    if (optlen >= sizeof(int)) {
                        sock->reuse_addr = (*(const int*)optval) != 0;
                    }
                    break;
                case SO_KEEPALIVE:
                    if (optlen >= sizeof(int)) {
                        sock->keep_alive = (*(const int*)optval) != 0;
                    }
                    break;
                case SO_SNDTIMEO:
                    if (optlen >= sizeof(struct timeval)) {
                        const struct timeval* tv = (const struct timeval*)optval;
                        sock->send_timeout_ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
                    }
                    break;
                case SO_RCVTIMEO:
                    if (optlen >= sizeof(struct timeval)) {
                        const struct timeval* tv = (const struct timeval*)optval;
                        sock->recv_timeout_ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
                    }
                    break;
                case SO_SNDBUF:
                    if (optlen >= sizeof(int)) {
                        size_t new_size = *(const int*)optval;
                        if (new_size > 0 && new_size != sock->send_buf_size) {
                            uint8_t* new_buf = krealloc(sock->send_buffer, new_size);
                            if (new_buf) {
                                net_stats.memory_used -= sock->send_buf_size;
                                sock->send_buffer = new_buf;
                                sock->send_buf_size = new_size;
                                net_stats.memory_used += new_size;
                            }
                        }
                    }
                    break;
                case SO_RCVBUF:
                    if (optlen >= sizeof(int)) {
                        size_t new_size = *(const int*)optval;
                        if (new_size > 0 && new_size != sock->recv_buf_size) {
                            uint8_t* new_buf = krealloc(sock->recv_buffer, new_size);
                            if (new_buf) {
                                net_stats.memory_used -= sock->recv_buf_size;
                                sock->recv_buffer = new_buf;
                                sock->recv_buf_size = new_size;
                                net_stats.memory_used += new_size;
                            }
                        }
                    }
                    break;
                default:
                    spin_unlock(&sock->lock);
                    return -ENOPROTOOPT;
            }
            break;
        case IPPROTO_TCP:
            switch (optname) {
                case TCP_NODELAY:
                    break;
                case TCP_KEEPIDLE:
                    break;
                case TCP_KEEPINTVL:
                    break;
                case TCP_KEEPCNT:
                    break;
                default:
                    spin_unlock(&sock->lock);
                    return -ENOPROTOOPT;
            }
            break;
        default:
            spin_unlock(&sock->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&sock->lock);
    return 0;
}

int socket_getsockopt(int sockfd, int level, int optname,
                      void* optval, socklen_t* optlen)
{
    if (!initialized || !optval || !optlen || *optlen <= 0) return -EINVAL;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return -EBADF;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    switch (level) {
        case SOL_SOCKET:
            switch (optname) {
                case SO_ERROR: {
                    if (*optlen >= sizeof(int)) {
                        *(int*)optval = 0;
                        *optlen = sizeof(int);
                    }
                    break;
                }
                case SO_TYPE: {
                    if (*optlen >= sizeof(int)) {
                        *(int*)optval = sock->type;
                        *optlen = sizeof(int);
                    }
                    break;
                }
                default:
                    spin_unlock(&sock->lock);
                    return -ENOPROTOOPT;
            }
            break;
        default:
            spin_unlock(&sock->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&sock->lock);
    return 0;
}

void socket_set_nonblocking(int sockfd, bool non_blocking)
{
    if (!initialized) return;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);
    sock->non_blocking = non_blocking;
    spin_unlock(&sock->lock);
}

void socket_set_callback(int sockfd,
                         void (*on_connect)(socket_info_t*),
                         void (*on_data)(socket_info_t*, const void*, size_t),
                         void (*on_disconnect)(socket_info_t*),
                         void (*on_error)(socket_info_t*, int))
{
    if (!initialized) return;

    if (sockfd < 0 || sockfd >= NET_MAX_SOCKETS || sockets[sockfd].fd == -1) {
        return;
    }

    socket_info_t* sock = &sockets[sockfd];
    spin_lock(&sock->lock);

    sock->on_connect = on_connect;
    sock->on_data = on_data;
    sock->on_disconnect = on_disconnect;
    sock->on_error = on_error;

    spin_unlock(&sock->lock);
}

void process_network_packets(void)
{
    if (!initialized) return;

    spin_lock(&packet_queue_lock);

    packet_queue_entry_t* entry = packet_queue_head;
    while (entry) {
        packet_queue_entry_t* next = entry->next;

        for (int i = 0; i < NET_MAX_SOCKETS; i++) {
            socket_info_t* sock = &sockets[i];
            if (sock->fd == -1) continue;

            spin_lock(&sock->lock);

            bool match = false;
            if (entry->protocol == IPPROTO_TCP) {
                match = (entry->dst_port == sock->local_port) &&
                       ((entry->dst_addr == sock->local_addr) ||
                        (sock->local_addr == INADDR_ANY));
            } else if (entry->protocol == IPPROTO_UDP) {
                match = (entry->dst_port == sock->local_port) &&
                       ((entry->dst_addr == sock->local_addr) ||
                        (sock->local_addr == INADDR_ANY));
            }

            if (match) {
                if (entry->protocol == IPPROTO_TCP) {
                    handle_tcp_packet(sock, entry->data, entry->length,
                                     entry->src_ip, entry->src_port);
                } else if (entry->protocol == IPPROTO_UDP) {
                    handle_udp_packet(sock, entry->data, entry->length,
                                     entry->src_ip, entry->src_port);
                }
            }

            spin_unlock(&sock->lock);
        }

        kfree(entry->data);
        kfree(entry);
        entry = next;
    }

    packet_queue_head = NULL;
    packet_queue_tail = NULL;

    spin_unlock(&packet_queue_lock);
}

int queue_network_packet(const uint8_t* data, size_t length,
                        uint32_t src_ip, uint32_t dst_ip,
                        uint16_t src_port, uint16_t dst_port,
                        uint8_t protocol)
{
    if (!initialized || !data || length == 0) return -EINVAL;

    packet_queue_entry_t* entry = kzalloc(sizeof(packet_queue_entry_t));
    if (!entry) return -ENOMEM;

    entry->data = kzalloc(length);
    if (!entry->data) {
        kfree(entry);
        return -ENOMEM;
    }

    memcpy(entry->data, data, length);
    entry->length = length;
    entry->src_ip = src_ip;
    entry->dst_ip = dst_ip;
    entry->src_port = src_port;
    entry->dst_port = dst_port;
    entry->protocol = protocol;
    entry->timestamp = get_current_time_ns();
    entry->next = NULL;

    spin_lock(&packet_queue_lock);

    if (packet_queue_tail) {
        packet_queue_tail->next = entry;
    } else {
        packet_queue_head = entry;
    }
    packet_queue_tail = entry;

    spin_unlock(&packet_queue_lock);

    net_stats.packets_received++;
    net_stats.bytes_received += length;

    return 0;
}

int network_get_stats(network_stats_t* stats)
{
    if (!stats || !initialized) return -EINVAL;

    memcpy(stats, &net_stats, sizeof(network_stats_t));
    return 0;
}

void network_dump_info(void)
{
    network_stats_t stats;
    network_get_stats(&stats);

    printk("Enhanced Network Statistics:\n");
    printk("  Packets sent:           %llu\n", stats.packets_sent);
    printk("  Packets received:       %llu\n", stats.packets_received);
    printk("  Bytes sent:             %llu KB\n", stats.bytes_sent / 1024ULL);
    printk("  Bytes received:         %llu KB\n", stats.bytes_received / 1024ULL);
    printk("  Sockets created:        %llu\n", stats.sockets_created);
    printk("  Sockets destroyed:      %llu\n", stats.sockets_destroyed);
    printk("  Current sockets:        %llu\n", stats.current_sockets);
    printk("  Connections established:%llu\n", stats.connections_established);
    printk("  Connections closed:     %llu\n", stats.connections_closed);
    printk("  Active connections:     %llu\n", stats.active_connections);
    printk("  Retransmissions:        %llu\n", stats.retransmissions);
    printk("  Checksum errors:        %llu\n", stats.checksum_errors);
    printk("  Dropped packets:        %llu\n", stats.dropped_packets);
    printk("  Queue overflows:        %llu\n", stats.queue_overflows);
    printk("  Avg latency:            %.2f us\n", stats.avg_latency_ns / 1000.0);
    printk("  Max latency:            %.2f us\n", stats.max_latency_ns / 1000.0);
    printk("  Memory used:            %llu KB\n", stats.memory_used / 1024ULL);
}