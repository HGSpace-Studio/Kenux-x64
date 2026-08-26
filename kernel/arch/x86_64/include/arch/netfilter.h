#ifndef ARCH_NETFILTER_H
#define ARCH_NETFILTER_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/net.h>

#define NF_INET_PREROUTING   0
#define NF_INET_INPUT        1
#define NF_INET_FORWARD      2
#define NF_INET_OUTPUT       3
#define NF_INET_POSTROUTING  4
#define NF_INET_NUMHOOKS     5

#define NF_DROP              0
#define NF_ACCEPT            1
#define NF_STOLEN            2
#define NF_QUEUE             3
#define NF_REPEAT            4
#define NF_STOP              5

#define NF_INET_PRI_FIRST   (-2147483647 - 1)
#define NF_IP_PRI_CONNTRACK  (-200)
#define NF_IP_PRI_FILTER    0
#define NF_IP_PRI_NAT_SRC   100
#define NF_INET_PRI_LAST    2147483647

#define NF_MAX_HOOKS_PER_HOOK 64

typedef int (*nf_hookfn_t)(void* skb, const void* state, void* priv);

typedef struct nf_hook_ops {
    nf_hookfn_t hook;
    int pf;
    int hooknum;
    int priority;
    void* priv;
    struct nf_hook_ops* next;
} nf_hook_ops_t;

void nf_hook_slow(void* skb, const void* state, int pf, int hooknum,
                  struct nf_hook_ops** hook_list);

int nf_register_hook(nf_hook_ops_t* reg);
void nf_unregister_hook(nf_hook_ops_t* reg);
int nf_register_net_hooks(int pf, nf_hook_ops_t* reg, int n);
void nf_unregister_net_hooks(int pf, nf_hook_ops_t* reg, int n);

void nf_init(void);

static inline uint16_t nf_hook_slow_eval(struct nf_hook_ops** hook_list,
                                         void* skb, const void* state)
{
    int verdict = NF_ACCEPT;
    if (!hook_list || !*hook_list)
        return verdict;
    struct nf_hook_ops* ops = *hook_list;
    while (ops) {
        verdict = ops->hook(skb, state, ops->priv);
        if (verdict != NF_ACCEPT)
            return (uint16_t)verdict;
        ops = ops->next;
    }
    return NF_ACCEPT;
}

#endif
