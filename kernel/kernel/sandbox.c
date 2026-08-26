/*
 * sandbox.c - Cross-domain security sandbox implementation
 *
 * Implements seccomp-like syscall filtering, Linux capabilities-like
 * 64-bit capability bitmaps, privilege dropping and namespace isolation
 * flags backed by a static per-process filter table. Violations are routed
 * through the audit callback and the kernel log.
 *
 * Freestanding kernel code: static arrays only, no malloc, no stdlib.
 */

#include "kapi_sandbox.h"

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

/* ----- Static state (no dynamic allocation) ----- */
static kapi_sandbox_filter_t sandbox_filters[KAPI_SANDBOX_PROCESS_MAX];
static spinlock_t            sandbox_lock = SPINLOCK_INIT;
static kapi_sandbox_audit_fn_t sandbox_audit_cb;
static int                   sandbox_inited;

/* ----- Internal helpers ----- */

/* Find an active filter for pid. Caller holds sandbox_lock. */
static kapi_sandbox_filter_t* sandbox_lookup(int32_t pid)
{
    for (int i = 0; i < KAPI_SANDBOX_PROCESS_MAX; i++) {
        if ((sandbox_filters[i].flags & KAPI_SANDBOX_F_ACTIVE) &&
            sandbox_filters[i].pid == pid) {
            return &sandbox_filters[i];
        }
    }
    return NULL;
}

/* Allocate a slot for pid: reuse an existing one or grab a free slot. */
static kapi_sandbox_filter_t* sandbox_alloc_slot(int32_t pid)
{
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (f) {
        return f;
    }
    for (int i = 0; i < KAPI_SANDBOX_PROCESS_MAX; i++) {
        if (!(sandbox_filters[i].flags & KAPI_SANDBOX_F_ACTIVE)) {
            return &sandbox_filters[i];
        }
    }
    return NULL;
}

static inline int syscall_mask_test(const uint64_t* mask, int nr)
{
    if (nr < 0 || nr >= KAPI_SANDBOX_SYSCALL_NR_MAX) {
        return 0;
    }
    return (mask[nr >> 6] & (1ULL << (nr & 63))) != 0;
}

/* ----- Public API ----- */

int kapi_sandbox_init(void)
{
    if (sandbox_inited) {
        return 0;
    }
    memset(sandbox_filters, 0, sizeof(sandbox_filters));
    spin_init(&sandbox_lock);
    sandbox_audit_cb = NULL;
    sandbox_inited = 1;
    printk(KERN_INFO "sandbox: initialized (%d slots, %d-bit capset)\n",
           KAPI_SANDBOX_PROCESS_MAX, KAPI_SANDBOX_CAP_MAX);
    return 0;
}

int kapi_sandbox_install_filter(int32_t pid, const kapi_sandbox_policy_t* policy)
{
    if (!policy || pid < 0) {
        return -2; /* KAPI_EINVAL */
    }
    if (policy->mode != KAPI_SANDBOX_MODE_WHITELIST &&
        policy->mode != KAPI_SANDBOX_MODE_BLACKLIST) {
        return -2;
    }

    spin_lock(&sandbox_lock);
    if (!sandbox_inited) {
        spin_unlock(&sandbox_lock);
        return -7; /* KAPI_ENOSYS */
    }
    kapi_sandbox_filter_t* f = sandbox_alloc_slot(pid);
    if (!f) {
        spin_unlock(&sandbox_lock);
        printk(KERN_WARNING "sandbox: filter table full, cannot install for pid=%d\n",
               (int)pid);
        return -3; /* KAPI_ENOMEM */
    }

    memset(f, 0, sizeof(*f));
    f->pid = pid;
    f->mode = policy->mode;
    f->flags = KAPI_SANDBOX_F_ACTIVE | (policy->flags & ~KAPI_SANDBOX_F_ACTIVE);
    f->ns_flags = policy->ns_flags & KAPI_SANDBOX_NS_MASK;
    f->audit_flags = policy->audit_flags;

    if (policy->mode == KAPI_SANDBOX_MODE_WHITELIST) {
        memcpy(f->syscall_mask, policy->syscall_whitelist, sizeof(f->syscall_mask));
    } else {
        memcpy(f->syscall_mask, policy->syscall_blacklist, sizeof(f->syscall_mask));
    }

    /* Resolve capabilities: keep what is in keep_mask but not in drop_mask. */
    f->cap_effective   = policy->cap_keep_mask & ~policy->cap_drop_mask;
    f->cap_permitted   = f->cap_effective;
    f->cap_inheritable = 0;
    f->violations      = 0;
    spin_unlock(&sandbox_lock);

    printk(KERN_INFO "sandbox: filter installed pid=%d mode=%u ns=0x%x caps_eff=0x%lx\n",
           (int)pid, (unsigned)f->mode, (unsigned)f->ns_flags,
           (unsigned long)f->cap_effective);
    return 0;
}

int kapi_sandbox_remove_filter(int32_t pid)
{
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (!f) {
        spin_unlock(&sandbox_lock);
        return -4; /* KAPI_ENOENT */
    }
    memset(f, 0, sizeof(*f));
    spin_unlock(&sandbox_lock);
    printk(KERN_INFO "sandbox: filter removed pid=%d\n", (int)pid);
    return 0;
}

