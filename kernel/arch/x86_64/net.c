

#include <arch/net.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <string.h>


static uint8_t  local_mac[ETH_ALEN];
static uint32_t local_ip  = 0;
static uint32_t gateway_ip = 0;
static uint32_t netmask   = 0;
static int      net_configured = 0;
static uint32_t dhcp_xid = 0;
static uint32_t dhcp_server_ip = 0;

#define ARP_CACHE_SIZE  32
typedef struct {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    uint64_t timestamp;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static uint32_t arp_count = 0;

static socket_t sockets[128];
static tcp_socket_t tcp_socks[TCP_MAX_SOCKETS];
static udp_socket_t udp_socks[UDP_MAX_SOCKETS];
static spinlock_t net_lock = SPINLOCK_INIT;

static int (*netdev_tx)(const void* data, uint64_t len) = NULL;
static uint64_t net_rx_frames = 0;
static uint64_t net_tx_frames = 0;
static uint64_t net_rx_bytes = 0;
static uint64_t net_tx_bytes = 0;

static uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
static uint16_t ntohs(uint16_t v) { return htons(v); }
static uint32_t htonl(uint32_t v)
{
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
           ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}
static uint32_t ntohl(uint32_t v) { return htonl(v); }

static void eth_set_mac(uint8_t* dst, const uint8_t* src)
{
    memcpy(dst, src, ETH_ALEN);
}


static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}


static void generate_mac_from_cpuid(uint8_t* mac)
{
    uint32_t eax, ebx, ecx, edx;
    
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));

    
    mac[0] = 0x02;
    mac[1] = (uint8_t)(eax & 0xFF);
    mac[2] = (uint8_t)((eax >> 8) & 0xFF);
    mac[3] = (uint8_t)(ebx & 0xFF);
    mac[4] = (uint8_t)((ebx >> 8) & 0xFF);
    mac[5] = (uint8_t)((ecx ^ edx) & 0xFF);
}


static uint32_t parse_ip_addr(const char* str)
{
    uint32_t ip = 0;
    int octet = 0;
    int shift = 24;
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            octet = octet * 10 + (*str - '0');
        } else if (*str == '.') {
            ip |= (octet << shift);
            octet = 0;
            shift -= 8;
        } else {
            break;
        }
        str++;
    }
    ip |= (octet << shift);
    return ip;
}

static int parse_mac_addr(const char* str, uint8_t* mac)
{
    for (int i = 0; i < ETH_ALEN; i++) {
        unsigned int byte = 0;
        int n = 0;
        while (*str && n < 2) {
            char c = *str;
            if (c >= '0' && c <= '9') byte = byte * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') byte = byte * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') byte = byte * 16 + (c - 'A' + 10);
            else break;
            str++;
            n++;
        }
        mac[i] = (uint8_t)byte;
        if (*str == ':' || *str == '-') str++;
    }
    return 0;
}

void net_config_load(const char* config_data)
{
    if (!config_data) return;

    const char* p = config_data;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "IP=", 3) == 0) {
            p += 3;
            local_ip = parse_ip_addr(p);
        } else if (strncmp(p, "GATEWAY=", 8) == 0) {
            p += 8;
            gateway_ip = parse_ip_addr(p);
        } else if (strncmp(p, "NETMASK=", 8) == 0) {
            p += 8;
            netmask = parse_ip_addr(p);
        } else if (strncmp(p, "MAC=", 4) == 0) {
            p += 4;
            parse_mac_addr(p, local_mac);
        }
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    net_configured = 1;
}


typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic_cookie;
    uint8_t  options[312];
} dhcp_packet_t;

#define DHCP_MAGIC_COOKIE 0x63825363
#define DHCP_OP_REQUEST   1
#define DHCP_OP_REPLY     2
#define DHCP_TYPE_DISCOVER 1
#define DHCP_TYPE_OFFER    2
#define DHCP_TYPE_REQUEST  3
#define DHCP_TYPE_ACK      5
#define DHCP_OPT_MESSAGE_TYPE 53
#define DHCP_OPT_REQ_IP      50
#define DHCP_OPT_SERVER_ID   54
#define DHCP_OPT_END        255


