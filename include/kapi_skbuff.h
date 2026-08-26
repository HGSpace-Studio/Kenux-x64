#ifndef KAPI_SKBUFF_H
#define KAPI_SKBUFF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_skb kapi_skb_t;

struct kapi_skb {
    kapi_skb_t* next;
    kapi_skb_t* prev;
    uint8_t* head;
    uint8_t* data;
    uint8_t* tail;
    uint8_t* end;
    uint32_t len;
    uint32_t data_len;
    uint16_t mac_len;
    uint16_t network_len;
    uint16_t transport_len;
    uint32_t priority;
    uint32_t flags;
    int   dev_index;
};

#define KAPI_SKB_FF_CB        (1 << 0)
#define KAPI_SKB_GSO          (1 << 1)
#define KAPI_SKB_CSUM         (1 << 2)

kapi_skb_t* kapi_skb_alloc(size_t size);

void kapi_skb_free(kapi_skb_t* skb);

uint8_t* kapi_skb_push(kapi_skb_t* skb, size_t len);

uint8_t* kapi_skb_pull(kapi_skb_t* skb, size_t len);

uint8_t* kapi_skb_put(kapi_skb_t* skb, size_t len);

int kapi_skb_reserve(kapi_skb_t* skb, size_t len);

size_t kapi_skb_headroom(kapi_skb_t* skb);

size_t kapi_skb_tailroom(kapi_skb_t* skb);

size_t kapi_skb_len(kapi_skb_t* skb);

uint8_t* kapi_skb_mac_header(kapi_skb_t* skb);

uint8_t* kapi_skb_network_header(kapi_skb_t* skb);

uint8_t* kapi_skb_transport_header(kapi_skb_t* skb);

void kapi_skb_reset_mac_header(kapi_skb_t* skb);

void kapi_skb_reset_network_header(kapi_skb_t* skb);

void kapi_skb_reset_transport_header(kapi_skb_t* skb);

int kapi_skb_copy_bits(kapi_skb_t* skb, int offset, int size, void* to);

kapi_skb_t* kapi_skb_clone(kapi_skb_t* skb);

kapi_skb_t* kapi_skb_copy(kapi_skb_t* skb);

#ifdef __cplusplus
}
#endif

#endif
