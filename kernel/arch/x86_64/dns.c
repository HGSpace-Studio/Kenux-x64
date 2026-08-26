#include <arch/dns.h>
#include <arch/net.h>
#include <arch/slab.h>
#include <string.h>

static dns_cache_t dns_cache;
static uint32_t dns_servers[DNS_SERVER_MAX];
static uint16_t dns_query_id = 0;
static spinlock_t dns_lock = SPINLOCK_INIT;

void dns_init(void)
{
    spin_init(&dns_lock);
    spin_init(&dns_cache.lock);
    dns_cache.count = 0;
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        dns_cache.entries[i].valid = 0;
        dns_cache.entries[i].next = NULL;
    }
    for (uint32_t i = 0; i < DNS_SERVER_MAX; i++) {
        dns_servers[i] = 0;
    }
    dns_set_server(0x08080808, 0);
}

void dns_cache_init(void)
{
    spin_init(&dns_cache.lock);
    dns_cache.count = 0;
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        dns_cache.entries[i].valid = 0;
        dns_cache.entries[i].next = NULL;
    }
}

static uint16_t dns_htons(uint16_t v) { return (v >> 8) | (v << 8); }
static uint16_t dns_ntohs(uint16_t v) { return dns_htons(v); }
static uint32_t dns_htonl(uint32_t v) {
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
           ((v & 0xff0000) >> 8) | ((v >> 24) & 0xff);
}
static uint32_t dns_ntohl(uint32_t v) { return dns_htonl(v); }

extern uint64_t timer_get_jiffies(void);
extern void msleep(uint64_t ms);

static uint64_t dns_get_time(void)
{
    return timer_get_jiffies() / 1000;
}

static uint16_t dns_next_id(void)
{
    dns_query_id++;
    if (dns_query_id == 0)
        dns_query_id = 1;
    return dns_query_id;
}

static int dns_encode_name(const char* hostname, uint8_t* buf, uint16_t buf_size)
{
    uint16_t pos = 0;
    const char* label = hostname;
    while (*label) {
        const char* dot = label;
        while (*dot && *dot != '.') dot++;
        uint8_t len = (uint8_t)(dot - label);
        if (len == 0) break;
        if (pos + 1 + len > buf_size) return -1;
        buf[pos++] = len;
        for (uint8_t i = 0; i < len; i++) {
            buf[pos++] = (uint8_t)label[i];
        }
        if (*dot == '.') {
            label = dot + 1;
        } else {
            break;
        }
    }
    if (pos + 1 > buf_size) return -1;
    buf[pos++] = 0;
    return (int)pos;
}

static int dns_decode_name(const uint8_t* buf, uint16_t buf_size,
                           uint16_t offset, char* name, uint16_t name_size)
{
    uint16_t pos = offset;
    uint16_t name_pos = 0;
    uint16_t jumped = 0;
    uint16_t saved_offset = 0;
    while (pos < buf_size) {
        uint8_t len = buf[pos];
        if (len == 0) {
            if (name_pos > 0 && name_pos < name_size) {
                name[name_pos - 1] = 0;
            } else {
                name[0] = 0;
            }
            break;
        }
        if ((len & 0xc0) == 0xc0) {
            if (pos + 1 >= buf_size) return -1;
            if (!jumped) saved_offset = pos + 2;
            jumped = 1;
            pos = ((uint16_t)(len & 0x3f) << 8) | buf[pos + 1];
            continue;
        }
        pos++;
        if (pos + len > buf_size) return -1;
        if (name_pos + len + 1 >= name_size) return -1;
        for (uint8_t i = 0; i < len; i++) {
            name[name_pos++] = (char)buf[pos++];
        }
        name[name_pos++] = '.';
    }
    if (jumped) {
        return (int)saved_offset;
    }
    return (int)(pos + 1);
}

int dns_build_query(uint16_t id, const char* hostname, uint16_t qtype,
                    uint8_t* buf, uint16_t buf_size)
{
    if (!hostname || !buf || buf_size < DNS_HEADER_SIZE + 5)
        return -1;
    memset(buf, 0, buf_size);
    dns_header_t* hdr = (dns_header_t*)buf;
    hdr->id = dns_htons(id);
    uint16_t flags = DNS_QR_QUERY;
    flags |= DNS_FLAG_RD;
    hdr->flags = dns_htons(flags);
    hdr->qdcount = dns_htons(1);
    uint16_t pos = DNS_HEADER_SIZE;
    int name_len = dns_encode_name(hostname, buf + pos, buf_size - pos);
    if (name_len < 0) return -1;
    pos += (uint16_t)name_len;
    if (pos + 4 > buf_size) return -1;
    buf[pos] = (uint8_t)(qtype >> 8);
    buf[pos + 1] = (uint8_t)(qtype & 0xff);
    buf[pos + 2] = (uint8_t)(DNS_CLASS_IN >> 8);
    buf[pos + 3] = (uint8_t)(DNS_CLASS_IN & 0xff);
    pos += 4;
    return (int)pos;
}

