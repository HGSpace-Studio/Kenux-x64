#ifndef ARCH_ROUTE_H
#define ARCH_ROUTE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define FIB_TABLE_MAX       256
#define FIB_ENTRY_MAX       1024
#define RT_TABLE_MAIN       254
#define RT_TABLE_DEFAULT    253
#define RT_TABLE_LOCAL      255
#define ROUTE_IFNAME_MAX    16

#define RTA_ALIGN(len)      (((len) + 3) & ~3)
#define RTA_LENGTH(len)     (sizeof(struct rtattr) - sizeof(void*) + (len))
#define RTA_SPACE(len)      (RTA_ALIGN(RTA_LENGTH(len)))

#define RTN_UNSPEC          0
#define RTN_UNICAST         1
#define RTN_LOCAL           2
#define RTN_BROADCAST       3
#define RTN_MULTICAST       4
#define RTN_BLACKHOLE       5
#define RTN_UNREACHABLE     6
#define RTN_PROHIBIT        7

#define RTF_UP              0x0001
#define RTF_GATEWAY         0x0002
#define RTF_HOST            0x0004
#define RTF_DYNAMIC         0x0010
#define RTF_MODIFIED        0x0020
#define RTF_REJECT          0x0400

#define RT_ATTR_DST         1
#define RT_ATTR_SRC         2
#define RT_ATTR_GATEWAY     5
#define RT_ATTR_OIF         8
#define RT_ATTR_PRIORITY    6
#define RT_ATTR_METRICS     14

typedef struct rtattr {
    uint16_t rta_len;
    uint16_t rta_type;
    char rta_data[];
} rtattr_t;

typedef struct route_entry {
    uint32_t dst;
    uint32_t mask;
    uint32_t gw;
    int32_t  if_index;
    uint32_t metric;
    uint32_t mtu;
    uint16_t type;
    uint16_t flags;
    char     ifname[ROUTE_IFNAME_MAX];
    struct route_entry* next;
} route_entry_t;

typedef struct route_table {
    uint32_t tb_id;
    route_entry_t* entries;
    uint32_t count;
    spinlock_t lock;
} route_table_t;

typedef struct {
    route_table_t tables[FIB_TABLE_MAX];
    uint32_t table_count;
    spinlock_t global_lock;
} fib_table_t;

int route_init(void);

route_table_t* route_table_get(uint32_t table_id);
int route_table_create(uint32_t table_id);

int route_add(uint32_t table_id, uint32_t dst, uint32_t mask,
               uint32_t gw, int32_t if_index, uint32_t metric);
int route_del(uint32_t table_id, uint32_t dst, uint32_t mask);
const route_entry_t* route_lookup(uint32_t dst);

void route_flush(uint32_t table_id);
void route_flush_all(void);

int route_forward(uint32_t dst_ip, uint32_t* next_hop,
                  int32_t* out_if, char* ifname);

#endif
