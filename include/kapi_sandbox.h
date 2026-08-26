#ifndef _KAPI_SANDBOX_H
#define _KAPI_SANDBOX_H

/*
 * Kenux KAPI - Cross-domain Security Sandbox & Privilege Dropping
 *
 * Provides seccomp-like syscall filtering (whitelist/blacklist), Linux
 * capabilities-like 64-bit capability bitmaps, privilege dropping,
 * namespace isolation flags (filesystem / network / PID) and a sandbox
 * violation audit callback.
 *
 * Freestanding kernel code: no standard library, no dynamic allocation.
 */

#include <stdint.h>
#include <stddef.h>

/* Errno used for sandbox denials. Mirrors kernel/lib/libc/errno.h. */
#ifndef EPERM
#define EPERM 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Limits ----- */
#define KAPI_SANDBOX_PROCESS_MAX    256                       /* per-process filter slots */
#define KAPI_SANDBOX_SYSCALL_NR_MAX 1024                      /* highest filterable syscall nr */
#define KAPI_SANDBOX_SYSCALL_WORDS  (KAPI_SANDBOX_SYSCALL_NR_MAX / 64)
#define KAPI_SANDBOX_CAP_MAX        64                        /* 64-bit capability set */

/* ----- Filter mode ----- */
#define KAPI_SANDBOX_MODE_DISABLED   0x00u                    /* filter inactive */
#define KAPI_SANDBOX_MODE_WHITELIST  0x01u                    /* allow only listed syscalls */
#define KAPI_SANDBOX_MODE_BLACKLIST  0x02u                    /* deny listed syscalls */

/* ----- Filter flags ----- */
#define KAPI_SANDBOX_F_ACTIVE         0x00000001u             /* slot is in use */
#define KAPI_SANDBOX_F_NO_NEW_PRIVS   0x00000002u             /* SECBIT_NO_NEW_PRIVS-like */
#define KAPI_SANDBOX_F_AUDIT_ALLOW    0x00000004u             /* audit allowed syscalls too */
#define KAPI_SANDBOX_F_AUDIT_DENY     0x00000008u             /* audit denied syscalls */
#define KAPI_SANDBOX_F_LOCKED         0x00000010u             /* filter is immutable */

/* ----- Namespace + isolation flags ----- */
#define KAPI_SANDBOX_NS_MOUNT         0x00000001u
#define KAPI_SANDBOX_NS_PID           0x00000002u
#define KAPI_SANDBOX_NS_NET           0x00000004u
#define KAPI_SANDBOX_NS_IPC           0x00000008u
#define KAPI_SANDBOX_NS_UTS           0x00000010u
#define KAPI_SANDBOX_NS_USER          0x00000020u
#define KAPI_SANDBOX_NS_CGROUP        0x00000040u
#define KAPI_SANDBOX_ISOLATE_FS       0x00000100u             /* filesystem isolation */
#define KAPI_SANDBOX_ISOLATE_NET     0x00000200u             /* network isolation */
#define KAPI_SANDBOX_ISOLATE_PID     0x00000400u             /* PID isolation */
#define KAPI_SANDBOX_NS_MASK         0x000007FFu

/* ----- Capabilities (Linux-compatible numbering, 0..63) ----- */
#define KAPI_CAP_CHOWN               0
#define KAPI_CAP_DAC_OVERRIDE        1
#define KAPI_CAP_DAC_READ_SEARCH     2
#define KAPI_CAP_FOWNER              3
#define KAPI_CAP_FSETID              4
#define KAPI_CAP_KILL                5
#define KAPI_CAP_SETGID              6
#define KAPI_CAP_SETUID              7
#define KAPI_CAP_SETPCAP             8
#define KAPI_CAP_LINUX_IMMUTABLE     9
#define KAPI_CAP_NET_BIND_SERVICE    10
#define KAPI_CAP_NET_BROADCAST       11
#define KAPI_CAP_NET_ADMIN           12
#define KAPI_CAP_NET_RAW             13
#define KAPI_CAP_IPC_LOCK            14
#define KAPI_CAP_IPC_OWNER           15
#define KAPI_CAP_SYS_MODULE          16
#define KAPI_CAP_SYS_RAWIO           17
#define KAPI_CAP_SYS_CHROOT          18
#define KAPI_CAP_SYS_PTRACE          19
#define KAPI_CAP_SYS_PACCT           20
#define KAPI_CAP_SYS_ADMIN           21
#define KAPI_CAP_SYS_BOOT            22
#define KAPI_CAP_SYS_NICE            23
#define KAPI_CAP_SYS_RESOURCE        24
#define KAPI_CAP_SYS_TIME            25
#define KAPI_CAP_SYS_TTY_CONFIG      26
#define KAPI_CAP_MKNOD               27
#define KAPI_CAP_LEASE               28
#define KAPI_CAP_AUDIT_WRITE         29
#define KAPI_CAP_AUDIT_CONTROL       30
#define KAPI_CAP_SETFCAP             31
#define KAPI_CAP_MAC_OVERRIDE        32
#define KAPI_CAP_MAC_ADMIN           33
#define KAPI_CAP_SYSLOG              34
#define KAPI_CAP_WAKE_ALARM          35
#define KAPI_CAP_BLOCK_SUSPEND       36
#define KAPI_CAP_AUDIT_READ          37
#define KAPI_CAP_LAST                KAPI_CAP_AUDIT_READ
#define KAPI_SANDBOX_CAP_FULL        ((uint64_t)(~((uint64_t)0)))
#define KAPI_SANDBOX_CAP_NONE        ((uint64_t)0)