static void dhcp_send_discover(void)
{
    uint8_t packet[sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + sizeof(dhcp_packet_t)];
    memset(packet, 0, sizeof(packet));

    eth_header_t* eth = (eth_header_t*)packet;
    ip_header_t* ip = (ip_header_t*)(packet + sizeof(eth_header_t));
    udp_header_t* udp = (udp_header_t*)(packet + sizeof(eth_header_t) + sizeof(ip_header_t));
    dhcp_packet_t* dhcp = (dhcp_packet_t*)(packet + sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t));

    
    memset(eth->dst, 0xFF, ETH_ALEN);
    eth_set_mac(eth->src, local_mac);
    eth->type = htons(ETH_TYPE_IP);

    
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(sizeof(ip_header_t) + sizeof(udp_header_t) + sizeof(dhcp_packet_t));
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = 0; 
    ip->dst_ip = htonl(0xFFFFFFFF); 
    ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

    
    udp->src_port = htons(68);
    udp->dst_port = htons(67);
    udp->len = htons(sizeof(udp_header_t) + sizeof(dhcp_packet_t));
    udp->checksum = 0;

    
    dhcp->op = DHCP_OP_REQUEST;
    dhcp->htype = 1; 
    dhcp->hlen = ETH_ALEN;
    dhcp->hops = 0;
    dhcp_xid = (uint32_t)rdtsc();
    dhcp->xid = htonl(dhcp_xid);
    dhcp->secs = 0;
    dhcp->flags = htons(0x8000); 
    eth_set_mac(dhcp->chaddr, local_mac);
    dhcp->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    
    dhcp->options[0] = DHCP_OPT_MESSAGE_TYPE;
    dhcp->options[1] = 1;
    dhcp->options[2] = DHCP_TYPE_DISCOVER;
    dhcp->options[3] = DHCP_OPT_END;

    net_tx_frame(packet, sizeof(packet));
}


static int dhcp_parse_response(const dhcp_packet_t* dhcp, uint32_t len,
                                 uint32_t* offered_ip_out, uint32_t* server_ip_out)
{
    if (len < sizeof(dhcp_packet_t)) return -1;
    if (dhcp->op != DHCP_OP_REPLY) return -1;
    if (ntohl(dhcp->magic_cookie) != DHCP_MAGIC_COOKIE) return -1;

    uint32_t offered_ip = ntohl(dhcp->yiaddr);
    if (offered_ip == 0) return -1;

    const uint8_t* opts = dhcp->options;
    uint32_t opt_len = len - (sizeof(dhcp_packet_t) - 312);
    int msg_type = -1;
    uint32_t server_id = 0;
    uint32_t i = 0;

    while (i < opt_len) {
        uint8_t opt = opts[i];
        if (opt == DHCP_OPT_END) break;
        if (opt == 0) { i++; continue; }
        if (i + 1 >= opt_len) break;
        uint8_t olen = opts[i + 1];
        if (i + 2 + olen > opt_len) break;
        if (opt == DHCP_OPT_MESSAGE_TYPE && olen == 1) {
            msg_type = opts[i + 2];
        } else if (opt == DHCP_OPT_SERVER_ID && olen == 4) {
            server_id = ((uint32_t)opts[i + 2] << 24) |
                        ((uint32_t)opts[i + 3] << 16) |
                        ((uint32_t)opts[i + 4] << 8) |
                        (uint32_t)opts[i + 5];
        }
        i += 2 + olen;
    }

    if (msg_type < 0) return -1;
    if (offered_ip_out) *offered_ip_out = offered_ip;
    if (server_ip_out) *server_ip_out = server_id;
    return msg_type;
}

