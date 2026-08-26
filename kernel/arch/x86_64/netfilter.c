#include <arch/netfilter.h>
#include <arch/slab.h>
#include <string.h>

static struct nf_hook_ops* nf_inet_hooks[NF_INET_NUMHOOKS];
static spinlock_t nf_hooks_lock = SPINLOCK_INIT;

void nf_init(void)
{
    spin_init(&nf_hooks_lock);
    for (int i = 0; i < NF_INET_NUMHOOKS; i++) {
        nf_inet_hooks[i] = NULL;
    }
}

void nf_hook_slow(void* skb, const void* state, int pf, int hooknum,
                  struct nf_hook_ops** hook_list)
{
    (void)pf;
    (void)hooknum;
    if (!hook_list || !*hook_list)
        return;
    struct nf_hook_ops* ops = *hook_list;
    while (ops) {
        int verdict = ops->hook(skb, state, ops->priv);
        if (verdict != NF_ACCEPT)
            break;
        ops = ops->next;
    }
}

static int nf_hooks_count(int hooknum)
{
    int count = 0;
    struct nf_hook_ops* ops = nf_inet_hooks[hooknum];
    while (ops) {
        count++;
        ops = ops->next;
    }
    return count;
}

static void nf_insert_hook_sorted(int hooknum, nf_hook_ops_t* reg)
{
    struct nf_hook_ops** pp = &nf_inet_hooks[hooknum];
    while (*pp) {
        if (reg->priority < (*pp)->priority) {
            break;
        }
        pp = &(*pp)->next;
    }
    reg->next = *pp;
    *pp = reg;
}

int nf_register_hook(nf_hook_ops_t* reg)
{
    if (!reg || reg->hooknum < 0 || reg->hooknum >= NF_INET_NUMHOOKS)
        return -1;
    spin_lock(&nf_hooks_lock);
    if (nf_hooks_count(reg->hooknum) >= NF_MAX_HOOKS_PER_HOOK) {
        spin_unlock(&nf_hooks_lock);
        return -1;
    }
    nf_insert_hook_sorted(reg->hooknum, reg);
    spin_unlock(&nf_hooks_lock);
    return 0;
}

void nf_unregister_hook(nf_hook_ops_t* reg)
{
    if (!reg || reg->hooknum < 0 || reg->hooknum >= NF_INET_NUMHOOKS)
        return;
    spin_lock(&nf_hooks_lock);
    struct nf_hook_ops** pp = &nf_inet_hooks[reg->hooknum];
    while (*pp) {
        if (*pp == reg) {
            *pp = reg->next;
            reg->next = NULL;
            break;
        }
        pp = &(*pp)->next;
    }
    spin_unlock(&nf_hooks_lock);
}

int nf_register_net_hooks(int pf, nf_hook_ops_t* reg, int n)
{
    (void)pf;
    for (int i = 0; i < n; i++) {
        int ret = nf_register_hook(&reg[i]);
        if (ret != 0) {
            for (int j = i - 1; j >= 0; j--) {
                nf_unregister_hook(&reg[j]);
            }
            return ret;
        }
    }
    return 0;
}

void nf_unregister_net_hooks(int pf, nf_hook_ops_t* reg, int n)
{
    (void)pf;
    for (int i = 0; i < n; i++) {
        nf_unregister_hook(&reg[i]);
    }
}
