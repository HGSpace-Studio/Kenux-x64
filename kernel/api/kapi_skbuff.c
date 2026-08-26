#include "kapi.h"
#include "kapi_memory.h"

#include <string.h>

static uint32_t skb_alloc_count = 0;

int kapi_skbuff_init(void)
{
    skb_alloc_count = 0;
    return KAPI_OK;
}

kapi_skb_t* kapi_skb_alloc(size_t size)
{
    size_t headroom = 64;
    size_t total = sizeof(kapi_skb_t) + headroom + size;
    kapi_skb_t* skb = (kapi_skb_t*)kapi_malloc(total);
    if (!skb) return NULL;
    memset(skb, 0, sizeof(*skb));
    skb->head = (uint8_t*)skb + sizeof(kapi_skb_t);
    skb->data = skb->head + headroom;
    skb->tail = skb->data;
    skb->end = skb->head + headroom + size;
    skb->len = 0;
    skb->data_len = 0;
    skb_alloc_count++;
    return skb;
}

void kapi_skb_free(kapi_skb_t* skb)
{
    if (!skb) return;
    if (skb_alloc_count) skb_alloc_count--;
    kapi_free(skb);
}

uint8_t* kapi_skb_push(kapi_skb_t* skb, size_t len)
{
    if (!skb || len > (size_t)(skb->data - skb->head)) return NULL;
    skb->data -= len;
    skb->len += (uint32_t)len;
    return skb->data;
}

uint8_t* kapi_skb_pull(kapi_skb_t* skb, size_t len)
{
    if (!skb || len > skb->len) return NULL;
    skb->data += len;
    skb->len -= (uint32_t)len;
    if (skb->data > skb->tail) skb->tail = skb->data;
    return skb->data;
}

uint8_t* kapi_skb_put(kapi_skb_t* skb, size_t len)
{
    if (!skb || len > (size_t)(skb->end - skb->tail)) return NULL;
    uint8_t* p = skb->tail;
    skb->tail += len;
    skb->len += (uint32_t)len;
    return p;
}

int kapi_skb_reserve(kapi_skb_t* skb, size_t len)
{
    if (!skb || skb->len != 0 || len > (size_t)(skb->end - skb->data)) return KAPI_EINVAL;
    skb->data += len;
    skb->tail += len;
    return KAPI_OK;
}

size_t kapi_skb_headroom(kapi_skb_t* skb)
{
    if (!skb) return 0;
    return (size_t)(skb->data - skb->head);
}

size_t kapi_skb_tailroom(kapi_skb_t* skb)
{
    if (!skb) return 0;
    return (size_t)(skb->end - skb->tail);
}

size_t kapi_skb_len(kapi_skb_t* skb)
{
    return skb ? skb->len : 0;
}

uint8_t* kapi_skb_mac_header(kapi_skb_t* skb)
{
    if (!skb || skb->mac_len == 0) return NULL;
    if (skb->head + skb->mac_len > skb->end) return NULL;
    return skb->head + skb->mac_len;
}

uint8_t* kapi_skb_network_header(kapi_skb_t* skb)
{
    if (!skb || skb->network_len == 0) return NULL;
    if (skb->head + skb->network_len > skb->end) return NULL;
    return skb->head + skb->network_len;
}

uint8_t* kapi_skb_transport_header(kapi_skb_t* skb)
{
    if (!skb || skb->transport_len == 0) return NULL;
    if (skb->head + skb->transport_len > skb->end) return NULL;
    return skb->head + skb->transport_len;
}

void kapi_skb_reset_mac_header(kapi_skb_t* skb)
{
    if (!skb) return;
    skb->mac_len = (uint16_t)(skb->data - skb->head);
}

void kapi_skb_reset_network_header(kapi_skb_t* skb)
{
    if (!skb) return;
    skb->network_len = (uint16_t)(skb->data - skb->head);
}

void kapi_skb_reset_transport_header(kapi_skb_t* skb)
{
    if (!skb) return;
    skb->transport_len = (uint16_t)(skb->data - skb->head);
}

int kapi_skb_copy_bits(kapi_skb_t* skb, int offset, int size, void* to)
{
    if (!skb || !to || offset < 0 || size < 0) return KAPI_EINVAL;
    if ((uint32_t)offset + (uint32_t)size > skb->len) return KAPI_EINVAL;
    memcpy(to, skb->data + offset, (size_t)size);
    return KAPI_OK;
}

kapi_skb_t* kapi_skb_clone(kapi_skb_t* skb)
{
    return kapi_skb_copy(skb);
}

kapi_skb_t* kapi_skb_copy(kapi_skb_t* skb)
{
    if (!skb) return NULL;
    kapi_skb_t* n = kapi_skb_alloc(kapi_skb_headroom(skb) + skb->len + kapi_skb_tailroom(skb));
    if (!n) return NULL;
    size_t reserve = kapi_skb_headroom(skb);
    kapi_skb_reserve(n, reserve);
    uint8_t* dst = kapi_skb_put(n, skb->len);
    if (!dst) {
        kapi_skb_free(n);
        return NULL;
    }
    memcpy(dst, skb->data, skb->len);
    n->priority = skb->priority;
    n->flags = skb->flags;
    n->dev_index = skb->dev_index;
    n->mac_len = skb->mac_len;
    n->network_len = skb->network_len;
    n->transport_len = skb->transport_len;
    return n;
}
