

#ifndef KAPI_NOTIFIER_H
#define KAPI_NOTIFIER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_notifier_block {
    struct kapi_notifier_block* next;
    int (*notifier_call)(struct kapi_notifier_block* self, unsigned long action, void* data);
    int priority;
} kapi_notifier_block_t;

typedef struct kapi_notifier_chain {
    kapi_notifier_block_t* head;
} kapi_notifier_chain_t;

void kapi_notifier_chain_init(kapi_notifier_chain_t* chain);

int kapi_notifier_chain_register(kapi_notifier_chain_t* chain, kapi_notifier_block_t* nb);

int kapi_notifier_chain_unregister(kapi_notifier_chain_t* chain, kapi_notifier_block_t* nb);

int kapi_notifier_call_chain(kapi_notifier_chain_t* chain, unsigned long action, void* data);

#ifdef __cplusplus
}
#endif

#endif
