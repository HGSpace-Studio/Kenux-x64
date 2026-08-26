#ifndef ARCH_DNS_H
#define ARCH_DNS_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define DNS_PORT              53
#define DNS_MAX_NAME_LEN      253
#define DNS_MAX_LABEL_LEN     63
#define DNS_MAX_QUERY_SIZE    512
#define DNS_MAX_RESPONSE_SIZE 4096
#define DNS_HEADER_SIZE       12
#define DNS_MAX_LABELS        128
#define DNS_MAX_RR            64

#define DNS_TYPE_A            1
#define DNS_TYPE_AAAA         28
#define DNS_TYPE_CNAME        5
#define DNS_TYPE_MX           15
#define DNS_TYPE_NS           2
#define DNS_TYPE_SOA          6
#define DNS_TYPE_PTR          12
#define DNS_TYPE_TXT          16

#define DNS_CLASS_IN          1
#define DNS_CLASS_CS          2
#define DNS_CLASS_CH          3
#define DNS_CLASS_HS          4

#define DNS_QR_QUERY         0
#define DNS_QR_RESPONSE       1
#define DNS_OPCODE_QUERY      0
#define DNS_RCODE_OK          0
#define DNS_RCODE_FORMERR     1
#define DNS_RCODE_SERVFAIL    2
#define DNS_RCODE_NXDOMAIN    3
#define DNS_RCODE_NOTIMP      4
#define DNS_RCODE_REFUSED     5

#define DNS_FLAG_RD           0x0100
#define DNS_FLAG_TC           0x0200
#define DNS_FLAG_AA           0x0400
#define DNS_FLAG_CD           0x0800

#define DNS_CACHE_MAX         256
#define DNS_CACHE_TTL_MIN     60
#define DNS_CACHE_TTL_MAX     86400
#define DNS_SERVER_MAX        4

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

typedef struct {
    char qname[DNS_MAX_NAME_LEN + 1];
    uint16_t qtype;
    uint16_t qclass;
} dns_question_t;

typedef struct {
    char name[DNS_MAX_NAME_LEN + 1];
    uint16_t type;
    uint16_t class_;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t  rdata[256];
} dns_rr_t;

typedef struct {
    char hostname[DNS_MAX_NAME_LEN + 1];
    uint32_t ip_addr;
    uint32_t ttl;
    uint64_t expire_time;
    uint8_t  valid;
    struct dns_cache_entry* next;
} dns_cache_entry_t;

typedef struct {
    dns_cache_entry_t entries[DNS_CACHE_MAX];
    uint32_t count;
    spinlock_t lock;
} dns_cache_t;

void dns_init(void);

int dns_build_query(uint16_t id, const char* hostname, uint16_t qtype,
                    uint8_t* buf, uint16_t buf_size);

int dns_parse_response(const uint8_t* buf, uint16_t len,
                       uint32_t* ip_out, uint32_t* ttl_out);

int dns_resolve(const char* hostname, uint32_t* ip_out);

void dns_cache_init(void);
int dns_cache_lookup(const char* hostname, uint32_t* ip_out);
void dns_cache_add(const char* hostname, uint32_t ip, uint32_t ttl);
void dns_cache_flush(void);
void dns_cache_cleanup(void);
uint32_t dns_cache_count(void);

void dns_set_server(uint32_t server_ip, uint32_t index);
uint32_t dns_get_server(uint32_t index);

#endif