static void dhcp_send_request(uint32_t req_ip, uint32_t server_ip)
{
    uint8_t packet[sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + sizeof(dhcp_packet_t)];
    memset(packet, 0, sizeof(packet));

    eth_header_t* eth = (eth_header_t*)packet;
    ip_header_t* ip = (ip_header_t*)(packet + sizeof(eth_header_t));
    udp_header_t* udp = (udp_header_t*)(packet + sizeof(eth_header_t) + sizeof(ip_header_t));
    dhcp_packet_t* dhcp = (dhcp_packet_t*)(packet + sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t));

    memset(eth->dst, 0xFF, ETH_ALEN);
    eth_set_mac(eth->src, local_mac);
    eth->type = htons(ETH_TYPE_IP);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(sizeof(ip_header_t) + sizeof(udp_header_t) + sizeof(dhcp_packet_t));
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = 0;
    ip->dst_ip = htonl(0xFFFFFFFF);
    ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

    udp->src_port = htons(68);
    udp->dst_port = htons(67);
    udp->len = htons(sizeof(udp_header_t) + sizeof(dhcp_packet_t));
    udp->checksum = 0;

    dhcp->op = DHCP_OP_REQUEST;
    dhcp->htype = 1;
    dhcp->hlen = ETH_ALEN;
    dhcp->hops = 0;
    dhcp->xid = htonl(dhcp_xid);
    dhcp->secs = 0;
    dhcp->flags = htons(0x8000);
    eth_set_mac(dhcp->chaddr, local_mac);
    dhcp->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    uint8_t* opt = dhcp->options;
    opt[0] = DHCP_OPT_MESSAGE_TYPE;
    opt[1] = 1;
    opt[2] = DHCP_TYPE_REQUEST;
    opt[3] = DHCP_OPT_REQ_IP;
    opt[4] = 4;
    opt[5] = (uint8_t)(req_ip >> 24);
    opt[6] = (uint8_t)(req_ip >> 16);
    opt[7] = (uint8_t)(req_ip >> 8);
    opt[8] = (uint8_t)(req_ip);
    opt[9] = DHCP_OPT_SERVER_ID;
    opt[10] = 4;
    opt[11] = (uint8_t)(server_ip >> 24);
    opt[12] = (uint8_t)(server_ip >> 16);
    opt[13] = (uint8_t)(server_ip >> 8);
    opt[14] = (uint8_t)(server_ip);
    opt[15] = DHCP_OPT_END;

    net_tx_frame(packet, sizeof(packet));
}

uint16_t ip_checksum(const void* data, uint64_t len)
{
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len) sum += *(const uint8_t*)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

void arp_send_request(uint32_t target_ip)
{
    uint8_t packet[sizeof(eth_header_t) + sizeof(arp_packet_t)];
    eth_header_t* eth = (eth_header_t*)packet;
    arp_packet_t* arp = (arp_packet_t*)(packet + sizeof(eth_header_t));

    memset(eth->dst, 0xFF, ETH_ALEN);
    eth_set_mac(eth->src, local_mac);
    eth->type = htons(ETH_TYPE_ARP);

    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_TYPE_IP);
    arp->hw_len = ETH_ALEN;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    eth_set_mac(arp->sender_mac, local_mac);
    arp->sender_ip = htonl(local_ip);
    memset(arp->target_mac, 0, ETH_ALEN);
    arp->target_ip = htonl(target_ip);

    net_tx_frame(packet, sizeof(packet));
}

void arp_handle_packet(const arp_packet_t* arp, uint64_t len)
{
    (void)len;
    uint16_t op = ntohs(arp->opcode);
    uint32_t sender_ip = ntohl(arp->sender_ip);

    if (op == ARP_OP_REQUEST) {
        uint32_t target_ip = ntohl(arp->target_ip);
        if (target_ip == local_ip) {

            uint8_t packet[sizeof(eth_header_t) + sizeof(arp_packet_t)];
            eth_header_t* eth = (eth_header_t*)packet;
            arp_packet_t* reply = (arp_packet_t*)(packet + sizeof(eth_header_t));

            eth_set_mac(eth->dst, arp->sender_mac);
            eth_set_mac(eth->src, local_mac);
            eth->type = htons(ETH_TYPE_ARP);

            reply->hw_type = htons(1);
            reply->proto_type = htons(ETH_TYPE_IP);
            reply->hw_len = ETH_ALEN;
            reply->proto_len = 4;
            reply->opcode = htons(ARP_OP_REPLY);
            eth_set_mac(reply->sender_mac, local_mac);
            reply->sender_ip = htonl(local_ip);
            eth_set_mac(reply->target_mac, arp->sender_mac);
            reply->target_ip = arp->sender_ip;

            net_tx_frame(packet, sizeof(packet));
        }
    } else if (op == ARP_OP_REPLY) {

        for (uint32_t i = 0; i < arp_count; i++) {
            if (arp_cache[i].ip == sender_ip) {
                eth_set_mac(arp_cache[i].mac, arp->sender_mac);
                return;
            }
        }
        if (arp_count < ARP_CACHE_SIZE) {
            arp_cache[arp_count].ip = sender_ip;
            eth_set_mac(arp_cache[arp_count].mac, arp->sender_mac);
            arp_count++;
        }
    }
}