int dns_parse_response(const uint8_t* buf, uint16_t len,
                       uint32_t* ip_out, uint32_t* ttl_out)
{
    if (!buf || len < DNS_HEADER_SIZE)
        return -1;
    const dns_header_t* hdr = (const dns_header_t*)buf;
    uint16_t flags = dns_ntohs(hdr->flags);
    uint8_t rcode = (uint8_t)(flags & 0xf);
    if (rcode != DNS_RCODE_OK)
        return -1;
    uint16_t qdcount = dns_ntohs(hdr->qdcount);
    uint16_t ancount = dns_ntohs(hdr->ancount);
    uint16_t pos = DNS_HEADER_SIZE;
    for (uint16_t i = 0; i < qdcount && pos < len; i++) {
        char name[DNS_MAX_NAME_LEN + 1];
        int ret = dns_decode_name(buf, len, pos, name, sizeof(name));
        if (ret < 0) return -1;
        pos = (uint16_t)ret;
        if (pos + 4 > len) return -1;
        pos += 4;
    }
    for (uint16_t i = 0; i < ancount && pos < len; i++) {
        char rr_name[DNS_MAX_NAME_LEN + 1];
        int ret = dns_decode_name(buf, len, pos, rr_name, sizeof(rr_name));
        if (ret < 0) return -1;
        pos = (uint16_t)ret;
        if (pos + 10 > len) return -1;
        uint16_t rr_type = ((uint16_t)buf[pos] << 8) | (uint16_t)buf[pos + 1];
        pos += 2;
        uint16_t rr_class = ((uint16_t)buf[pos] << 8) | (uint16_t)buf[pos + 1];
        pos += 2;
        uint32_t rr_ttl = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos+1] << 16) |
                          ((uint32_t)buf[pos+2] << 8) | (uint32_t)buf[pos+3];
        pos += 4;
        uint16_t rdlen = ((uint16_t)buf[pos] << 8) | (uint16_t)buf[pos + 1];
        pos += 2;
        (void)rr_class;
        if (rr_type == DNS_TYPE_A && rdlen == 4 && pos + 4 <= len) {
            uint32_t ip = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos+1] << 16) |
                          ((uint32_t)buf[pos+2] << 8) | (uint32_t)buf[pos+3];
            if (ip_out) *ip_out = ip;
            if (ttl_out) *ttl_out = rr_ttl;
            return 0;
        }
        if (rr_type == DNS_TYPE_CNAME && rdlen > 0 && pos + rdlen <= len) {
            char cname[DNS_MAX_NAME_LEN + 1];
            dns_decode_name(buf, len, pos, cname, sizeof(cname));
        }
        pos += rdlen;
    }
    return -1;
}

int dns_resolve(const char* hostname, uint32_t* ip_out)
{
    if (!hostname || !ip_out) return -1;
    uint32_t cached_ip = 0;
    if (dns_cache_lookup(hostname, &cached_ip) == 0) {
        *ip_out = cached_ip;
        return 0;
    }
    uint32_t server = dns_servers[0];
    if (server == 0) return -1;
    uint8_t query_buf[DNS_MAX_QUERY_SIZE];
    uint8_t resp_buf[DNS_MAX_RESPONSE_SIZE];
    uint16_t id = dns_next_id();
    int qlen = dns_build_query(id, hostname, DNS_TYPE_A, query_buf, sizeof(query_buf));
    if (qlen < 0) return -1;

    int fd = sys_socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    sockaddr_in_t local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = dns_htons(10000 + (id % 1000));
    if (sys_bind(fd, &local_addr, sizeof(local_addr)) < 0) {
        sys_close_socket(fd);
        return -1;
    }

    sockaddr_in_t server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = dns_htons(DNS_PORT);
    server_addr.sin_addr = dns_htonl(server);

    if (sys_sendto(fd, query_buf, (uint16_t)qlen, 0, &server_addr, sizeof(server_addr)) < 0) {
        sys_close_socket(fd);
        return -1;
    }

    memset(resp_buf, 0, sizeof(resp_buf));
    uint32_t result_ip = 0;
    uint32_t result_ttl = 0;
    int rlen = -1;

    for (int retry = 0; retry < 50; retry++) {
        rlen = sys_recvfrom(fd, resp_buf, sizeof(resp_buf), 0, NULL, NULL);
        if (rlen > 0) break;
        msleep(10);
    }
    sys_close_socket(fd);

    if (rlen > 0 && dns_parse_response(resp_buf, (uint16_t)rlen, &result_ip, &result_ttl) == 0) {
        if (result_ip != 0) {
            dns_cache_add(hostname, result_ip, result_ttl);
            *ip_out = result_ip;
            return 0;
        }
    }
    return -1;
}

