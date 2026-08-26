#include "leonos_compat.h"

#include <arch/elf.h>
#include <kapi_syscall.h>
#include <string.h>

static uint32_t s_leonos_compat_ready;

static uint32_t compat_strlen(const char* s) {
    uint32_t len = 0;
    if (!s) return 0;
    while (s[len]) len++;
    return len;
}

static int compat_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle || !needle[0]) return 0;
    uint32_t hlen = compat_strlen(haystack);
    uint32_t nlen = compat_strlen(needle);
    if (nlen > hlen) return 0;
    for (uint32_t i = 0; i + nlen <= hlen; i++) {
        uint32_t j = 0;
        while (j < nlen && haystack[i + j] == needle[j]) j++;
        if (j == nlen) return 1;
    }
    return 0;
}

static void compat_copy(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0;
    if (!dst || cap == 0) return;
    while (src && src[i] && i + 1u < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void leonos_compat_init(void) {
    s_leonos_compat_ready = 1;
}

int leonos_compat_detect_elf(const void* data, uint64_t size,
                             leonos_compat_info_t* out) {
    if (!data || !out || size < sizeof(Elf64_Ehdr)) return 0;
    memset(out, 0, sizeof(*out));

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;
    if (elf_verify(ehdr) != 0) return 0;

    out->type = LEONOS_COMPAT_APP_NATIVE_ELF;
    out->abi_version = LEONOS_COMPAT_ABI_VERSION;
    compat_copy(out->note, sizeof(out->note), "Kenux native ELF");

    if (ehdr->e_phoff && ehdr->e_phnum && ehdr->e_phentsize == sizeof(Elf64_Phdr)) {
        const Elf64_Phdr* phdrs = (const Elf64_Phdr*)((const uint8_t*)data + ehdr->e_phoff);
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            const Elf64_Phdr* ph = &phdrs[i];
            if (ph->p_type == PT_INTERP && ph->p_offset + ph->p_filesz <= size) {
                uint64_t len = ph->p_filesz;
                if (len >= sizeof(out->interpreter)) len = sizeof(out->interpreter) - 1u;
                memcpy(out->interpreter, (const uint8_t*)data + ph->p_offset, (size_t)len);
                out->interpreter[len] = 0;
                if (compat_contains(out->interpreter, "leonos") ||
                    compat_contains(out->interpreter, "osmlayer")) {
                    out->type = LEONOS_COMPAT_APP_LEONOS_ELF;
                    compat_copy(out->note, sizeof(out->note),
                                "LeonOS interpreter detected");
                }
            }
        }
    }

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t scan = size < (256u * 1024u) ? size : (256u * 1024u);
    for (uint64_t i = 0; i + 6u < scan; i++) {
        if ((bytes[i] == 'L' || bytes[i] == 'l') &&
            i + 6u < scan &&
            (bytes[i + 1] == 'E' || bytes[i + 1] == 'e') &&
            (bytes[i + 2] == 'O' || bytes[i + 2] == 'o') &&
            (bytes[i + 3] == 'N' || bytes[i + 3] == 'n') &&
            (bytes[i + 4] == 'O' || bytes[i + 4] == 'o') &&
            (bytes[i + 5] == 'S' || bytes[i + 5] == 's')) {
            out->type = LEONOS_COMPAT_APP_LEONOS_ELF;
            compat_copy(out->note, sizeof(out->note), "LeonOS marker detected");
            break;
        }
    }

    out->supported_syscalls = 20;
    return 1;
}

int leonos_compat_translate_syscall(int leon_nr) {
    switch (leon_nr) {
        case 0: return SYS_read;
        case 1: return SYS_write;
        case 2: return SYS_open;
        case 3: return SYS_close;
        case 4: return SYS_stat;
        case 5: return SYS_fstat;
        case 8: return SYS_lseek;
        case 9: return SYS_mmap;
        case 11: return SYS_munmap;
        case 16: return SYS_ioctl;
        case 24: return SYS_sched_yield;
        case 35: return SYS_nanosleep;
        case 39: return SYS_getpid;
        case 59: return SYS_execve;
        case 60: return SYS_exit;
        case 61: return SYS_wait4;
        case 79: return SYS_getcwd;
        case 80: return SYS_chdir;
        case 82: return SYS_rename;
        case 83: return SYS_mkdir;
        case 84: return SYS_rmdir;
        case 87: return SYS_unlink;
        default: return -1;
    }
}

int leonos_compat_is_supported_syscall(int nr) {
    return leonos_compat_translate_syscall(nr) >= 0;
}

long leonos_compat_dispatch_syscall(int leon_nr, long a1, long a2, long a3,
                                    long a4, long a5, long a6) {
    if (!s_leonos_compat_ready) leonos_compat_init();
    int kenux_nr = leonos_compat_translate_syscall(leon_nr);
    if (kenux_nr < 0) return -38;
    return kapi_syscall_dispatch(kenux_nr, a1, a2, a3, a4, a5, a6);
}

int leonos_compat_fill_env(char* buffer, uint32_t capacity) {
    static const char env[] =
        "KENUX_LEONOS_COMPAT=1\n"
        "LEONOS_ABI=1\n"
        "LEONOS_ROOT=/\n"
        "LEONOS_DISPLAY=kenux-gui\n";
    uint32_t len = sizeof(env);
    if (!buffer || capacity < len) return -1;
    memcpy(buffer, env, len);
    return 0;
}
