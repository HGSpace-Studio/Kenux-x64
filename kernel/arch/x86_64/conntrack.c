#include <arch/conntrack.h>
#include <arch/slab.h>
#include <string.h>

static conntrack_hash_t conntrack_hash;
static uint64_t conntrack_jiffies_base = 0;

extern uint64_t timer_get_jiffies(void);

static uint64_t conntrack_get_time(void)
{
    return timer_get_jiffies() / 1000;
}

static uint64_t conntrack_now(void)
{
    return timer_get_jiffies() / 1000;
}

uint32_t conntrack_tuple_hash(const nf_conn_tuple_t* tuple)
{
    if (!tuple) return 0;
    uint32_t h = (uint32_t)(tuple->src_ip * 2654435761ULL);
    h ^= (uint32_t)(tuple->dst_ip * 40503ULL);
    h ^= (uint32_t)((tuple->src_port | ((uint32_t)tuple->dst_port << 16)) * 0x9E3779B9U);
    h ^= (uint32_t)tuple->proto;
    h ^= h >> 16;
    h *= 0x85EBCA6BU;
    h ^= h >> 13;
    h *= 0xC2B2AE35U;
    h ^= h >> 16;
    return h % CONNTRACK_BUCKETS;
}

static void conntrack_make_reply(const nf_conn_tuple_t* orig, nf_conn_tuple_t* reply)
{
    if (!orig || !reply) return;
    reply->src_ip = orig->dst_ip;
    reply->dst_ip = orig->src_ip;
    reply->src_port = orig->dst_port;
    reply->dst_port = orig->src_port;
    reply->proto = orig->proto;
}

void conntrack_init(void)
{
    spin_init(&conntrack_hash.global_lock);
    conntrack_hash.total_count = 0;
    conntrack_hash.max_count = CONNTRACK_MAX;
    for (uint32_t i = 0; i < CONNTRACK_BUCKETS; i++) {
        conntrack_hash.buckets[i].head = NULL;
        conntrack_hash.buckets[i].count = 0;
        spin_init(&conntrack_hash.buckets[i].lock);
    }
}

static int conntrack_tuple_equal(const nf_conn_tuple_t* a, const nf_conn_tuple_t* b)
{
    if (!a || !b) return 0;
    return (a->src_ip == b->src_ip && a->dst_ip == b->dst_ip &&
            a->src_port == b->src_port && a->dst_port == b->dst_port &&
            a->proto == b->proto);
}

nf_conntrack_t* conntrack_lookup(const nf_conn_tuple_t* tuple)
{
    if (!tuple) return NULL;
    uint32_t bucket = conntrack_tuple_hash(tuple);
    conntrack_bucket_t* b = &conntrack_hash.buckets[bucket];
    spin_lock(&b->lock);
    nf_conntrack_t* ct = b->head;
    while (ct) {
        if (conntrack_tuple_equal(&ct->tuple, tuple)) {
            spin_unlock(&b->lock);
            return ct;
        }
        if (conntrack_tuple_equal(&ct->reply_tuple, tuple)) {
            spin_unlock(&b->lock);
            return ct;
        }
        ct = ct->next;
    }
    spin_unlock(&b->lock);
    return NULL;
}

nf_conntrack_t* conntrack_find_get(const nf_conn_tuple_t* tuple)
{
    nf_conntrack_t* ct = conntrack_lookup(tuple);
    if (ct) {
        spin_lock(&conntrack_hash.global_lock);
        ct->refcnt++;
        spin_unlock(&conntrack_hash.global_lock);
    }
    return ct;
}

void conntrack_put(nf_conntrack_t* ct)
{
    if (!ct) return;
    spin_lock(&conntrack_hash.global_lock);
    if (ct->refcnt > 0) ct->refcnt--;
    uint32_t ref = ct->refcnt;
    spin_unlock(&conntrack_hash.global_lock);
    if (ref == 0) {
        conntrack_remove(ct);
    }
}

nf_conntrack_t* conntrack_add(const nf_conn_tuple_t* tuple, uint8_t proto)
{
    if (!tuple) return NULL;
    spin_lock(&conntrack_hash.global_lock);
    if (conntrack_hash.total_count >= conntrack_hash.max_count) {
        spin_unlock(&conntrack_hash.global_lock);
        return NULL;
    }
    nf_conntrack_t* existing = conntrack_lookup(tuple);
    if (existing) {
        spin_unlock(&conntrack_hash.global_lock);
        return existing;
    }
    nf_conntrack_t* ct = (nf_conntrack_t*)kmalloc(sizeof(nf_conntrack_t));
    if (!ct) {
        spin_unlock(&conntrack_hash.global_lock);
        return NULL;
    }
    memset(ct, 0, sizeof(nf_conntrack_t));
    memcpy(&ct->tuple, tuple, sizeof(nf_conn_tuple_t));
    conntrack_make_reply(tuple, &ct->reply_tuple);
    ct->proto = proto;
    ct->confirmed = 0;
    ct->direction = CONNTRACK_DIR_ORIGINAL;
    ct->mark = 0;
    ct->status = 0;
    ct->refcnt = 1;
    ct->sibling = NULL;
    if (proto == IPPROTO_TCP) {
        ct->state = CONNTRACK_TCP_SYN_SENT;
        ct->timeout = conntrack_get_time() + 120;
    } else if (proto == IPPROTO_UDP) {
        ct->state = CONNTRACK_UDP_ESTABLISHED;
        ct->timeout = conntrack_get_time() + CONNTRACK_TIMEOUT;
    } else {
        ct->state = 0;
        ct->timeout = conntrack_get_time() + CONNTRACK_TIMEOUT;
    }
    ct->last_seen = conntrack_get_time();
    uint32_t bucket = conntrack_tuple_hash(tuple);
    conntrack_bucket_t* b = &conntrack_hash.buckets[bucket];
    spin_lock(&b->lock);
    ct->next = b->head;
    ct->prev = NULL;
    if (b->head) {
        b->head->prev = ct;
    }
    b->head = ct;
    b->count++;
    conntrack_hash.total_count++;
    spin_unlock(&b->lock);
    spin_unlock(&conntrack_hash.global_lock);
    return ct;
}

