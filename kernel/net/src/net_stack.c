#include "net_stack.h"
#include <arch/memory.h>
#include <string.h>

static net_iface_t net_ifaces[NET_IFACE_MAX];
static net_socket_t net_sockets[NET_SOCKET_MAX];
static uint16_t net_next_port = NET_PORT_MIN;
static uint16_t icmp_echo_seq = 0;

uint16_t net_htons(uint16_t val) { return (uint16_t)((val << 8) | (val >> 8)); }
uint32_t net_htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
}

uint16_t net_checksum(const void* data, uint32_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint32_t)((uint16_t)p[0] << 8 | p[1]);
        p += 2;
        len -= 2;
    }
    if (len == 1) sum += (uint32_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

uint16_t net_checksum_combine(uint16_t c1, uint16_t c2)
{
    uint32_t sum = (uint32_t)(~c1 & 0xFFFF) + (uint32_t)(~c2 & 0xFFFF);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

void net_init(void)
{
    memset(net_ifaces, 0, sizeof(net_ifaces));
    memset(net_sockets, 0, sizeof(net_sockets));
    for (int i = 0; i < NET_IFACE_MAX; i++) {
        net_ifaces[i].index = i;
        spin_init(&net_ifaces[i].lock);
    }
    for (int i = 0; i < NET_SOCKET_MAX; i++) {
        spin_init(&net_sockets[i].lock);
    }
    net_next_port = NET_PORT_MIN;
}

net_iface_t* net_get_iface(int index)
{
    if (index < 0 || index >= NET_IFACE_MAX) return NULL;
    return &net_ifaces[index];
}

int net_register_iface(int index, mac_addr_t* mac, void* driver_ctx,
                       int (*send)(void*, const void*, uint32_t))
{
    if (index < 0 || index >= NET_IFACE_MAX || !mac || !send) return -1;
    net_iface_t* iface = &net_ifaces[index];
    spinlock_acquire(&iface->lock);
    memcpy(iface->mac.mac, mac->mac, 6);
    iface->driver_ctx = driver_ctx;
    iface->send = send;
    iface->up = 0;
    iface->mtu = 1500;
    iface->has_ipv4 = 0;
    iface->has_ipv6 = 0;
    spinlock_release(&iface->lock);
    return 0;
}

int net_iface_set_ipv4(int index, uint32_t addr, uint32_t mask, uint32_t gw)
{
    if (index < 0 || index >= NET_IFACE_MAX) return -1;
    net_iface_t* iface = &net_ifaces[index];
    spinlock_acquire(&iface->lock);
    iface->ipv4.addr = addr;
    iface->ipv4_mask.addr = mask;
    iface->ipv4_gw.addr = gw;
    iface->has_ipv4 = 1;
    spinlock_release(&iface->lock);
    return 0;
}

int net_iface_set_ipv6(int index, const uint8_t* addr, uint8_t prefix_len)
{
    if (index < 0 || index >= NET_IFACE_MAX || !addr) return -1;
    net_iface_t* iface = &net_ifaces[index];
    spinlock_acquire(&iface->lock);
    memcpy(iface->ipv6.addr, addr, 16);
    iface->ipv6_prefix_len = prefix_len;
    iface->has_ipv6 = 1;
    spinlock_release(&iface->lock);
    return 0;
}

int net_iface_up(int index)
{
    if (index < 0 || index >= NET_IFACE_MAX) return -1;
    net_ifaces[index].up = 1;
    return 0;
}

int net_iface_down(int index)
{
    if (index < 0 || index >= NET_IFACE_MAX) return -1;
    net_ifaces[index].up = 0;
    return 0;
}

int net_send_packet(net_iface_t* iface, const void* data, uint32_t len)
{
    if (!iface || !iface->send || !iface->up || !data) return -1;
    return iface->send(iface->driver_ctx, data, len);
}

static void net_build_ethernet(uint8_t* frame, const mac_addr_t* dst, const mac_addr_t* src,
                               uint16_t proto, const void* payload, uint32_t payload_len)
{
    memcpy(frame, dst->mac, 6);
    memcpy(frame + 6, src->mac, 6);
    frame[12] = (uint8_t)(proto >> 8);
    frame[13] = (uint8_t)(proto & 0xFF);
    memcpy(frame + 14, payload, payload_len);
}

int net_ipv4_send(net_iface_t* iface, uint32_t src, uint32_t dst, uint8_t proto,
                  const void* payload, uint32_t payload_len)
{
    if (!iface || !iface->up || !payload) return -1;

    uint32_t total_len = sizeof(ipv4_header_t) + payload_len;
    if (total_len > (uint32_t)iface->mtu) return -2;

    ipv4_header_t hdr;
    hdr.version_ihl = 0x45;
    hdr.tos = 0;
    hdr.total_length = net_htons((uint16_t)total_len);
    hdr.identification = 0;
    hdr.flags_fragment = net_htons(0x4000);
    hdr.ttl = 64;
    hdr.protocol = proto;
    hdr.header_checksum = 0;
    hdr.src_addr = src;
    hdr.dst_addr = dst;
    hdr.header_checksum = net_checksum(&hdr, sizeof(ipv4_header_t));

    uint8_t frame[NET_PACKET_MAX];
    uint8_t ip_packet[NET_PACKET_MAX];
    memcpy(ip_packet, &hdr, sizeof(ipv4_header_t));
    memcpy(ip_packet + sizeof(ipv4_header_t), payload, payload_len);

    mac_addr_t dst_mac;
    if ((dst & 0xF0000000) == 0xE0000000) {
        dst_mac.mac[0] = 0x01; dst_mac.mac[1] = 0x00;
        dst_mac.mac[2] = 0x5E; dst_mac.mac[3] = (uint8_t)((dst >> 16) & 0x7F);
        dst_mac.mac[4] = (uint8_t)((dst >> 8) & 0xFF);
        dst_mac.mac[5] = (uint8_t)(dst & 0xFF);
    } else {
        memcpy(dst_mac.mac, iface->ipv4_gw.addr ? "\xFF\xFF\xFF\xFF\xFF\xFF" : "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
    }

    net_build_ethernet(frame, &dst_mac, &iface->mac, NET_PROTO_IPV4,
                       ip_packet, total_len);

    return net_send_packet(iface, frame, 14 + total_len);
}

int net_ipv6_send(net_iface_t* iface, const uint8_t* src, const uint8_t* dst,
                  uint8_t proto, const void* payload, uint32_t payload_len)
{
    if (!iface || !iface->up || !src || !dst || !payload) return -1;

    ipv6_header_t hdr;
    hdr.flow_label = net_htonl(0x60000000);
    hdr.payload_length = net_htons((uint16_t)payload_len);
    hdr.next_header = proto;
    hdr.hop_limit = 64;
    memcpy(hdr.src_addr, src, 16);
    memcpy(hdr.dst_addr, dst, 16);

    uint8_t frame[NET_PACKET_MAX];
    uint8_t ip6_packet[NET_PACKET_MAX];
    memcpy(ip6_packet, &hdr, 40);
    memcpy(ip6_packet + 40, payload, payload_len);

    mac_addr_t dst_mac;
    if (dst[0] == 0xFF) {
        dst_mac.mac[0] = 0x33; dst_mac.mac[1] = 0x33;
        dst_mac.mac[2] = dst[12]; dst_mac.mac[3] = dst[13];
        dst_mac.mac[4] = dst[14]; dst_mac.mac[5] = dst[15];
    } else {
        memset(dst_mac.mac, 0xFF, 6);
    }

    net_build_ethernet(frame, &dst_mac, &iface->mac, NET_PROTO_IPV6,
                       ip6_packet, 40 + payload_len);

    return net_send_packet(iface, frame, 14 + 40 + payload_len);
}

int net_icmp_send_echo(net_iface_t* iface, uint32_t dst, uint16_t id, uint16_t seq,
                       const void* data, uint32_t data_len)
{
    if (!iface || !iface->has_ipv4) return -1;

    icmp_header_t icmp;
    icmp.type = ICMP_TYPE_ECHO_REQUEST;
    icmp.code = 0;
    icmp.checksum = 0;
    icmp.identifier = net_htons(id);
    icmp.sequence = net_htons(seq);

    uint32_t total_len = sizeof(icmp_header_t) + data_len;
    uint8_t buf[NET_PACKET_MAX];
    memcpy(buf, &icmp, sizeof(icmp_header_t));
    if (data && data_len > 0) memcpy(buf + sizeof(icmp_header_t), data, data_len);
    ((icmp_header_t*)buf)->checksum = net_checksum(buf, total_len);

    return net_ipv4_send(iface, iface->ipv4.addr, dst, NET_PROTO_ICMP, buf, total_len);
}

int net_icmpv6_send_echo(net_iface_t* iface, const uint8_t* dst, uint16_t id, uint16_t seq,
                          const void* data, uint32_t data_len)
{
    if (!iface || !iface->has_ipv6 || !dst) return -1;

    icmp_header_t icmp;
    icmp.type = ICMPV6_TYPE_ECHO_REQUEST;
    icmp.code = 0;
    icmp.checksum = 0;
    icmp.identifier = net_htons(id);
    icmp.sequence = net_htons(seq);

    uint32_t total_len = sizeof(icmp_header_t) + data_len;
    uint8_t buf[NET_PACKET_MAX];
    memcpy(buf, &icmp, sizeof(icmp_header_t));
    if (data && data_len > 0) memcpy(buf + sizeof(icmp_header_t), data, data_len);

    return net_ipv6_send(iface, iface->ipv6.addr, dst, NET_PROTO_ICMPV6, buf, total_len);
}

static uint16_t net_alloc_port(void)
{
    uint16_t port = net_next_port++;
    if (net_next_port > NET_PORT_MAX) net_next_port = NET_PORT_MIN;
    return port;
}

net_socket_t* net_socket_create(int domain, int type, int protocol)
{
    for (int i = 0; i < NET_SOCKET_MAX; i++) {
        if (!net_sockets[i].used) {
            net_socket_t* sock = &net_sockets[i];
            memset(sock, 0, sizeof(net_socket_t));
            sock->used = 1;
            sock->domain = domain;
            sock->type = type;
            sock->protocol = protocol;
            sock->state = TCP_STATE_CLOSED;
            sock->window_size = 65535;
            sock->mss = 1460;
            sock->cwnd = 1;
            sock->ssthresh = 65535;
            sock->retransmit_timeout = 3000;
            sock->recv_buf_size = 65536;
            sock->send_buf_size = 65536;
            sock->recv_buffer = memory_alloc(sock->recv_buf_size);
            sock->send_buffer = memory_alloc(sock->send_buf_size);
            spin_init(&sock->lock);
            return sock;
        }
    }
    return NULL;
}

void net_socket_destroy(net_socket_t* sock)
{
    if (!sock || !sock->used) return;
    if (sock->recv_buffer) memory_free(sock->recv_buffer);
    if (sock->send_buffer) memory_free(sock->send_buffer);
    sock->used = 0;
}

int net_tcp_connect(net_socket_t* sock, uint32_t remote_addr, uint16_t remote_port)
{
    if (!sock || !sock->used) return -1;

    spinlock_acquire(&sock->lock);
    sock->remote_addr.addr = remote_addr;
    sock->remote_port = remote_port;
    sock->local_port = net_alloc_port();
    sock->seq_num = 1000;
    sock->ack_num = 0;
    sock->state = TCP_STATE_SYN_SENT;

    uint8_t buf[sizeof(tcp_header_t)];
    tcp_header_t* tcp = (tcp_header_t*)buf;
    tcp->src_port = net_htons(sock->local_port);
    tcp->dst_port = net_htons(sock->remote_port);
    tcp->seq_num = net_htonl(sock->seq_num);
    tcp->ack_num = 0;
    tcp->data_offset = 0x50;
    tcp->flags = TCP_FLAG_SYN;
    tcp->window_size = net_htons((uint16_t)sock->window_size);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    net_iface_t* iface = net_get_iface(0);
    if (iface && iface->has_ipv4) {
        sock->local_addr = iface->ipv4;
        net_ipv4_send(iface, iface->ipv4.addr, remote_addr, NET_PROTO_TCP,
                      buf, sizeof(tcp_header_t));
    }

    spinlock_release(&sock->lock);
    return 0;
}

int net_tcp_listen(net_socket_t* sock, uint16_t port)
{
    if (!sock || !sock->used) return -1;
    spinlock_acquire(&sock->lock);
    sock->local_port = port;
    sock->state = TCP_STATE_LISTEN;
    spinlock_release(&sock->lock);
    return 0;
}

net_socket_t* net_tcp_accept(net_socket_t* sock)
{
    if (!sock || !sock->used || sock->state != TCP_STATE_LISTEN) return NULL;
    return NULL;
}

int net_tcp_send(net_socket_t* sock, const void* data, uint32_t len)
{
    if (!sock || !sock->used || !data || sock->state != TCP_STATE_ESTABLISHED) return -1;

    spinlock_acquire(&sock->lock);

    uint32_t total_len = sizeof(tcp_header_t) + len;
    uint8_t buf[NET_PACKET_MAX];
    tcp_header_t* tcp = (tcp_header_t*)buf;
    tcp->src_port = net_htons(sock->local_port);
    tcp->dst_port = net_htons(sock->remote_port);
    tcp->seq_num = net_htonl(sock->seq_num);
    tcp->ack_num = net_htonl(sock->ack_num);
    tcp->data_offset = 0x50;
    tcp->flags = TCP_FLAG_ACK | TCP_FLAG_PSH;
    tcp->window_size = net_htons((uint16_t)sock->window_size);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    memcpy(buf + sizeof(tcp_header_t), data, len);

    net_iface_t* iface = net_get_iface(0);
    if (iface && iface->has_ipv4) {
        net_ipv4_send(iface, sock->local_addr.addr, sock->remote_addr.addr,
                      NET_PROTO_TCP, buf, total_len);
    }

    sock->seq_num += len;
    spinlock_release(&sock->lock);
    return (int)len;
}

int net_tcp_recv(net_socket_t* sock, void* buf, uint32_t len)
{
    if (!sock || !sock->used || !buf) return -1;

    spinlock_acquire(&sock->lock);
    if (sock->recv_data_len == 0) {
        spinlock_release(&sock->lock);
        return 0;
    }

    uint32_t to_copy = len;
    if (to_copy > sock->recv_data_len) to_copy = sock->recv_data_len;
    memcpy(buf, (uint8_t*)sock->recv_buffer + sock->recv_buf_pos, to_copy);
    sock->recv_buf_pos += to_copy;
    sock->recv_data_len -= to_copy;
    spinlock_release(&sock->lock);
    return (int)to_copy;
}

int net_tcp_close(net_socket_t* sock)
{
    if (!sock || !sock->used) return -1;

    spinlock_acquire(&sock->lock);
    if (sock->state == TCP_STATE_ESTABLISHED) {
        uint8_t buf[sizeof(tcp_header_t)];
        tcp_header_t* tcp = (tcp_header_t*)buf;
        tcp->src_port = net_htons(sock->local_port);
        tcp->dst_port = net_htons(sock->remote_port);
        tcp->seq_num = net_htonl(sock->seq_num);
        tcp->ack_num = net_htonl(sock->ack_num);
        tcp->data_offset = 0x50;
        tcp->flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
        tcp->window_size = 0;
        tcp->checksum = 0;
        tcp->urgent_ptr = 0;

        net_iface_t* iface = net_get_iface(0);
        if (iface && iface->has_ipv4) {
            net_ipv4_send(iface, sock->local_addr.addr, sock->remote_addr.addr,
                          NET_PROTO_TCP, buf, sizeof(tcp_header_t));
        }
        sock->state = TCP_STATE_FIN_WAIT_1;
    } else {
        sock->state = TCP_STATE_CLOSED;
    }
    spinlock_release(&sock->lock);
    return 0;
}

int net_udp_send(net_socket_t* sock, uint32_t remote_addr, uint16_t remote_port,
                 const void* data, uint32_t len)
{
    if (!sock || !sock->used || !data) return -1;

    spinlock_acquire(&sock->lock);

    uint32_t total_len = sizeof(udp_header_t) + len;
    uint8_t buf[NET_PACKET_MAX];
    udp_header_t* udp = (udp_header_t*)buf;
    udp->src_port = net_htons(sock->local_port);
    udp->dst_port = net_htons(remote_port);
    udp->length = net_htons((uint16_t)total_len);
    udp->checksum = 0;
    memcpy(buf + sizeof(udp_header_t), data, len);

    net_iface_t* iface = net_get_iface(0);
    if (iface && iface->has_ipv4) {
        net_ipv4_send(iface, iface->ipv4.addr, remote_addr, NET_PROTO_UDP, buf, total_len);
    }

    spinlock_release(&sock->lock);
    return (int)len;
}

int net_udp_recv(net_socket_t* sock, void* buf, uint32_t len,
                 uint32_t* from_addr, uint16_t* from_port)
{
    if (!sock || !sock->used || !buf) return -1;

    spinlock_acquire(&sock->lock);
    if (sock->recv_data_len == 0) {
        spinlock_release(&sock->lock);
        return 0;
    }

    uint32_t to_copy = len;
    if (to_copy > sock->recv_data_len) to_copy = sock->recv_data_len;
    memcpy(buf, (uint8_t*)sock->recv_buffer + sock->recv_buf_pos, to_copy);
    sock->recv_buf_pos += to_copy;
    sock->recv_data_len -= to_copy;
    if (from_addr) *from_addr = sock->remote_addr.addr;
    if (from_port) *from_port = sock->remote_port;
    spinlock_release(&sock->lock);
    return (int)to_copy;
}

int net_udp_bind(net_socket_t* sock, uint16_t port)
{
    if (!sock || !sock->used) return -1;
    spinlock_acquire(&sock->lock);
    sock->local_port = port;
    spinlock_release(&sock->lock);
    return 0;
}

int net_udp_close(net_socket_t* sock)
{
    if (!sock || !sock->used) return -1;
    sock->state = TCP_STATE_CLOSED;
    return 0;
}

int net_receive_packet(net_iface_t* iface, const void* data, uint32_t len)
{
    if (!iface || !data || len < 14) return -1;

    const uint8_t* frame = (const uint8_t*)data;
    uint16_t ethertype = (uint16_t)((frame[12] << 8) | frame[13]);
    const uint8_t* payload = frame + 14;
    uint32_t payload_len = len - 14;

    if (ethertype == NET_PROTO_IPV4) {
        if (payload_len < sizeof(ipv4_header_t)) return -2;
        const ipv4_header_t* ip = (const ipv4_header_t*)payload;
        uint32_t ip_hdr_len = (uint32_t)(ip->version_ihl & 0x0F) * 4;
        const uint8_t* ip_payload = payload + ip_hdr_len;
        uint32_t ip_payload_len = payload_len - ip_hdr_len;

        if (ip->protocol == NET_PROTO_ICMP) {
            if (ip_payload_len < sizeof(icmp_header_t)) return -3;
            const icmp_header_t* icmp = (const icmp_header_t*)ip_payload;
            if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
                uint8_t reply[NET_PACKET_MAX];
                memcpy(reply, ip_payload, ip_payload_len);
                ((icmp_header_t*)reply)->type = ICMP_TYPE_ECHO_REPLY;
                ((icmp_header_t*)reply)->checksum = 0;
                ((icmp_header_t*)reply)->checksum = net_checksum(reply, ip_payload_len);
                net_ipv4_send(iface, iface->ipv4.addr, ip->src_addr, NET_PROTO_ICMP,
                              reply, ip_payload_len);
            }
        } else if (ip->protocol == NET_PROTO_TCP) {
            if (ip_payload_len < sizeof(tcp_header_t)) return -3;
            const tcp_header_t* tcp = (const tcp_header_t*)ip_payload;
            uint16_t dst_port = net_htons(tcp->dst_port);
            uint16_t src_port = net_htons(tcp->src_port);

            for (int i = 0; i < NET_SOCKET_MAX; i++) {
                net_socket_t* sock = &net_sockets[i];
                if (!sock->used || sock->local_port != dst_port) continue;
                if (sock->type != 1) continue;

                spinlock_acquire(&sock->lock);
                if (tcp->flags & TCP_FLAG_SYN && sock->state == TCP_STATE_LISTEN) {
                    sock->remote_addr.addr = ip->src_addr;
                    sock->remote_port = src_port;
                    sock->ack_num = net_htonl(tcp->seq_num) + 1;
                    sock->state = TCP_STATE_SYN_RECV;
                } else if (tcp->flags & TCP_FLAG_ACK) {
                    if (sock->state == TCP_STATE_SYN_SENT) {
                        sock->state = TCP_STATE_ESTABLISHED;
                        sock->ack_num = net_htonl(tcp->seq_num);
                    } else if (sock->state == TCP_STATE_ESTABLISHED) {
                        uint32_t tcp_hdr_len = (uint32_t)(tcp->data_offset >> 4) * 4;
                        uint32_t data_len = ip_payload_len - tcp_hdr_len;
                        if (data_len > 0 && sock->recv_data_len + data_len <= sock->recv_buf_size) {
                            memcpy((uint8_t*)sock->recv_buffer + sock->recv_buf_pos + sock->recv_data_len,
                                   ip_payload + tcp_hdr_len, data_len);
                            sock->recv_data_len += data_len;
                            sock->ack_num = net_htonl(tcp->seq_num) + data_len;
                        }
                    }
                } else if (tcp->flags & TCP_FLAG_FIN) {
                    sock->state = TCP_STATE_CLOSE_WAIT;
                    sock->ack_num = net_htonl(tcp->seq_num) + 1;
                }
                spinlock_release(&sock->lock);
                break;
            }
        } else if (ip->protocol == NET_PROTO_UDP) {
            if (ip_payload_len < sizeof(udp_header_t)) return -3;
            const udp_header_t* udp = (const udp_header_t*)ip_payload;
            uint16_t dst_port = net_htons(udp->dst_port);
            uint16_t src_port = net_htons(udp->src_port);

            for (int i = 0; i < NET_SOCKET_MAX; i++) {
                net_socket_t* sock = &net_sockets[i];
                if (!sock->used || sock->local_port != dst_port) continue;
                if (sock->type != 2) continue;

                spinlock_acquire(&sock->lock);
                uint32_t udp_data_len = ip_payload_len - sizeof(udp_header_t);
                if (udp_data_len > 0 && sock->recv_data_len + udp_data_len <= sock->recv_buf_size) {
                    memcpy((uint8_t*)sock->recv_buffer + sock->recv_buf_pos + sock->recv_data_len,
                           ip_payload + sizeof(udp_header_t), udp_data_len);
                    sock->recv_data_len += udp_data_len;
                    sock->remote_addr.addr = ip->src_addr;
                    sock->remote_port = src_port;
                }
                spinlock_release(&sock->lock);
                break;
            }
        }
    } else if (ethertype == NET_PROTO_IPV6) {
        if (payload_len < 40) return -2;
        const ipv6_header_t* ip6 = (const ipv6_header_t*)payload;
        const uint8_t* ip6_payload = payload + 40;
        uint32_t ip6_payload_len = net_htons(ip6->payload_length);

        if (ip6->next_header == NET_PROTO_ICMPV6) {
            if (ip6_payload_len < sizeof(icmp_header_t)) return -3;
            const icmp_header_t* icmp = (const icmp_header_t*)ip6_payload;
            if (icmp->type == ICMPV6_TYPE_ECHO_REQUEST) {
                uint8_t reply[NET_PACKET_MAX];
                memcpy(reply, ip6_payload, ip6_payload_len);
                ((icmp_header_t*)reply)->type = ICMPV6_TYPE_ECHO_REPLY;
                ((icmp_header_t*)reply)->checksum = 0;
                net_ipv6_send(iface, iface->ipv6.addr, ip6->src_addr,
                              NET_PROTO_ICMPV6, reply, ip6_payload_len);
            }
        }
    }

    return 0;
}