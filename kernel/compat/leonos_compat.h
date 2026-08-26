#ifndef KENUX_LEONOS_COMPAT_H
#define KENUX_LEONOS_COMPAT_H

#include <stdint.h>
#include <stddef.h>

#define LEONOS_COMPAT_ABI_VERSION 1u
#define LEONOS_COMPAT_SYSCALL_BASE 0x4C000

typedef enum {
    LEONOS_COMPAT_APP_UNKNOWN = 0,
    LEONOS_COMPAT_APP_NATIVE_ELF,
    LEONOS_COMPAT_APP_LEONOS_ELF,
} leonos_compat_app_type_t;

typedef struct {
    leonos_compat_app_type_t type;
    uint32_t abi_version;
    uint32_t supported_syscalls;
    char interpreter[128];
    char note[96];
} leonos_compat_info_t;

void leonos_compat_init(void);
int leonos_compat_detect_elf(const void* data, uint64_t size,
                             leonos_compat_info_t* out);
int leonos_compat_is_supported_syscall(int nr);
int leonos_compat_translate_syscall(int leon_nr);
long leonos_compat_dispatch_syscall(int leon_nr, long a1, long a2, long a3,
                                    long a4, long a5, long a6);
int leonos_compat_fill_env(char* buffer, uint32_t capacity);

#endif /* KENUX_LEONOS_COMPAT_H */
