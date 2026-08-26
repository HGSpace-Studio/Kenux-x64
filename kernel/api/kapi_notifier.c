

#include "kapi_notifier.h"
#include "kapi.h"

void kapi_notifier_chain_init(kapi_notifier_chain_t* chain)
{
    if (!chain) {
        return;
    }
    chain->head = NULL;
}

int kapi_notifier_chain_register(kapi_notifier_chain_t* chain, kapi_notifier_block_t* nb)
{
    if (!chain || !nb || !nb->notifier_call) {
        return KAPI_EINVAL;
    }

    nb->next = chain->head;
    chain->head = nb;
    return KAPI_OK;
}

int kapi_notifier_chain_unregister(kapi_notifier_chain_t* chain, kapi_notifier_block_t* nb)
{
    if (!chain || !nb) {
        return KAPI_EINVAL;
    }

    kapi_notifier_block_t** pp = &chain->head;
    while (*pp) {
        if (*pp == nb) {
            *pp = nb->next;
            nb->next = NULL;
            return KAPI_OK;
        }
        pp = &(*pp)->next;
    }
    return KAPI_ERROR;
}

int kapi_notifier_call_chain(kapi_notifier_chain_t* chain, unsigned long action, void* data)
{
    if (!chain) {
        return KAPI_EINVAL;
    }

    int ret = KAPI_OK;
    kapi_notifier_block_t* nb = chain->head;
    while (nb) {
        ret = nb->notifier_call(nb, action, data);
        if (ret < 0) {
            break;
        }
        nb = nb->next;
    }
    return ret;
}