const uint8_t* arp_lookup(uint32_t ip)
{
    if ((ip & netmask) != (local_ip & netmask)) {
        ip = gateway_ip;
    }
    for (uint32_t i = 0; i < arp_count; i++) {
        if (arp_cache[i].ip == ip) {
            return arp_cache[i].mac;
        }
    }
    arp_send_request(ip);
    return NULL;
}

void ip_send_packet(uint32_t dst_ip, uint8_t proto, const void* data, uint16_t len)
{
    uint16_t total = sizeof(ip_header_t) + len;
    uint8_t* packet = (uint8_t*)memory_alloc(sizeof(eth_header_t) + total);
    if (!packet) return;

    eth_header_t* eth = (eth_header_t*)packet;
    ip_header_t* ip = (ip_header_t*)(packet + sizeof(eth_header_t));

    const uint8_t* dst_mac = arp_lookup(dst_ip);
    if (!dst_mac) {
        memory_free(packet);
        return;
    }

    eth_set_mac(eth->dst, dst_mac);
    eth_set_mac(eth->src, local_mac);
    eth->type = htons(ETH_TYPE_IP);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(total);
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;
    ip->src_ip = htonl(local_ip);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

    memcpy(packet + sizeof(eth_header_t) + sizeof(ip_header_t), data, len);
    net_tx_frame(packet, sizeof(eth_header_t) + total);
    memory_free(packet);
}

void ip_handle_packet(const ip_header_t* ip, uint64_t len)
{
    if (len < sizeof(ip_header_t)) return;
    if ((ip->version_ihl & 0xF0) != 0x40) return;

    uint16_t total_len = ntohs(ip->total_len);
    if (total_len > len) return;

    uint32_t dst = ntohl(ip->dst_ip);
    if (dst != local_ip && dst != 0xFFFFFFFF) return;

    uint8_t proto = ip->protocol;
    const void* payload = (const uint8_t*)ip + ((ip->version_ihl & 0x0F) * 4);
    uint16_t payload_len = total_len - ((ip->version_ihl & 0x0F) * 4);

    if (proto == IP_PROTO_ICMP) {
        icmp_handle_packet(ip, (const icmp_header_t*)payload, payload_len);
    } else if (proto == IP_PROTO_UDP) {
        udp_handle_packet(ip, (const udp_header_t*)payload, payload_len);
    } else if (proto == IP_PROTO_TCP) {
        tcp_handle_packet(ip, (const tcp_header_t*)payload, payload_len);
    }
}

void icmp_send_echo_reply(uint32_t dst_ip, uint16_t id, uint16_t seq,
                          const void* data, uint16_t len)
{
    uint16_t total = sizeof(icmp_header_t) + len;
    uint8_t* packet = (uint8_t*)memory_alloc(total);
    if (!packet) return;

    icmp_header_t* icmp = (icmp_header_t*)packet;
    icmp->type = ICMP_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    memcpy(packet + sizeof(icmp_header_t), data, len);
    icmp->checksum = ip_checksum(icmp, total);

    ip_send_packet(dst_ip, IP_PROTO_ICMP, packet, total);
    memory_free(packet);
}

void icmp_handle_packet(const ip_header_t* ip, const icmp_header_t* icmp, uint64_t len)
{
    if (len < sizeof(icmp_header_t)) return;
    if (icmp->type == ICMP_ECHO) {
        uint16_t id = ntohs(icmp->id);
        uint16_t seq = ntohs(icmp->seq);
        uint16_t data_len = (uint16_t)(len - sizeof(icmp_header_t));
        icmp_send_echo_reply(ntohl(ip->src_ip), id, seq,
                             (const uint8_t*)icmp + sizeof(icmp_header_t), data_len);
    }
}

void udp_send_packet(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                     const void* data, uint16_t len)
{
    uint16_t total = sizeof(udp_header_t) + len;
    uint8_t* packet = (uint8_t*)memory_alloc(total);
    if (!packet) return;

    udp_header_t* udp = (udp_header_t*)packet;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->len = htons(total);
    udp->checksum = 0;
    memcpy(packet + sizeof(udp_header_t), data, len);

    ip_send_packet(dst_ip, IP_PROTO_UDP, packet, total);
    memory_free(packet);
}


static uint32_t tcp_next_seq = 0;

