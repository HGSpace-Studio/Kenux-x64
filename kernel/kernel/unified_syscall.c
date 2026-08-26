/*
 * unified_syscall.c - Fused kernel unified system call entry layer
 *
 * Implements the unified syscall dispatch table that fuses kernel-internal
 * and user-originated syscalls. The invoke path detects the calling domain
 * from the RPL bits of the saved CS register, enforces descriptor flags /
 * minimum capabilities, records per-syscall rdtsc latency statistics and
 * fires tracing hooks. Unknown syscalls return -ENOSYS.
 *
 * Freestanding kernel code: static tables only, no malloc, no stdlib.
 */

#include "kapi_unified_syscall.h"

#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>

/* printk is implemented in kernel/kernel/printk.c; there is no public
 * printk.h header, so the symbol is declared here like in epoll.c / mmap.c. */
extern void printk(const char* fmt, ...);

/* Log level prefixes parsed by printk(). Mirrors kernel/kernel/printk.c. */
#define KERN_EMERG    "<0>"
#define KERN_ALERT    "<1>"
#define KERN_CRIT     "<2>"
#define KERN_ERR      "<3>"
#define KERN_WARNING  "<4>"
#define KERN_NOTICE   "<5>"
#define KERN_INFO     "<6>"
#define KERN_DEBUG    "<7>"

/* Sentinel used before the first sample is recorded. */
#define UNISYS_CYCLES_MAX  ((uint64_t)(~((uint64_t)0)))

/* ----- Static state (no dynamic allocation) ----- */
static kapi_unisys_desc_t  unisys_table[KAPI_SYSCALL_MAX];
static kapi_unisys_stats_t unisys_stats[KAPI_SYSCALL_MAX];
static spinlock_t          unisys_lock = SPINLOCK_INIT;
static kapi_unisys_trace_fn_t unisys_trace_cb;
static int                 unisys_inited;

/* ----- Internal helpers ----- */

/* Read the timestamp counter (x86_64). */
static inline uint64_t unisys_rdtsc(void)
{
    uint32_t hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

/* Inspect the RPL bits of CS: nonzero means user mode (ring 3). */
static inline int unisys_from_user(uint64_t cs)
{
    return (cs & 0x3u) != 0;
}

/* ----- Public API ----- */

int kapi_unisys_init(void)
{
    if (unisys_inited) {
        return 0;
    }
    memset(unisys_table, 0, sizeof(unisys_table));
    memset(unisys_stats, 0, sizeof(unisys_stats));
    /* min_cycles is "uninitialized" until the first sample arrives. */
    for (int i = 0; i < KAPI_SYSCALL_MAX; i++) {
        unisys_stats[i].min_cycles = UNISYS_CYCLES_MAX;
    }
    spin_init(&unisys_lock);
    unisys_trace_cb = NULL;
    unisys_inited = 1;
    printk(KERN_INFO "unisys: unified syscall table initialized (%d slots)\n",
           KAPI_SYSCALL_MAX);
    return 0;
}

int kapi_unisys_register(int nr, const char* name,
                         kapi_unisys_handler_t fn, uint32_t flags)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_MAX) {
        return -2; /* KAPI_EINVAL */
    }
    if (!fn) {
        return -2;
    }

    spin_lock(&unisys_lock);
    kapi_unisys_desc_t* d = &unisys_table[nr];
    d->nr      = nr;
    d->handler = fn;
    d->flags   = flags;
    if (name) {
        strncpy(d->name, name, KAPI_UNISYS_NAME_MAX - 1);
        d->name[KAPI_UNISYS_NAME_MAX - 1] = '\0';
    } else {
        d->name[0] = '\0';
    }
    memset(&unisys_stats[nr], 0, sizeof(kapi_unisys_stats_t));
    unisys_stats[nr].min_cycles = UNISYS_CYCLES_MAX;
    spin_unlock(&unisys_lock);

    printk(KERN_INFO "unisys: registered nr=%d name=%s flags=0x%x\n",
           nr, d->name, (unsigned)flags);
    return 0;
}

int kapi_unisys_unregister(int nr)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_MAX) {
        return -2;
    }
    spin_lock(&unisys_lock);
    memset(&unisys_table[nr], 0, sizeof(kapi_unisys_desc_t));
    spin_unlock(&unisys_lock);
    printk(KERN_INFO "unisys: unregistered nr=%d\n", nr);
    return 0;
}

int kapi_unisys_detect_domain(const kapi_unisys_context_t* ctx)
{
    if (!ctx) {
        return KAPI_UNISYS_DOMAIN_KERNEL;
    }
    /* Explicit context flags win over CS inspection. */
    if (ctx->flags & KAPI_UNISYS_CTX_FROM_USER) {
        return KAPI_UNISYS_DOMAIN_USER;
    }
    if (ctx->flags & KAPI_UNISYS_CTX_FROM_KERNEL) {
        return KAPI_UNISYS_DOMAIN_KERNEL;
    }
    /* Fall back to RPL of the saved CS selector. */
    if (unisys_from_user(ctx->cs)) {
        return KAPI_UNISYS_DOMAIN_USER;
    }
    /* Container / virtualization hint carried in context.domain. */
    if (ctx->domain == KAPI_UNISYS_DOMAIN_CONTAINER) {
        return KAPI_UNISYS_DOMAIN_CONTAINER;
    }
    if (ctx->domain == KAPI_UNISYS_DOMAIN_VIRTUALIZATION) {
        return KAPI_UNISYS_DOMAIN_VIRTUALIZATION;
    }
    return KAPI_UNISYS_DOMAIN_KERNEL;
}