int kapi_sandbox_drop_caps(int32_t pid, uint64_t drop_mask)
{
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (!f) {
        spin_unlock(&sandbox_lock);
        printk(KERN_WARNING "sandbox: drop_caps: no filter for pid=%d\n", (int)pid);
        return -4;
    }
    if (f->flags & KAPI_SANDBOX_F_LOCKED) {
        uint64_t eff = f->cap_effective;
        spin_unlock(&sandbox_lock);
        printk(KERN_WARNING "sandbox: drop_caps: filter locked pid=%d caps=0x%lx eff=0x%lx\n",
               (int)pid, (unsigned long)drop_mask, (unsigned long)eff);
        return -6; /* KAPI_EBUSY */
    }
    f->cap_effective &= ~drop_mask;
    f->cap_permitted &= ~drop_mask;
    uint64_t eff_after = f->cap_effective;
    spin_unlock(&sandbox_lock);

    printk(KERN_INFO "sandbox: dropped caps=0x%lx for pid=%d effective=0x%lx\n",
           (unsigned long)drop_mask, (int)pid, (unsigned long)eff_after);
    return 0;
}

int kapi_sandbox_raise_caps(int32_t pid, uint64_t raise_mask)
{
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (!f) {
        spin_unlock(&sandbox_lock);
        return -4;
    }
    /* Can only raise capabilities that remain in the permitted set. */
    uint64_t allowed = raise_mask & f->cap_permitted;
    f->cap_effective |= allowed;
    spin_unlock(&sandbox_lock);
    return 0;
}

int kapi_sandbox_cap_check(int32_t pid, int cap)
{
    if (cap < 0 || cap >= KAPI_SANDBOX_CAP_MAX) {
        return -2;
    }
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (!f) {
        spin_unlock(&sandbox_lock);
        return -4; /* no sandbox: caller did not opt in */
    }
    int held = (f->cap_effective & KAPI_SANDBOX_CAP_BIT(cap)) != 0;
    spin_unlock(&sandbox_lock);

    if (!held) {
        kapi_sandbox_audit(pid, -1, KAPI_SANDBOX_AUDIT_CAP_DENIED,
                           "capability denied");
        return -EPERM;
    }
    return 0;
}

int kapi_sandbox_check_syscall(int32_t pid, int syscall_nr)
{
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    if (!f || !(f->flags & KAPI_SANDBOX_F_ACTIVE)) {
        spin_unlock(&sandbox_lock);
        return 0; /* no sandbox: allow */
    }
    uint32_t mode = f->mode;
    int present = syscall_mask_test(f->syscall_mask, syscall_nr);
    uint32_t audit_allow = f->audit_flags & KAPI_SANDBOX_F_AUDIT_ALLOW;
    spin_unlock(&sandbox_lock);

    int allow;
    if (mode == KAPI_SANDBOX_MODE_WHITELIST) {
        allow = present;   /* only listed syscalls pass */
    } else {
        allow = !present;  /* blacklist: deny only listed */
    }

    if (!allow) {
        kapi_sandbox_audit(pid, syscall_nr,
                           KAPI_SANDBOX_AUDIT_SYSCALL_BLOCKED,
                           "syscall blocked by sandbox");
        spin_lock(&sandbox_lock);
        /* re-fetch under lock to bump the violation counter safely */
        kapi_sandbox_filter_t* vf = sandbox_lookup(pid);
        if (vf) {
            vf->violations++;
        }
        spin_unlock(&sandbox_lock);
        printk(KERN_WARNING "sandbox: blocked syscall=%d pid=%d mode=%u\n",
               syscall_nr, (int)pid, (unsigned)mode);
        return -EPERM;
    }

    if (audit_allow) {
        kapi_sandbox_audit(pid, syscall_nr, KAPI_SANDBOX_AUDIT_ALLOW,
                           "syscall allowed");
    }
    return 0;
}

int kapi_sandbox_audit(int32_t pid, int syscall_nr, int reason,
                       const char* detail)
{
    if (sandbox_audit_cb) {
        sandbox_audit_cb(pid, syscall_nr, reason, detail);
    }
    if (reason == KAPI_SANDBOX_AUDIT_SYSCALL_BLOCKED ||
        reason == KAPI_SANDBOX_AUDIT_CAP_DENIED ||
        reason == KAPI_SANDBOX_AUDIT_NS_VIOLATION) {
        printk(KERN_WARNING "sandbox: audit pid=%d syscall=%d reason=%d detail=%s\n",
               (int)pid, syscall_nr, reason, detail ? detail : "(none)");
    }
    return 0;
}

void kapi_sandbox_set_audit_cb(kapi_sandbox_audit_fn_t cb)
{
    spin_lock(&sandbox_lock);
    sandbox_audit_cb = cb;
    spin_unlock(&sandbox_lock);
}

const kapi_sandbox_filter_t* kapi_sandbox_filter_get(int32_t pid)
{
    spin_lock(&sandbox_lock);
    kapi_sandbox_filter_t* f = sandbox_lookup(pid);
    spin_unlock(&sandbox_lock);
    return f;
}
