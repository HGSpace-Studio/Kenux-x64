#include <arch/route.h>
#include <arch/slab.h>
#include <string.h>

/* Route types */
#define RTN_UNICAST    1
#define RTN_HOST       7

static fib_table_t fib;

static uint32_t mask_to_prefix(uint32_t mask)
{
    uint32_t prefix = 0;
    while (mask) {
        prefix += mask & 1;
        mask >>= 1;
    }
    return prefix;
}

int route_init(void)
{
    spin_init(&fib.global_lock);
    fib.table_count = 0;
    for (uint32_t i = 0; i < FIB_TABLE_MAX; i++) {
        fib.tables[i].tb_id = 0;
        fib.tables[i].entries = NULL;
        fib.tables[i].count = 0;
        spin_init(&fib.tables[i].lock);
    }
    route_table_create(RT_TABLE_LOCAL);
    route_table_create(RT_TABLE_MAIN);
    route_table_create(RT_TABLE_DEFAULT);
    route_add(RT_TABLE_LOCAL, 0x7f000001, 0xffffffff, 0, 1, 0);
    route_add(RT_TABLE_LOCAL, 0x7f000000, 0xff000000, 0, 1, 0);
    return 0;
}

route_table_t* route_table_get(uint32_t table_id)
{
    for (uint32_t i = 0; i < FIB_TABLE_MAX; i++) {
        if (fib.tables[i].tb_id == table_id && fib.tables[i].count >= 0) {
            return &fib.tables[i];
        }
    }
    return NULL;
}

int route_table_create(uint32_t table_id)
{
    spin_lock(&fib.global_lock);
    for (uint32_t i = 0; i < FIB_TABLE_MAX; i++) {
        if (fib.tables[i].tb_id == 0) {
            fib.tables[i].tb_id = table_id;
            fib.tables[i].entries = NULL;
            fib.tables[i].count = 0;
            spin_init(&fib.tables[i].lock);
            fib.table_count++;
            spin_unlock(&fib.global_lock);
            return 0;
        }
    }
    spin_unlock(&fib.global_lock);
    return -1;
}

static void route_insert_sorted(route_table_t* table, route_entry_t* entry)
{
    uint32_t new_prefix = mask_to_prefix(entry->mask);
    route_entry_t** pp = &table->entries;
    while (*pp) {
        uint32_t cur_prefix = mask_to_prefix((*pp)->mask);
        if (new_prefix > cur_prefix) {
            break;
        }
        if (new_prefix == cur_prefix && entry->metric < (*pp)->metric) {
            break;
        }
        pp = &(*pp)->next;
    }
    entry->next = *pp;
    *pp = entry;
}

int route_add(uint32_t table_id, uint32_t dst, uint32_t mask,
               uint32_t gw, int32_t if_index, uint32_t metric)
{
    route_table_t* table = route_table_get(table_id);
    if (!table) {
        if (route_table_create(table_id) != 0)
            return -1;
        table = route_table_get(table_id);
        if (!table)
            return -1;
    }
    spin_lock(&table->lock);
    if (table->count >= FIB_ENTRY_MAX) {
        spin_unlock(&table->lock);
        return -1;
    }
    route_entry_t* e = (route_entry_t*)kmalloc(sizeof(route_entry_t));
    if (!e) {
        spin_unlock(&table->lock);
        return -1;
    }
    e->dst = dst & mask;
    e->mask = mask;
    e->gw = gw;
    e->if_index = if_index;
    e->metric = metric;
    e->mtu = 1500;
    e->type = (mask == 0xffffffff) ? RTN_HOST : RTN_UNICAST;
    e->flags = RTF_UP;
    if (gw != 0) {
        e->flags |= RTF_GATEWAY;
    }
    e->ifname[0] = 0;
    e->next = NULL;
    route_insert_sorted(table, e);
    table->count++;
    spin_unlock(&table->lock);
    return 0;
}

int route_del(uint32_t table_id, uint32_t dst, uint32_t mask)
{
    route_table_t* table = route_table_get(table_id);
    if (!table)
        return -1;
    spin_lock(&table->lock);
    route_entry_t** pp = &table->entries;
    while (*pp) {
        route_entry_t* cur = *pp;
        if (cur->dst == (dst & mask) && cur->mask == mask) {
            *pp = cur->next;
            kfree(cur);
            table->count--;
            spin_unlock(&table->lock);
            return 0;
        }
        pp = &cur->next;
    }
    spin_unlock(&table->lock);
    return -1;
}

const route_entry_t* route_lookup(uint32_t dst)
{
    const route_entry_t* best = NULL;
    uint32_t best_prefix = 0;
    spin_lock(&fib.global_lock);
    for (uint32_t t = 0; t < FIB_TABLE_MAX; t++) {
        if (fib.tables[t].tb_id == 0)
            continue;
        route_table_t* table = &fib.tables[t];
        spin_lock(&table->lock);
        route_entry_t* e = table->entries;
        while (e) {
            if ((dst & e->mask) == e->dst) {
                uint32_t p = mask_to_prefix(e->mask);
                if (p > best_prefix) {
                    best_prefix = p;
                    best = e;
                }
            }
            e = e->next;
        }
        spin_unlock(&table->lock);
    }
    spin_unlock(&fib.global_lock);
    return best;
}

void route_flush(uint32_t table_id)
{
    route_table_t* table = route_table_get(table_id);
    if (!table)
        return;
    spin_lock(&table->lock);
    route_entry_t* e = table->entries;
    while (e) {
        route_entry_t* next = e->next;
        kfree(e);
        e = next;
    }
    table->entries = NULL;
    table->count = 0;
    spin_unlock(&table->lock);
}

void route_flush_all(void)
{
    spin_lock(&fib.global_lock);
    for (uint32_t i = 0; i < FIB_TABLE_MAX; i++) {
        if (fib.tables[i].tb_id == 0)
            continue;
        route_table_t* table = &fib.tables[i];
        spin_lock(&table->lock);
        route_entry_t* e = table->entries;
        while (e) {
            route_entry_t* next = e->next;
            kfree(e);
            e = next;
        }
        table->entries = NULL;
        table->count = 0;
        spin_unlock(&table->lock);
    }
    spin_unlock(&fib.global_lock);
}

int route_forward(uint32_t dst_ip, uint32_t* next_hop,
                  int32_t* out_if, char* ifname)
{
    const route_entry_t* e = route_lookup(dst_ip);
    if (!e)
        return -1;
    if (e->flags & RTF_GATEWAY) {
        if (next_hop)
            *next_hop = e->gw;
    } else {
        if (next_hop)
            *next_hop = dst_ip;
    }
    if (out_if)
        *out_if = e->if_index;
    if (ifname && e->ifname[0]) {
        for (int i = 0; i < ROUTE_IFNAME_MAX && e->ifname[i]; i++)
            ifname[i] = e->ifname[i];
        ifname[ROUTE_IFNAME_MAX - 1] = 0;
    }
    return 0;
}
