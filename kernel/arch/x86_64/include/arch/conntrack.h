#ifndef ARCH_CONNTRACK_H
#define ARCH_CONNTRACK_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/net.h>

#define CONNTRACK_MAX         65536
#define CONNTRACK_BUCKETS     1024
#define CONNTRACK_TIMEOUT     300

#define IPPROTO_TCP           6
#define IPPROTO_UDP           17

#define CONNTRACK_TCP_NONE       0
#define CONNTRACK_TCP_SYN_SENT   1
#define CONNTRACK_TCP_SYN_RECV   2
#define CONNTRACK_TCP_ESTABLISHED 3
#define CONNTRACK_TCP_FIN_WAIT   4
#define CONNTRACK_TCP_CLOSE_WAIT 5
#define CONNTRACK_TCP_LAST_ACK   6
#define CONNTRACK_TCP_TIME_WAIT  7
#define CONNTRACK_TCP_CLOSED     8

#define CONNTRACK_UDP_NONE        0
#define CONNTRACK_UDP_ESTABLISHED 1

#define CONNTRACK_DIR_ORIGINAL    0
#define CONNTRACK_DIR_REPLY       1

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
} nf_conn_tuple_t;

typedef struct nf_conntrack {
    nf_conn_tuple_t tuple;
    nf_conn_tuple_t reply_tuple;
    uint8_t  proto;
    uint8_t  state;
    uint8_t  direction;
    uint8_t  confirmed;
    uint64_t timeout;
    uint64_t last_seen;
    uint32_t mark;
    uint32_t status;
    uint32_t refcnt;
    struct nf_conntrack* next;
    struct nf_conntrack* prev;
    struct nf_conntrack* sibling;
} nf_conntrack_t;

typedef struct {
    nf_conntrack_t* head;
    uint32_t count;
    spinlock_t lock;
} conntrack_bucket_t;

typedef struct {
    conntrack_bucket_t buckets[CONNTRACK_BUCKETS];
    uint32_t total_count;
    uint32_t max_count;
    spinlock_t global_lock;
} conntrack_hash_t;

void conntrack_init(void);
nf_conntrack_t* conntrack_lookup(const nf_conn_tuple_t* tuple);
nf_conntrack_t* conntrack_add(const nf_conn_tuple_t* tuple, uint8_t proto);
int conntrack_update(nf_conntrack_t* ct, uint8_t new_state, uint64_t timeout);
int conntrack_confirm(nf_conntrack_t* ct);
void conntrack_remove(nf_conntrack_t* ct);
void conntrack_cleanup(void);
uint32_t conntrack_count(void);
void conntrack_flush(void);

nf_conntrack_t* conntrack_find_get(const nf_conn_tuple_t* tuple);
void conntrack_put(nf_conntrack_t* ct);

uint32_t conntrack_tuple_hash(const nf_conn_tuple_t* tuple);

#endif