static tcp_socket_t* tcp_find_socket(uint32_t local_ip, uint16_t local_port,
                                     uint32_t remote_ip, uint16_t remote_port)
{
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_socket_t* s = &tcp_socks[i];
        if (s->state == TCP_CLOSED) continue;
        if (s->local_port == local_port) {
            if (s->state == TCP_SYN_RECV || s->state == TCP_ESTABLISHED) {
                if (s->remote_ip == remote_ip && s->remote_port == remote_port) {
                    return s;
                }
            } else if (s->state == TCP_LISTEN) {
                return s;
            }
        }
    }
    return NULL;
}

void tcp_send_packet(tcp_socket_t* sock, uint8_t flags, const void* data, uint16_t len)
{
    uint16_t total = sizeof(tcp_header_t) + len;
    uint8_t* packet = (uint8_t*)memory_alloc(total);
    if (!packet) return;

    tcp_header_t* tcp = (tcp_header_t*)packet;
    tcp->src_port = htons(sock->local_port);
    tcp->dst_port = htons(sock->remote_port);
    tcp->seq = htonl(sock->snd_nxt);
    tcp->ack = htonl(sock->rcv_nxt);
    tcp->data_offset = (5 << 4);
    tcp->flags = flags;
    tcp->window = htons(sock->rcv_wnd);
    tcp->checksum = 0;
    tcp->urgent = 0;

    if (data && len) {
        memcpy(packet + sizeof(tcp_header_t), data, len);
    }

    tcp->checksum = 0;

    ip_send_packet(sock->remote_ip, IP_PROTO_TCP, packet, total);
    memory_free(packet);

    if (flags & TCP_FLAG_SYN) sock->snd_nxt++;
    if (flags & TCP_FLAG_FIN) sock->snd_nxt++;
    sock->snd_nxt += len;
}

void tcp_handle_packet(const ip_header_t* ip, const tcp_header_t* tcp, uint64_t len)
{
    if (len < sizeof(tcp_header_t)) return;

    uint16_t dst_port = ntohs(tcp->dst_port);
    uint16_t src_port = ntohs(tcp->src_port);
    uint32_t src_ip = ntohl(ip->src_ip);
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);
    uint8_t flags = tcp->flags;
    uint16_t data_offset = ((tcp->data_offset >> 4) & 0x0F) * 4;
    uint16_t payload_len = (uint16_t)(len - data_offset);
    const void* payload = (const uint8_t*)tcp + data_offset;

    tcp_socket_t* sock = tcp_find_socket(local_ip, dst_port, src_ip, src_port);

    if (!sock) {

        return;
    }

    spin_lock(&sock->lock);

    switch (sock->state) {
        case TCP_LISTEN:
            if (flags & TCP_FLAG_SYN) {

                for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
                    if (tcp_socks[i].state == TCP_CLOSED) {
                        tcp_socket_t* new_sock = &tcp_socks[i];
                        memset(new_sock, 0, sizeof(tcp_socket_t));
                        new_sock->state = TCP_SYN_RECV;
                        new_sock->local_ip = local_ip;
                        new_sock->local_port = dst_port;
                        new_sock->remote_ip = src_ip;
                        new_sock->remote_port = src_port;
                        new_sock->rcv_nxt = seq + 1;
                        new_sock->snd_nxt = tcp_next_seq++;
                        new_sock->snd_una = new_sock->snd_nxt;
                        new_sock->rcv_wnd = TCP_RX_BUF_SIZE;
                        new_sock->snd_wnd = ntohs(tcp->window);
                        spin_init(&new_sock->lock);
                        tcp_send_packet(new_sock, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                        break;
                    }
                }
            }
            break;

        case TCP_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                if (ack == sock->snd_nxt) {
                    sock->state = TCP_ESTABLISHED;
                    sock->rcv_nxt = seq + 1;
                    tcp_send_packet(sock, TCP_FLAG_ACK, NULL, 0);
                }
            }
            break;

        case TCP_SYN_RECV:
            if (flags & TCP_FLAG_ACK) {
                sock->state = TCP_ESTABLISHED;
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_FLAG_FIN) {
                sock->rcv_nxt = seq + 1;
                tcp_send_packet(sock, TCP_FLAG_ACK, NULL, 0);
                sock->state = TCP_CLOSE_WAIT;
            } else if (payload_len > 0) {

                uint32_t avail = TCP_RX_BUF_SIZE - (sock->rx_tail - sock->rx_head);
                if (avail > payload_len) avail = payload_len;
                for (uint32_t i = 0; i < avail; i++) {
                    uint32_t idx = (sock->rx_tail + i) % TCP_RX_BUF_SIZE;
                    sock->rx_buf[idx] = ((const uint8_t*)payload)[i];
                }
                sock->rx_tail += avail;
                sock->rcv_nxt = seq + payload_len;
                tcp_send_packet(sock, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_CLOSE_WAIT:
            if (flags & TCP_FLAG_FIN) {
                tcp_send_packet(sock, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_LAST_ACK:
            if (flags & TCP_FLAG_ACK) {
                sock->state = TCP_CLOSED;
            }
            break;
    }

    spin_unlock(&sock->lock);
}

int sys_socket(int domain, int type, int protocol)
{
    if (domain != AF_INET) return -1;

    spin_lock(&net_lock);
    int fd = -1;
    for (int i = 0; i < 128; i++) {
        if (sockets[i].domain == 0) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        spin_unlock(&net_lock);
        return -1;
    }

    socket_t* sock = &sockets[fd];
    memset(sock, 0, sizeof(socket_t));
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;

    if (type == SOCK_STREAM) {
        for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
            if (tcp_socks[i].state == TCP_CLOSED) {
                memset(&tcp_socks[i], 0, sizeof(tcp_socket_t));
                tcp_socks[i].state = TCP_CLOSED;
                spin_init(&tcp_socks[i].lock);
                sock->tcp = &tcp_socks[i];
                break;
            }
        }
        if (!sock->tcp) {
            memset(sock, 0, sizeof(socket_t));
            spin_unlock(&net_lock);
            return -1;
        }
    } else if (type == SOCK_DGRAM) {
        for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
            if (!udp_socks[i].bound) {
                memset(&udp_socks[i], 0, sizeof(udp_socket_t));
                spin_init(&udp_socks[i].lock);
                sock->udp = &udp_socks[i];
                break;
            }
        }
        if (!sock->udp) {
            memset(sock, 0, sizeof(socket_t));
            spin_unlock(&net_lock);
            return -1;
        }
    }

    spin_unlock(&net_lock);
    return fd;
}