int dns_cache_lookup(const char* hostname, uint32_t* ip_out)
{
    if (!hostname || !ip_out) return -1;
    uint64_t now = dns_get_time();
    spin_lock(&dns_cache.lock);
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        if (!dns_cache.entries[i].valid) continue;
        if (strcmp(dns_cache.entries[i].hostname, hostname) == 0) {
            if (now >= dns_cache.entries[i].expire_time) {
                dns_cache.entries[i].valid = 0;
                dns_cache.count--;
                spin_unlock(&dns_cache.lock);
                return -1;
            }
            *ip_out = dns_cache.entries[i].ip_addr;
            spin_unlock(&dns_cache.lock);
            return 0;
        }
    }
    spin_unlock(&dns_cache.lock);
    return -1;
}

void dns_cache_add(const char* hostname, uint32_t ip, uint32_t ttl)
{
    if (!hostname) return;
    uint32_t effective_ttl = ttl;
    if (effective_ttl < DNS_CACHE_TTL_MIN) effective_ttl = DNS_CACHE_TTL_MIN;
    if (effective_ttl > DNS_CACHE_TTL_MAX) effective_ttl = DNS_CACHE_TTL_MAX;
    spin_lock(&dns_cache.lock);
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        if (dns_cache.entries[i].valid &&
            strcmp(dns_cache.entries[i].hostname, hostname) == 0) {
            dns_cache.entries[i].ip_addr = ip;
            dns_cache.entries[i].ttl = effective_ttl;
            dns_cache.entries[i].expire_time = dns_get_time() + effective_ttl;
            spin_unlock(&dns_cache.lock);
            return;
        }
    }
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        if (!dns_cache.entries[i].valid) {
            strncpy(dns_cache.entries[i].hostname, hostname, DNS_MAX_NAME_LEN);
            dns_cache.entries[i].hostname[DNS_MAX_NAME_LEN] = 0;
            dns_cache.entries[i].ip_addr = ip;
            dns_cache.entries[i].ttl = effective_ttl;
            dns_cache.entries[i].expire_time = dns_get_time() + effective_ttl;
            dns_cache.entries[i].valid = 1;
            dns_cache.entries[i].next = NULL;
            dns_cache.count++;
            spin_unlock(&dns_cache.lock);
            return;
        }
    }
    uint64_t oldest_expire = 0xffffffffffffffffULL;
    uint32_t oldest_idx = 0;
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        if (dns_cache.entries[i].valid &&
            dns_cache.entries[i].expire_time < oldest_expire) {
            oldest_expire = dns_cache.entries[i].expire_time;
            oldest_idx = i;
        }
    }
    strncpy(dns_cache.entries[oldest_idx].hostname, hostname, DNS_MAX_NAME_LEN);
    dns_cache.entries[oldest_idx].hostname[DNS_MAX_NAME_LEN] = 0;
    dns_cache.entries[oldest_idx].ip_addr = ip;
    dns_cache.entries[oldest_idx].ttl = effective_ttl;
    dns_cache.entries[oldest_idx].expire_time = dns_get_time() + effective_ttl;
    dns_cache.entries[oldest_idx].valid = 1;
    spin_unlock(&dns_cache.lock);
}

void dns_cache_flush(void)
{
    spin_lock(&dns_cache.lock);
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        dns_cache.entries[i].valid = 0;
    }
    dns_cache.count = 0;
    spin_unlock(&dns_cache.lock);
}

void dns_cache_cleanup(void)
{
    uint64_t now = dns_get_time();
    spin_lock(&dns_cache.lock);
    for (uint32_t i = 0; i < DNS_CACHE_MAX; i++) {
        if (dns_cache.entries[i].valid && now >= dns_cache.entries[i].expire_time) {
            dns_cache.entries[i].valid = 0;
            dns_cache.count--;
        }
    }
    spin_unlock(&dns_cache.lock);
}

uint32_t dns_cache_count(void)
{
    spin_lock(&dns_cache.lock);
    uint32_t c = dns_cache.count;
    spin_unlock(&dns_cache.lock);
    return c;
}

void dns_set_server(uint32_t server_ip, uint32_t index)
{
    if (index >= DNS_SERVER_MAX) return;
    spin_lock(&dns_lock);
    dns_servers[index] = server_ip;
    spin_unlock(&dns_lock);
}

uint32_t dns_get_server(uint32_t index)
{
    if (index >= DNS_SERVER_MAX) return 0;
    spin_lock(&dns_lock);
    uint32_t ip = dns_servers[index];
    spin_unlock(&dns_lock);
    return ip;
}
