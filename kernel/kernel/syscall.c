#include "syscall.h"
#include <kapi_syscall.h>
#include <leonos_compat.h>
#include <memory.h>
#include <string.h>

static syscall_entry_t syscall_table[SYSCALL_MAX];

void syscall_init(void)
{
    memset(syscall_table, 0, sizeof(syscall_table));

    /* KAPI syscall table belongs to kapi_init().
     * Do not initialize it here too: kapi_syscall_init() clears and rebuilds
     * the table, so calling it from both syscall_init() and kapi_init() makes
     * the low-level syscall layer and KAPI layer fight over ownership. */
    leonos_compat_init();
}

void syscall_register(int num, void* handler)
{
    if (num >= SYSCALL_MAX) {
        return;
    }

    syscall_table[num].handler = handler;
}

void* syscall_get(int num)
{
    if (num >= SYSCALL_MAX) {
        return NULL;
    }

    return syscall_table[num].handler;
}

long kenux_syscall_entry(int nr, long a1, long a2, long a3, long a4, long a5, long a6)
{
    if (nr >= LEONOS_COMPAT_SYSCALL_BASE &&
        nr < LEONOS_COMPAT_SYSCALL_BASE + SYSCALL_MAX) {
        return leonos_compat_dispatch_syscall(nr - LEONOS_COMPAT_SYSCALL_BASE,
                                              a1, a2, a3, a4, a5, a6);
    }

    if (nr >= 0 && nr < KAPI_SYSCALL_COUNT) {
        return kapi_syscall_dispatch(nr, a1, a2, a3, a4, a5, a6);
    }

    if (nr >= 0 && nr < SYSCALL_MAX && syscall_table[nr].handler) {
        kapi_syscall_fn_t fn = (kapi_syscall_fn_t)syscall_table[nr].handler;
        return fn(a1, a2, a3, a4, a5, a6);
    }

    return -38;
}