int sys_bind(int sockfd, const sockaddr_in_t* addr, uint32_t addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (sock->domain == 0) return -1;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        sock->tcp->local_ip = ntohl(addr->sin_addr);
        sock->tcp->local_port = ntohs(addr->sin_port);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        sock->udp->local_ip = ntohl(addr->sin_addr);
        sock->udp->local_port = ntohs(addr->sin_port);
        sock->udp->bound = 1;
    }
    return 0;
}

int sys_listen(int sockfd, int backlog)
{
    (void)backlog;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->tcp) return -1;
    sock->tcp->state = TCP_LISTEN;
    return 0;
}

int sys_accept(int sockfd, sockaddr_in_t* addr, uint32_t* addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* listen_sock = &sockets[sockfd];
    if (!listen_sock->tcp || listen_sock->tcp->state != TCP_LISTEN) return -1;

    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_socks[i].state == TCP_SYN_RECV) {

            int new_fd = -1;
            spin_lock(&net_lock);
            for (int j = 0; j < 128; j++) {
                if (sockets[j].domain == 0) {
                    new_fd = j;
                    break;
                }
            }
            if (new_fd < 0) {
                spin_unlock(&net_lock);
                return -1;
            }

            memset(&sockets[new_fd], 0, sizeof(socket_t));
            sockets[new_fd].domain = AF_INET;
            sockets[new_fd].type = SOCK_STREAM;
            sockets[new_fd].tcp = &tcp_socks[i];
            spin_unlock(&net_lock);

            tcp_socks[i].state = TCP_ESTABLISHED;

            if (addr) {
                addr->sin_family = AF_INET;
                addr->sin_port = htons(tcp_socks[i].remote_port);
                addr->sin_addr = htonl(tcp_socks[i].remote_ip);
            }
            return new_fd;
        }
    }
    return -1;
}