int conntrack_update(nf_conntrack_t* ct, uint8_t new_state, uint64_t timeout)
{
    if (!ct) return -1;
    uint32_t bucket = conntrack_tuple_hash(&ct->tuple);
    conntrack_bucket_t* b = &conntrack_hash.buckets[bucket];
    spin_lock(&b->lock);
    ct->state = new_state;
    ct->last_seen = conntrack_now();
    if (timeout > 0) {
        ct->timeout = timeout;
    } else {
        if (ct->proto == IPPROTO_TCP) {
            switch (new_state) {
            case CONNTRACK_TCP_SYN_SENT:
            case CONNTRACK_TCP_SYN_RECV:
                ct->timeout = ct->last_seen + 120;
                break;
            case CONNTRACK_TCP_ESTABLISHED:
                ct->timeout = ct->last_seen + 432000;
                break;
            case CONNTRACK_TCP_FIN_WAIT:
                ct->timeout = ct->last_seen + 120;
                break;
            case CONNTRACK_TCP_CLOSE_WAIT:
            case CONNTRACK_TCP_LAST_ACK:
                ct->timeout = ct->last_seen + 30;
                break;
            case CONNTRACK_TCP_TIME_WAIT:
                ct->timeout = ct->last_seen + 120;
                break;
            case CONNTRACK_TCP_CLOSED:
                ct->timeout = ct->last_seen + 10;
                break;
            default:
                ct->timeout = ct->last_seen + 300;
                break;
            }
        } else if (ct->proto == IPPROTO_UDP) {
            ct->timeout = ct->last_seen + CONNTRACK_TIMEOUT;
        } else {
            ct->timeout = ct->last_seen + CONNTRACK_TIMEOUT;
        }
    }
    spin_unlock(&b->lock);
    return 0;
}

int conntrack_confirm(nf_conntrack_t* ct)
{
    if (!ct) return -1;
    uint32_t bucket = conntrack_tuple_hash(&ct->tuple);
    conntrack_bucket_t* b = &conntrack_hash.buckets[bucket];
    spin_lock(&b->lock);
    ct->confirmed = 1;
    ct->last_seen = conntrack_now();
    spin_unlock(&b->lock);
    return 0;
}

void conntrack_remove(nf_conntrack_t* ct)
{
    if (!ct) return;
    uint32_t bucket = conntrack_tuple_hash(&ct->tuple);
    conntrack_bucket_t* b = &conntrack_hash.buckets[bucket];
    spin_lock(&b->lock);
    if (ct->prev) {
        ct->prev->next = ct->next;
    } else {
        b->head = ct->next;
    }
    if (ct->next) {
        ct->next->prev = ct->prev;
    }
    b->count--;
    spin_unlock(&b->lock);
    spin_lock(&conntrack_hash.global_lock);
    conntrack_hash.total_count--;
    spin_unlock(&conntrack_hash.global_lock);
    kfree(ct);
}

void conntrack_cleanup(void)
{
    uint64_t now = conntrack_now();
    for (uint32_t i = 0; i < CONNTRACK_BUCKETS; i++) {
        conntrack_bucket_t* b = &conntrack_hash.buckets[i];
        spin_lock(&b->lock);
        nf_conntrack_t* ct = b->head;
        while (ct) {
            nf_conntrack_t* next = ct->next;
            if (now >= ct->timeout && ct->refcnt == 0) {
                if (ct->prev) {
                    ct->prev->next = ct->next;
                } else {
                    b->head = ct->next;
                }
                if (ct->next) {
                    ct->next->prev = ct->prev;
                }
                b->count--;
                kfree(ct);
            }
            ct = next;
        }
        spin_unlock(&b->lock);
    }
    spin_lock(&conntrack_hash.global_lock);
    uint32_t recount = 0;
    for (uint32_t i = 0; i < CONNTRACK_BUCKETS; i++) {
        recount += conntrack_hash.buckets[i].count;
    }
    conntrack_hash.total_count = recount;
    spin_unlock(&conntrack_hash.global_lock);
}

uint32_t conntrack_count(void)
{
    spin_lock(&conntrack_hash.global_lock);
    uint32_t c = conntrack_hash.total_count;
    spin_unlock(&conntrack_hash.global_lock);
    return c;
}

void conntrack_flush(void)
{
    for (uint32_t i = 0; i < CONNTRACK_BUCKETS; i++) {
        conntrack_bucket_t* b = &conntrack_hash.buckets[i];
        spin_lock(&b->lock);
        nf_conntrack_t* ct = b->head;
        while (ct) {
            nf_conntrack_t* next = ct->next;
            kfree(ct);
            ct = next;
        }
        b->head = NULL;
        b->count = 0;
        spin_unlock(&b->lock);
    }
    spin_lock(&conntrack_hash.global_lock);
    conntrack_hash.total_count = 0;
    spin_unlock(&conntrack_hash.global_lock);
}