long kapi_unisys_invoke(int nr, long a1, long a2, long a3, long a4,
                        long a5, long a6, const kapi_unisys_context_t* ctx)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_MAX) {
        printk(KERN_WARNING "unisys: out-of-range syscall nr=%d\n", nr);
        return -ENOSYS;
    }

    kapi_unisys_desc_t* d = &unisys_table[nr];
    if (!d->handler) {
        printk(KERN_WARNING "unisys: unregistered syscall nr=%d\n", nr);
        return -ENOSYS;
    }

    int domain;
    if (ctx) {
        domain = kapi_unisys_detect_domain(ctx);
    } else {
        domain = KAPI_UNISYS_DOMAIN_KERNEL;
    }

    if (d->flags & KAPI_UNISYS_FLAG_DEPRECATED) {
        printk(KERN_NOTICE "unisys: deprecated syscall nr=%d invoked (pid=%d)\n",
               nr, ctx ? (int)ctx->pid : 0);
    }

    /* Cross-domain enforcement:
     *  - privileged syscalls may not be issued from user domain;
     *  - non user-safe syscalls may not be issued from user domain
     *    unless DOMAIN_ANY is set. */
    if (domain == KAPI_UNISYS_DOMAIN_USER) {
        if (d->flags & KAPI_UNISYS_FLAG_PRIVILEGED) {
            printk(KERN_WARNING "unisys: privileged syscall nr=%d denied from user (pid=%d)\n",
                   nr, ctx ? (int)ctx->pid : 0);
            return -EPERM;
        }
        if (!(d->flags & (KAPI_UNISYS_FLAG_USER_SAFE |
                          KAPI_UNISYS_FLAG_DOMAIN_ANY))) {
            printk(KERN_WARNING "unisys: non-user-safe syscall nr=%d denied from user (pid=%d)\n",
                   nr, ctx ? (int)ctx->pid : 0);
            return -EPERM;
        }
    }

    int traced = (unisys_trace_cb != NULL) &&
                 (d->flags & KAPI_UNISYS_FLAG_TRACED);

    if (traced) {
        unisys_trace_cb(ctx, d, 0, 0, 0); /* phase 0: entry */
    }

    uint64_t t0 = unisys_rdtsc();
    long ret = d->handler(a1, a2, a3, a4, a5, a6);
    uint64_t t1 = unisys_rdtsc();
    uint64_t cycles = t1 - t0;

    if (traced) {
        unisys_trace_cb(ctx, d, ret, cycles, 1); /* phase 1: exit */
    }

    /* Update statistics under lock. */
    spin_lock(&unisys_lock);
    kapi_unisys_stats_t* s = &unisys_stats[nr];
    s->count++;
    s->total_cycles += cycles;
    if (cycles > s->max_cycles) {
        s->max_cycles = cycles;
    }
    if (cycles < s->min_cycles) {
        s->min_cycles = cycles;
    }
    if (ret < 0) {
        s->error_count++;
        s->last_errno = (uint64_t)(-ret);
    }
    spin_unlock(&unisys_lock);

    return ret;
}

int kapi_unisys_stats_get(int nr, kapi_unisys_stats_t* out)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_MAX || !out) {
        return -2;
    }
    spin_lock(&unisys_lock);
    memcpy(out, &unisys_stats[nr], sizeof(*out));
    /* Report "no sample yet" as 0 for callers' convenience. */
    if (out->min_cycles == UNISYS_CYCLES_MAX) {
        out->min_cycles = 0;
    }
    spin_unlock(&unisys_lock);
    return 0;
}

int kapi_unisys_stats_reset(int nr)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_MAX) {
        return -2;
    }
    spin_lock(&unisys_lock);
    memset(&unisys_stats[nr], 0, sizeof(kapi_unisys_stats_t));
    unisys_stats[nr].min_cycles = UNISYS_CYCLES_MAX;
    spin_unlock(&unisys_lock);
    return 0;
}

void kapi_unisys_trace_set(kapi_unisys_trace_fn_t fn)
{
    spin_lock(&unisys_lock);
    unisys_trace_cb = fn;
    spin_unlock(&unisys_lock);
}

int kapi_unisys_errno_to_kapi(int err)
{
    /* err is a positive errno value (e.g. EPERM=1). Maps to KAPI codes. */
    switch (err) {
        case 0:        return 0;             /* KAPI_OK */
        case 1:        return -5;            /* EPERM  -> KAPI_EACCES */
        case 2:        return -4;            /* ENOENT -> KAPI_ENOENT */
        case 11:       return -8;            /* EAGAIN -> KAPI_EAGAIN */
        case 12:       return -3;            /* ENOMEM -> KAPI_ENOMEM */
        case 13:       return -5;            /* EACCES -> KAPI_EACCES */
        case 16:       return -6;            /* EBUSY  -> KAPI_EBUSY */
        case 22:       return -2;            /* EINVAL -> KAPI_EINVAL */
        case 38:       return -7;            /* ENOSYS -> KAPI_ENOSYS */
        default:       return -1;            /* KAPI_ERROR */
    }
}