int sys_connect(int sockfd, const sockaddr_in_t* addr, uint32_t addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->tcp) return -1;

    tcp_socket_t* tcp = sock->tcp;
    tcp->remote_ip = ntohl(addr->sin_addr);
    tcp->remote_port = ntohs(addr->sin_port);
    tcp->local_ip = local_ip;
    if (tcp->local_port == 0) {
        tcp->local_port = (uint16_t)(0xC000 + (tcp_next_seq % 0x4000));
    }
    tcp->snd_nxt = tcp_next_seq++;
    tcp->snd_una = tcp->snd_nxt;
    tcp->rcv_wnd = TCP_RX_BUF_SIZE;
    tcp->state = TCP_SYN_SENT;

    tcp_send_packet(tcp, TCP_FLAG_SYN, NULL, 0);
    return 0;
}

int sys_send(int sockfd, const void* buf, uint64_t len, int flags)
{
    (void)flags;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->tcp || sock->tcp->state != TCP_ESTABLISHED) return -1;

    tcp_socket_t* tcp = sock->tcp;
    uint64_t sent = 0;
    while (sent < len) {
        uint16_t chunk = (len - sent > TCP_MSS) ? TCP_MSS : (uint16_t)(len - sent);
        tcp_send_packet(tcp, TCP_FLAG_ACK | TCP_FLAG_PSH,
                        (const uint8_t*)buf + sent, chunk);
        sent += chunk;
    }
    return (int)sent;
}

int sys_recv(int sockfd, void* buf, uint64_t len, int flags)
{
    (void)flags;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->tcp) return -1;

    tcp_socket_t* tcp = sock->tcp;
    spin_lock(&tcp->lock);

    uint32_t avail = tcp->rx_tail - tcp->rx_head;
    if (avail > len) avail = (uint32_t)len;
    for (uint32_t i = 0; i < avail; i++) {
        ((uint8_t*)buf)[i] = tcp->rx_buf[(tcp->rx_head + i) % TCP_RX_BUF_SIZE];
    }
    tcp->rx_head += avail;

    spin_unlock(&tcp->lock);
    return (int)avail;
}

int sys_sendto(int sockfd, const void* buf, uint64_t len, int flags,
               const sockaddr_in_t* dst_addr, uint32_t addrlen)
{
    (void)flags; (void)addrlen;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->udp) return -1;

    udp_send_packet(ntohl(dst_addr->sin_addr),
                    sock->udp->local_port,
                    ntohs(dst_addr->sin_port),
                    buf, (uint16_t)len);
    return (int)len;
}


void udp_handle_packet(const ip_header_t* ip, const udp_header_t* udp, uint64_t len)
{
    if (len < sizeof(udp_header_t)) return;
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t src_port = ntohs(udp->src_port);
    uint32_t src_ip = ntohl(ip->src_ip);
    uint16_t data_len = ntohs(udp->len) - sizeof(udp_header_t);
    const void* data = (const uint8_t*)udp + sizeof(udp_header_t);

    
    if (src_port == 67 && dst_port == 68 && !net_configured) {
        uint32_t offered_ip = 0;
        uint32_t server_ip = 0;
        int msg_type = dhcp_parse_response((const dhcp_packet_t*)data, data_len, &offered_ip, &server_ip);
        if (msg_type == DHCP_TYPE_OFFER) {
            dhcp_server_ip = server_ip;
            dhcp_send_request(offered_ip, server_ip);
        } else if (msg_type == DHCP_TYPE_ACK) {
            local_ip = offered_ip;
            net_configured = 1;
        }
        return;
    }

    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_socks[i].bound && udp_socks[i].local_port == dst_port) {
            udp_socket_t* sock = &udp_socks[i];
            spin_lock(&sock->lock);

            uint32_t avail = UDP_RX_BUF_SIZE - (sock->rx_tail - sock->rx_head);
            if (avail > data_len) avail = data_len;
            for (uint32_t j = 0; j < avail; j++) {
                uint32_t idx = (sock->rx_tail + j) % UDP_RX_BUF_SIZE;
                sock->rx_buf[idx] = ((const uint8_t*)data)[j];
            }
            sock->rx_tail += avail;
            sock->peer_ip = src_ip;
            sock->peer_port = src_port;
            sock->has_data = 1;

            
            if (sock->waiter_pid != 0) {
                process_wakeup(sock->waiter_pid);
                sock->waiter_pid = 0;
            }

            spin_unlock(&sock->lock);
            return;
        }
    }
}


