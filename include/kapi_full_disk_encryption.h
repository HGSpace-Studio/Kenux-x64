#ifndef KAPI_FULL_DISK_ENCRYPTION_H
#define KAPI_FULL_DISK_ENCRYPTION_H

/*
 * Kenux Advanced OS Skeleton - Full Disk Encryption (FDE)
 *
 * Whole-disk / whole-partition transparent encryption with boot-time
 * key challenge and in-memory key zeroization. Layered on kapi_blkdev.
 * Skeleton: API + data structures.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_blkdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_FDE_NAME_MAX         32
#define KAPI_FDE_MAX_VOLUMES      16
#define KAPI_FDE_KEY_LEN          32   /* AES-256-XTS */
#define KAPI_FDE_SALT_LEN         16
#define KAPI_FDE_HEADER_OFFSET    0    /* LBA0 reserved for header */

typedef struct kapi_fde_volume kapi_fde_volume_t;

typedef enum {
    KAPI_FDE_OK             = 0,
    KAPI_FDE_EINVAL         = -1,
    KAPI_FDE_ENOMEM         = -2,
    KAPI_FDE_ENOENT        = -3,
    KAPI_FDE_EEXIST        = -4,
    KAPI_FDE_EKEY          = -5,
    KAPI_FDE_EIO           = -6,
    KAPI_FDE_EAUTH         = -7
} kapi_fde_err_t;

typedef enum {
    KAPI_FDE_STATE_LOCKED   = 0,
    KAPI_FDE_STATE_UNLOCKED = 1,
    KAPI_FDE_STATE_ERROR    = 2
} kapi_fde_state_t;

struct kapi_fde_volume {
    char           name[KAPI_FDE_NAME_MAX];
    kapi_blkdev_t  backing_dev;
    uint64_t       payload_sectors;   /* encrypted area */
    uint8_t        master_key[KAPI_FDE_KEY_LEN];
    uint8_t        salt[KAPI_FDE_SALT_LEN];
    kapi_fde_state_t state;
    int            registered;
};

/* Subsystem lifecycle */
int kapi_fde_init(void);
void kapi_fde_exit(void);

/* Volume management */
int kapi_fde_format(kapi_blkdev_t dev, const char *name,
                    const char *passphrase);
int kapi_fde_add_volume(const char *name, kapi_blkdev_t dev);
int kapi_fde_remove_volume(const char *name);
kapi_fde_volume_t *kapi_fde_find(const char *name);

/* Lock / unlock */
int kapi_fde_unlock(const char *name, const char *passphrase);
int kapi_fde_lock(const char *name);
int kapi_fde_change_passphrase(const char *name,
                               const char *old_pass,
                               const char *new_pass);

/* I/O path (transparent encrypt/decrypt on the backing device) */
int kapi_fde_read(kapi_fde_volume_t *vol, uint64_t sector,
                  uint32_t count, void *buf);
int kapi_fde_write(kapi_fde_volume_t *vol, uint64_t sector,
                   uint32_t count, const void *buf);

/* Secure erase of keys from memory */
int kapi_fde_zeroize_keys(void);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_FULL_DISK_ENCRYPTION_H */
