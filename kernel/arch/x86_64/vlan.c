#include <arch/vlan.h>
#include <arch/slab.h>
#include <string.h>

static vlan_table_t vlan_tbl;
static uint16_t vlan_htons(uint16_t v) { return (v >> 8) | (v << 8); }
static uint16_t vlan_ntohs(uint16_t v) { return vlan_htons(v); }

void vlan_init(void)
{
    vlan_tbl.devices = NULL;
    vlan_tbl.count = 0;
    spin_init(&vlan_tbl.lock);
}

vlan_device_t* vlan_add_device(const char* parent, uint16_t vlan_id, const char* name)
{
    if (!parent || !name || vlan_id == 0 || vlan_id > VLAN_MAX_ID)
        return NULL;
    spin_lock(&vlan_tbl.lock);
    if (vlan_tbl.count >= VLAN_MAX_DEVICES) {
        spin_unlock(&vlan_tbl.lock);
        return NULL;
    }
    vlan_device_t* v = vlan_tbl.devices;
    while (v) {
        if (strcmp(v->name, name) == 0 ||
            (strcmp(v->parent, parent) == 0 && v->vlan_id == vlan_id)) {
            spin_unlock(&vlan_tbl.lock);
            return NULL;
        }
        v = v->next;
    }
    v = (vlan_device_t*)kmalloc(sizeof(vlan_device_t));
    if (!v) {
        spin_unlock(&vlan_tbl.lock);
        return NULL;
    }
    memset(v, 0, sizeof(vlan_device_t));
    strncpy(v->name, name, VLAN_NAME_MAX - 1);
    v->name[VLAN_NAME_MAX - 1] = 0;
    strncpy(v->parent, parent, VLAN_NAME_MAX - 1);
    v->parent[VLAN_NAME_MAX - 1] = 0;
    v->vlan_id = vlan_id;
    v->flags = 1;
    v->rx_packets = 0;
    v->tx_packets = 0;
    v->rx_bytes = 0;
    v->tx_bytes = 0;
    v->rx_drops = 0;
    v->tx_drops = 0;
    v->next = vlan_tbl.devices;
    vlan_tbl.devices = v;
    vlan_tbl.count++;
    spin_unlock(&vlan_tbl.lock);
    return v;
}

int vlan_del_device(const char* name)
{
    if (!name) return -1;
    spin_lock(&vlan_tbl.lock);
    vlan_device_t** pp = &vlan_tbl.devices;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            vlan_device_t* del = *pp;
            *pp = del->next;
            kfree(del);
            vlan_tbl.count--;
            spin_unlock(&vlan_tbl.lock);
            return 0;
        }
        pp = &(*pp)->next;
    }
    spin_unlock(&vlan_tbl.lock);
    return -1;
}

vlan_device_t* vlan_find_device(const char* name)
{
    if (!name) return NULL;
    spin_lock(&vlan_tbl.lock);
    vlan_device_t* v = vlan_tbl.devices;
    while (v) {
        if (strcmp(v->name, name) == 0) {
            spin_unlock(&vlan_tbl.lock);
            return v;
        }
        v = v->next;
    }
    spin_unlock(&vlan_tbl.lock);
    return NULL;
}

vlan_device_t* vlan_find_by_vid(const char* parent, uint16_t vlan_id)
{
    if (!parent) return NULL;
    spin_lock(&vlan_tbl.lock);
    vlan_device_t* v = vlan_tbl.devices;
    while (v) {
        if (strcmp(v->parent, parent) == 0 && v->vlan_id == vlan_id) {
            spin_unlock(&vlan_tbl.lock);
            return v;
        }
        v = v->next;
    }
    spin_unlock(&vlan_tbl.lock);
    return NULL;
}

int vlan_rx(const uint8_t* frame, uint64_t len, const char* parent)
{
    if (!frame || !parent) return -1;
    if (len < sizeof(eth_header_t) + VLAN_TAG_INSERT_SIZE) return -1;
    const eth_header_t* eth = (const eth_header_t*)frame;
    uint16_t eth_type = vlan_ntohs(eth->type);
    if (eth_type != VLAN_ETHERTYPE) return -1;
    const uint8_t* tag = frame + sizeof(eth_header_t);
    uint16_t tci = ((uint16_t)tag[0] << 8) | (uint16_t)tag[1];
    uint16_t vid = tci & VLAN_VID_MASK;
    uint16_t priority = (tci & VLAN_PRIORITY_MASK) >> VLAN_PRIORITY_SHIFT;
    (void)priority;
    uint16_t encap_proto = ((uint16_t)tag[2] << 8) | (uint16_t)tag[3];
    (void)encap_proto;
    vlan_device_t* dev = vlan_find_by_vid(parent, vid);
    if (!dev) return -1;
    uint64_t inner_len = len - sizeof(eth_header_t) - VLAN_TAG_INSERT_SIZE;
    dev->rx_packets++;
    dev->rx_bytes += (uint32_t)inner_len;
    return 0;
}

int vlan_tx(uint8_t* frame, uint64_t* len, uint64_t max_len,
             const char* vlan_name, uint16_t proto)
{
    if (!frame || !len || !vlan_name) return -1;
    vlan_device_t* dev = vlan_find_device(vlan_name);
    if (!dev) return -1;
    if (!dev->flags) {
        dev->tx_drops++;
        return -1;
    }
    if (*len + VLAN_TAG_INSERT_SIZE > max_len) {
        dev->tx_drops++;
        return -1;
    }
    if (*len < sizeof(eth_header_t)) return -1;
    uint64_t payload_len = *len - sizeof(eth_header_t);
    memmove(frame + sizeof(eth_header_t) + VLAN_TAG_INSERT_SIZE,
            frame + sizeof(eth_header_t), payload_len);
    uint16_t tci = (uint16_t)(dev->vlan_id & VLAN_VID_MASK);
    uint8_t* tag = frame + sizeof(eth_header_t);
    tag[0] = (uint8_t)(tci >> 8);
    tag[1] = (uint8_t)(tci & 0xff);
    tag[2] = (uint8_t)(proto >> 8);
    tag[3] = (uint8_t)(proto & 0xff);
    eth_header_t* eth = (eth_header_t*)frame;
    eth->type = vlan_htons(VLAN_ETHERTYPE);
    *len += VLAN_TAG_INSERT_SIZE;
    dev->tx_packets++;
    dev->tx_bytes += (uint32_t)*len;
    return 0;
}

uint32_t vlan_device_count(void)
{
    spin_lock(&vlan_tbl.lock);
    uint32_t c = vlan_tbl.count;
    spin_unlock(&vlan_tbl.lock);
    return c;
}

vlan_device_t* vlan_get_device_by_index(uint32_t index)
{
    spin_lock(&vlan_tbl.lock);
    vlan_device_t* dev = vlan_tbl.devices;
    uint32_t i = 0;
    while (dev) {
        if (i == index) {
            spin_unlock(&vlan_tbl.lock);
            return dev;
        }
        dev = dev->next;
        i++;
    }
    spin_unlock(&vlan_tbl.lock);
    return NULL;
}