int sys_recvfrom(int sockfd, void* buf, uint64_t len, int flags,
                 sockaddr_in_t* src_addr, uint32_t* addrlen)
{
    (void)flags;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->udp) return -1;

    udp_socket_t* udp = sock->udp;
    spin_lock(&udp->lock);

    uint32_t avail = udp->rx_tail - udp->rx_head;
    if (avail > len) avail = (uint32_t)len;
    for (uint32_t i = 0; i < avail; i++) {
        ((uint8_t*)buf)[i] = udp->rx_buf[(udp->rx_head + i) % UDP_RX_BUF_SIZE];
    }
    udp->rx_head += avail;
    if (udp->rx_head == udp->rx_tail) {
        udp->has_data = 0;
    }

    
    if (src_addr) {
        src_addr->sin_family = AF_INET;
        src_addr->sin_port = htons(udp->peer_port);
        src_addr->sin_addr = htonl(udp->peer_ip);
    }
    if (addrlen) {
        *addrlen = sizeof(sockaddr_in_t);
    }

    spin_unlock(&udp->lock);
    return (int)avail;
}

int sys_shutdown(int sockfd, int how)
{
    (void)how;
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];
    if (!sock->tcp) return -1;

    if (sock->tcp->state == TCP_ESTABLISHED) {
        sock->tcp->state = TCP_FIN_WAIT1;
        tcp_send_packet(sock->tcp, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
    }
    return 0;
}

int sys_close_socket(int sockfd)
{
    if (sockfd < 0 || sockfd >= 128) return -1;
    socket_t* sock = &sockets[sockfd];

    if (sock->tcp) {
        if (sock->tcp->state == TCP_ESTABLISHED || sock->tcp->state == TCP_CLOSE_WAIT) {
            sys_shutdown(sockfd, 0);
        }
        sock->tcp->state = TCP_CLOSED;
        sock->tcp = NULL;
    }
    if (sock->udp) {
        sock->udp->bound = 0;
        sock->udp = NULL;
    }
    memset(sock, 0, sizeof(socket_t));
    return 0;
}

void net_rx_frame(const void* data, uint64_t len)
{
    if (len < sizeof(eth_header_t)) return;
    net_rx_frames++;
    net_rx_bytes += len;
    const eth_header_t* eth = (const eth_header_t*)data;
    uint16_t type = ntohs(eth->type);

    if (type == ETH_TYPE_ARP) {
        arp_handle_packet((const arp_packet_t*)((const uint8_t*)data + sizeof(eth_header_t)),
                          len - sizeof(eth_header_t));
    } else if (type == ETH_TYPE_IP) {
        ip_handle_packet((const ip_header_t*)((const uint8_t*)data + sizeof(eth_header_t)),
                         len - sizeof(eth_header_t));
    }
}

int net_tx_frame(const void* data, uint64_t len)
{
    if (netdev_tx) {
        int ret = netdev_tx(data, len);
        if (ret >= 0) {
            net_tx_frames++;
            net_tx_bytes += len;
        }
        return ret;
    }
    return -1;
}

void net_register_tx(int (*tx_func)(const void*, uint64_t))
{
    netdev_tx = tx_func;
}

void net_get_status(net_status_t* status)
{
    if (!status) return;
    memset(status, 0, sizeof(*status));
    memcpy(status->mac, local_mac, ETH_ALEN);
    status->local_ip = local_ip;
    status->gateway_ip = gateway_ip;
    status->netmask = netmask;
    status->dhcp_server_ip = dhcp_server_ip;
    status->configured = (uint32_t)net_configured;
    status->tx_registered = (netdev_tx != NULL) ? 1U : 0U;
    status->rx_frames = net_rx_frames;
    status->tx_frames = net_tx_frames;
    status->rx_bytes = net_rx_bytes;
    status->tx_bytes = net_tx_bytes;
}

void net_init(void)
{
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(sockets, 0, sizeof(sockets));
    memset(tcp_socks, 0, sizeof(tcp_socks));
    memset(udp_socks, 0, sizeof(udp_socks));
    arp_count = 0;
    net_rx_frames = 0;
    net_tx_frames = 0;
    net_rx_bytes = 0;
    net_tx_bytes = 0;
    spin_init(&net_lock);

    
    tcp_next_seq = (uint32_t)rdtsc();

    
    generate_mac_from_cpuid(local_mac);

    
    if (!net_configured) {
        dhcp_send_discover();
    }
}

void network_init(void)
{
    net_init();
}