/* Capability bit helpers (cap must be 0..63). */
#define KAPI_SANDBOX_CAP_BIT(cap)    (1ULL << (cap))
#define KAPI_SANDBOX_CAP_HAS(set, cap) \
    (((set) & KAPI_SANDBOX_CAP_BIT(cap)) != 0)

/* ----- Audit reasons ----- */
#define KAPI_SANDBOX_AUDIT_ALLOW             0
#define KAPI_SANDBOX_AUDIT_SYSCALL_BLOCKED   1
#define KAPI_SANDBOX_AUDIT_CAP_DENIED        2
#define KAPI_SANDBOX_AUDIT_NS_VIOLATION      3
#define KAPI_SANDBOX_AUDIT_UNKNOWN_PID        4

/* Audit callback. Called for every audited event. detail may be NULL. */
typedef void (*kapi_sandbox_audit_fn_t)(int32_t pid, int syscall_nr,
                                        int reason, const char* detail);

/*
 * Per-process sandbox filter. The syscall_mask meaning depends on mode:
 *  - whitelist: bits set are the only syscalls allowed
 *  - blacklist: bits set are syscalls denied
 */
typedef struct {
    int32_t   pid;
    uint32_t  mode;                                      /* whitelist / blacklist */
    uint32_t  flags;                                     /* KAPI_SANDBOX_F_* */
    uint64_t  syscall_mask[KAPI_SANDBOX_SYSCALL_WORDS];  /* allowed or denied set */
    uint64_t  cap_effective;                             /* capabilities currently granted */
    uint64_t  cap_permitted;                             /* capabilities raisable */
    uint64_t  cap_inheritable;                           /* capabilities inheritable across exec */
    uint32_t  ns_flags;                                  /* namespace + isolation flags */
    uint32_t  audit_flags;                               /* KAPI_SANDBOX_F_AUDIT_* */
    uint64_t  violations;                                /* running violation counter */
} kapi_sandbox_filter_t;

/*
 * Policy bundle installed onto a process. Resolved into a filter at
 * install time.
 */
typedef struct {
    uint32_t  mode;                                      /* whitelist / blacklist */
    uint32_t  flags;                                     /* KAPI_SANDBOX_F_* initial flags */
    uint64_t  syscall_whitelist[KAPI_SANDBOX_SYSCALL_WORDS];
    uint64_t  syscall_blacklist[KAPI_SANDBOX_SYSCALL_WORDS];
    uint64_t  cap_keep_mask;                             /* capabilities to retain */
    uint64_t  cap_drop_mask;                             /* capabilities to drop */
    uint32_t  ns_flags;                                  /* namespace + isolation flags */
    uint32_t  audit_flags;                               /* KAPI_SANDBOX_F_AUDIT_* */
} kapi_sandbox_policy_t;

/* ----- Public API ----- */

/* Initialize the sandbox subsystem. Idempotent. Returns 0 on success. */
int  kapi_sandbox_init(void);

/* Install a policy as the active filter for pid. Replaces an existing one. */
int  kapi_sandbox_install_filter(int32_t pid, const kapi_sandbox_policy_t* policy);

/* Remove the filter for pid (sandbox no longer applies). */
int  kapi_sandbox_remove_filter(int32_t pid);

/* Drop capabilities in drop_mask from pid's effective+permitted sets. */
int  kapi_sandbox_drop_caps(int32_t pid, uint64_t drop_mask);

/* Raise capabilities in raise_mask into pid's effective set (if permitted). */
int  kapi_sandbox_raise_caps(int32_t pid, uint64_t raise_mask);

/*
 * Check whether pid may invoke syscall_nr.
 * Returns 0 if allowed, -EPERM if denied by the sandbox.
 * No filter for pid means the call is allowed (legacy process).
 */
int  kapi_sandbox_check_syscall(int32_t pid, int syscall_nr);

/*
 * Check whether pid holds capability cap.
 * Returns 0 if held, -EPERM if denied, -KAPI_EINVAL if no filter / bad cap.
 */
int  kapi_sandbox_cap_check(int32_t pid, int cap);

/* Emit an audit record (callback + kernel log) for a sandbox event. */
int  kapi_sandbox_audit(int32_t pid, int syscall_nr, int reason,
                        const char* detail);

/* Register the audit callback. NULL disables callback notification. */
void kapi_sandbox_set_audit_cb(kapi_sandbox_audit_fn_t cb);

/* Lookup the live filter for pid (read-only; NULL if none). */
const kapi_sandbox_filter_t* kapi_sandbox_filter_get(int32_t pid);

#ifdef __cplusplus
}
#endif

#endif /* _KAPI_SANDBOX_H */
