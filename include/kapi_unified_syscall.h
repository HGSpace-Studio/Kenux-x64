#ifndef _KAPI_UNIFIED_SYSCALL_H
#define _KAPI_UNIFIED_SYSCALL_H

/*
 * Kenux KAPI - Fused Kernel Unified System Call Entry Layer
 *
 * Unifies kernel-internal and user-originated syscall dispatch through a
 * single descriptor table. Each syscall carries its handler, flags and
 * minimum capability requirement. The invoke path detects the calling
 * domain (user / kernel / container / virtualization) from the saved CS
 * register RPL, records per-syscall statistics with rdtsc and fires
 * tracing hooks. Errno values are converted to KAPI error codes.
 *
 * Freestanding kernel code: no standard library, static tables only.
 */

#include <stdint.h>
#include <stddef.h>

/* Errnos used by the unified layer. Mirror kernel/lib/libc/errno.h. */
#ifndef EPERM
#define EPERM 1
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Total number of unified syscall slots (matches kapi_syscall.h). */
#ifndef KAPI_SYSCALL_MAX
#define KAPI_SYSCALL_MAX       1000
#endif

#define KAPI_UNISYS_NAME_MAX   32

/* ----- Call domains ----- */
#define KAPI_UNISYS_DOMAIN_KERNEL         0
#define KAPI_UNISYS_DOMAIN_USER           1
#define KAPI_UNISYS_DOMAIN_CONTAINER      2
#define KAPI_UNISYS_DOMAIN_VIRTUALIZATION 3

/* ----- Descriptor flags ----- */
#define KAPI_UNISYS_FLAG_NONE          0x00000000u
#define KAPI_UNISYS_FLAG_USER_SAFE     0x00000001u   /* callable from user domain */
#define KAPI_UNISYS_FLAG_PRIVILEGED    0x00000002u   /* requires kernel / elevated cap */
#define KAPI_UNISYS_FLAG_TRACED        0x00000004u   /* always emit trace events */
#define KAPI_UNISYS_FLAG_DEPRECATED    0x00000008u   /* log on use */
#define KAPI_UNISYS_FLAG_DOMAIN_ANY    0x00000010u   /* callable from any domain */

/* ----- Context flags ----- */
#define KAPI_UNISYS_CTX_FROM_USER      0x00000001u   /* explicit user origin */
#define KAPI_UNISYS_CTX_FROM_KERNEL    0x00000002u   /* explicit kernel origin */
#define KAPI_UNISYS_CTX_BLOCKED        0x80000000u   /* request was denied */

/* Syscall handler signature (compatible with kapi_syscall_fn_t). */
typedef long (*kapi_unisys_handler_t)(long a1, long a2, long a3,
                                      long a4, long a5, long a6);

/*
 * Cross-domain call context. Passed into the invoke path. The saved CS
 * register is used to detect user vs kernel origin via its RPL bits.
 */
typedef struct {
    int32_t  domain;        /* KAPI_UNISYS_DOMAIN_* (caller hint) */
    uint32_t flags;         /* KAPI_UNISYS_CTX_* */
    int32_t  pid;           /* calling task pid */
    int32_t  syscall_nr;    /* syscall number being invoked */
    uint64_t cs;            /* saved code segment selector (RPL inspected) */
    uint64_t rip;           /* caller instruction pointer */
    uint64_t arg1;          /* first argument snapshot (for tracing) */
    uint64_t arg2;          /* second argument snapshot (for tracing) */
} kapi_unisys_context_t;

/* Unified syscall descriptor (table entry). */
typedef struct {
    int32_t  nr;            /* syscall number (index) */
    char     name[KAPI_UNISYS_NAME_MAX];
    kapi_unisys_handler_t handler;
    uint32_t flags;         /* KAPI_UNISYS_FLAG_* */
    uint32_t min_caps;      /* capability bits required to invoke */
    uint32_t reserved;
} kapi_unisys_desc_t;

/* Per-syscall performance statistics. */
typedef struct {
    uint64_t count;         /* total invocations */
    uint64_t total_cycles; /* cumulative rdtsc delta */
    uint64_t max_cycles;    /* worst-case latency */
    uint64_t min_cycles;    /* best-case latency */
    uint64_t error_count;   /* invocations returning < 0 */
    uint64_t last_errno;    /* errno of most recent failure */
} kapi_unisys_stats_t;

/*
 * Tracing hook. phase == 0 on entry, phase == 1 on exit.
 * On entry, retval/cycles are 0. On exit, cycles holds the rdtsc delta.
 */
typedef void (*kapi_unisys_trace_fn_t)(const kapi_unisys_context_t* ctx,
                                       const kapi_unisys_desc_t* desc,
                                       long retval, uint64_t cycles, int phase);

/* ----- Public API ----- */

/* Initialize the unified syscall table. Idempotent. Returns 0 on success. */
int  kapi_unisys_init(void);

/* Register a syscall handler at slot nr. Resets stats for nr. */
int  kapi_unisys_register(int nr, const char* name,
                          kapi_unisys_handler_t fn, uint32_t flags);

/* Unregister the syscall at slot nr. */
int  kapi_unisys_unregister(int nr);

/*
 * Invoke syscall nr through the unified path. Performs domain detection,
 * capability/flag enforcement, rdtsc latency accounting and tracing.
 * ctx may be NULL (treated as a kernel-internal call).
 */
long kapi_unisys_invoke(int nr, long a1, long a2, long a3, long a4,
                        long a5, long a6, const kapi_unisys_context_t* ctx);

/* Copy statistics for syscall nr into out. */
int  kapi_unisys_stats_get(int nr, kapi_unisys_stats_t* out);

/* Reset statistics for syscall nr. */
int  kapi_unisys_stats_reset(int nr);

/* Install / replace the global tracing hook. NULL disables tracing. */
void kapi_unisys_trace_set(kapi_unisys_trace_fn_t fn);

/* Resolve a positive errno value into a KAPI error code. */
int  kapi_unisys_errno_to_kapi(int err);

/* Detect the calling domain from the context (RPL of CS). */
int  kapi_unisys_detect_domain(const kapi_unisys_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* _KAPI_UNIFIED_SYSCALL_H */
