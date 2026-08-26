

#include <arch/types.h>
#include <arch/process.h>
#include <arch/memory.h>
#include <string.h>
#include <slab.h>

#define CAP_CHOWN            0
#define CAP_DAC_OVERRIDE     1
#define CAP_DAC_READ_SEARCH  2
#define CAP_FOWNER           3
#define CAP_FSETID           4
#define CAP_KILL             5
#define CAP_SETGID           6
#define CAP_SETUID           7
#define CAP_SETPCAP          8
#define CAP_LINUX_IMMUTABLE  9
#define CAP_NET_BIND_SERVICE 10
#define CAP_NET_BROADCAST    11
#define CAP_NET_ADMIN        12
#define CAP_NET_RAW          13
#define CAP_IPC_LOCK         14
#define CAP_IPC_OWNER        15
#define CAP_SYS_MODULE       16
#define CAP_SYS_RAWIO        17
#define CAP_SYS_CHROOT       18
#define CAP_SYS_PTRACE       19
#define CAP_SYS_PACCT        20
#define CAP_SYS_ADMIN        21
#define CAP_SYS_BOOT         22
#define CAP_SYS_NICE         23
#define CAP_SYS_RESOURCE     24
#define CAP_SYS_TIME         25
#define CAP_SYS_TTY_CONFIG   26
#define CAP_MKNOD            27
#define CAP_LEASE            28
#define CAP_AUDIT_WRITE      29
#define CAP_AUDIT_CONTROL    30
#define CAP_SETFCAP          31
#define CAP_MAX              32

typedef struct {
    uint64_t caps[1];
} cap_set_t;

static inline int cap_test(const cap_set_t* set, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return 0;
    return (set->caps[cap / 64] & (1ULL << (cap % 64))) != 0;
}

static inline void cap_set(cap_set_t* set, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return;
    set->caps[cap / 64] |= (1ULL << (cap % 64));
}

static inline void cap_clear(cap_set_t* set, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return;
    set->caps[cap / 64] &= ~(1ULL << (cap % 64));
}

static inline void cap_clear_all(cap_set_t* set)
{
    set->caps[0] = 0;
}

static inline void cap_set_all(cap_set_t* set)
{
    set->caps[0] = 0xFFFFFFFFFFFFFFFFULL;
}

typedef struct {
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    uint32_t suid;
    uint32_t sgid;
    uint32_t ruid;
    uint32_t rgid;
    uint32_t fsuid;
    uint32_t fsgid;

    cap_set_t permitted;
    cap_set_t effective;
    cap_set_t inheritable;
    cap_set_t ambient;

    uint32_t seccomp_mode;
} cred_t;

static cred_t init_cred;

void security_init(void)
{
    memset(&init_cred, 0, sizeof(init_cred));
    init_cred.uid = init_cred.euid = init_cred.suid = init_cred.fsuid = 0;
    init_cred.gid = init_cred.egid = init_cred.sgid = init_cred.fsgid = 0;
    cap_set_all(&init_cred.permitted);
    cap_set_all(&init_cred.effective);
    cap_set_all(&init_cred.inheritable);
    cap_set_all(&init_cred.ambient);
    init_cred.seccomp_mode = 0;
}

int security_check_cap(process_t* proc, int cap)
{
    if (!proc || !proc->cred) return -1;
    cred_t* cred = (cred_t*)proc->cred;

    if (cred->euid == 0) return 0;

    if (cap_test(&cred->effective, cap)) return 0;

    return -1;
}

int security_setuid(process_t* proc, uint32_t uid)
{
    if (!proc || !proc->cred) return -1;
    cred_t* cred = (cred_t*)proc->cred;

    if (cred->euid == 0) {

        cred->uid = cred->euid = cred->suid = uid;
        if (uid != 0) {
            cap_clear_all(&cred->effective);
            cap_clear_all(&cred->permitted);
        }
    } else if (cred->ruid == uid || cred->suid == uid) {
        cred->euid = uid;
    } else {
        return -1;
    }
    return 0;
}

int security_setgid(process_t* proc, uint32_t gid)
{
    if (!proc || !proc->cred) return -1;
    cred_t* cred = (cred_t*)proc->cred;

    if (cred->euid == 0) {
        cred->gid = cred->egid = cred->sgid = gid;
    } else if (cred->rgid == gid || cred->sgid == gid) {
        cred->egid = gid;
    } else {
        return -1;
    }
    return 0;
}

#define ASLR_ENTROPY_BITS   16
#define ASLR_BASE_OFFSET    (1ULL << 12)

static uint32_t aslr_random_seed = 0x12345678;

static uint32_t __rand(void)
{
    aslr_random_seed = aslr_random_seed * 1103515245 + 12345;
    return aslr_random_seed;
}

uint64_t security_randomize_base(uint64_t base, uint64_t range_bits)
{
    if (range_bits > ASLR_ENTROPY_BITS) range_bits = ASLR_ENTROPY_BITS;
    uint64_t mask = (1ULL << range_bits) - 1;
    uint64_t offset = (__rand() & mask) * PAGE_SIZE;
    return base + offset;
}

uint64_t security_randomize_stack(void)
{
    return security_randomize_base(0x00007FFFFFF00000ULL, ASLR_ENTROPY_BITS);
}

uint64_t security_randomize_mmap(void)
{
    return security_randomize_base(0x00007F0000000000ULL, ASLR_ENTROPY_BITS);
}

#define SECCOMP_MODE_DISABLED   0
#define SECCOMP_MODE_STRICT     1
#define SECCOMP_MODE_FILTER     2

int security_seccomp_set_mode(process_t* proc, uint32_t mode)
{
    if (!proc || !proc->cred) return -1;
    if (mode > SECCOMP_MODE_FILTER) return -1;

    if (security_check_cap(proc, CAP_SYS_ADMIN) != 0) {
        return -1;
    }

    ((cred_t*)proc->cred)->seccomp_mode = mode;
    return 0;
}

int security_seccomp_check(process_t* proc, uint64_t syscall_no)
{
    if (!proc || !proc->cred) return -1;

    switch (((cred_t*)proc->cred)->seccomp_mode) {
        case SECCOMP_MODE_DISABLED:
            return 0;
        case SECCOMP_MODE_STRICT:

            if (syscall_no == 0 || syscall_no == 1 || syscall_no == 60 || syscall_no == 15) {
                return 0;
            }
            return -1;
        case SECCOMP_MODE_FILTER:

            return 0;
    }
    return 0;
}

void security_enable_smep_smap(void)
{
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));

    cr4 |= (1ULL << 20) | (1ULL << 21);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
}

int security_smep_enabled(void)
{
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    return (cr4 & (1ULL << 20)) != 0;
}

int security_smap_enabled(void)
{
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    return (cr4 & (1ULL << 21)) != 0;
}

void security_init_cred(process_t* proc)
{
    if (!proc) return;
    proc->cred = kzalloc(sizeof(cred_t));
    if (!proc->cred) return;
    memcpy(proc->cred, &init_cred, sizeof(cred_t));
}

void security_copy_cred(process_t* dst, const process_t* src)
{
    if (!dst || !src || !src->cred) return;
    if (!dst->cred) {
        dst->cred = kzalloc(sizeof(cred_t));
        if (!dst->cred) return;
    }
    memcpy(dst->cred, src->cred, sizeof(cred_t));
}
