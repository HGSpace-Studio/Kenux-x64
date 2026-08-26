#ifndef ARCH_X86_64_TYPES_H
#define ARCH_X86_64_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int32_t pid_t;
typedef int32_t uid_t;
typedef int32_t gid_t;
typedef int64_t off_t;
typedef uint64_t mode_t;
typedef uint64_t dev_t;
typedef uint64_t ino_t;
typedef uint64_t nlink_t;
typedef uint64_t useconds_t;
typedef int64_t time_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

#ifndef NULL
#define NULL ((void*)0)
#endif

#define ARCH_X86_64

#endif
