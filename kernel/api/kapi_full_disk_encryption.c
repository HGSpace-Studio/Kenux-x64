/*
 * Kenux Advanced OS Skeleton - Full Disk Encryption implementation
 *
 * Skeleton: volume table + lock/unlock state machine + key zeroize.
 * AES-XTS transforms and on-disk header I/O are TODO pending crypto
 * backend + blkdev layer integration.
 */

#include "kapi_full_disk_encryption.h"
#include "kapi.h"

#include <string.h>

static kapi_fde_volume_t kapi_fde_table[KAPI_FDE_MAX_VOLUMES];
static int kapi_fde_initialized = 0;

static kapi_fde_volume_t *fde_slot_alloc(void)
{
    for (int i = 0; i < KAPI_FDE_MAX_VOLUMES; i++) {
        if (!kapi_fde_table[i].registered) {
            return &kapi_fde_table[i];
        }
    }
    return NULL;
}

int kapi_fde_init(void)
{
    if (kapi_fde_initialized) {
        return KAPI_FDE_OK;
    }
    memset(kapi_fde_table, 0, sizeof(kapi_fde_table));
    kapi_fde_initialized = 1;
    return KAPI_FDE_OK;
}

void kapi_fde_exit(void)
{
    kapi_fde_zeroize_keys();
    memset(kapi_fde_table, 0, sizeof(kapi_fde_table));
    kapi_fde_initialized = 0;
}

int kapi_fde_format(kapi_blkdev_t dev, const char *name,
                    const char *passphrase)
{
    if (!dev || !name || !passphrase) {
        return KAPI_FDE_EINVAL;
    }
    /* TODO: derive master key from passphrase + write header to LBA0 */
    (void)passphrase;
    kapi_fde_volume_t *v = fde_slot_alloc();
    if (!v) {
        return KAPI_FDE_ENOMEM;
    }
    memset(v, 0, sizeof(*v));
    strncpy(v->name, name, KAPI_FDE_NAME_MAX - 1);
    v->backing_dev = dev;
    v->state = KAPI_FDE_STATE_LOCKED;
    v->registered = 1;
    return KAPI_FDE_OK;
}

int kapi_fde_add_volume(const char *name, kapi_blkdev_t dev)
{
    if (!name || !dev) {
        return KAPI_FDE_EINVAL;
    }
    if (kapi_fde_find(name)) {
        return KAPI_FDE_EEXIST;
    }
    kapi_fde_volume_t *v = fde_slot_alloc();
    if (!v) {
        return KAPI_FDE_ENOMEM;
    }
    memset(v, 0, sizeof(*v));
    strncpy(v->name, name, KAPI_FDE_NAME_MAX - 1);
    v->backing_dev = dev;
    v->state = KAPI_FDE_STATE_LOCKED;
    v->registered = 1;
    return KAPI_FDE_OK;
}

int kapi_fde_remove_volume(const char *name)
{
    kapi_fde_volume_t *v = kapi_fde_find(name);
    if (!v) {
        return KAPI_FDE_ENOENT;
    }
    if (v->state == KAPI_FDE_STATE_UNLOCKED) {
        return KAPI_FDE_EBUSY;
    }
    memset(v, 0, sizeof(*v));
    return KAPI_FDE_OK;
}

kapi_fde_volume_t *kapi_fde_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_FDE_MAX_VOLUMES; i++) {
        if (kapi_fde_table[i].registered &&
            strncmp(kapi_fde_table[i].name, name,
                    KAPI_FDE_NAME_MAX) == 0) {
            return &kapi_fde_table[i];
        }
    }
    return NULL;
}

int kapi_fde_unlock(const char *name, const char *passphrase)
{
    kapi_fde_volume_t *v = kapi_fde_find(name);
    if (!v || !passphrase) {
        return KAPI_FDE_ENOENT;
    }
    /* TODO: read header, derive key from passphrase, verify auth tag */
    (void)passphrase;
    v->state = KAPI_FDE_STATE_UNLOCKED;
    return KAPI_FDE_OK;
}

int kapi_fde_lock(const char *name)
{
    kapi_fde_volume_t *v = kapi_fde_find(name);
    if (!v) {
        return KAPI_FDE_ENOENT;
    }
    /* TODO: flush pending I/O, then zeroize master key in memory */
    memset(v->master_key, 0, sizeof(v->master_key));
    v->state = KAPI_FDE_STATE_LOCKED;
    return KAPI_FDE_OK;
}

int kapi_fde_change_passphrase(const char *name,
                               const char *old_pass,
                               const char *new_pass)
{
    kapi_fde_volume_t *v = kapi_fde_find(name);
    if (!v || !old_pass || !new_pass) {
        return KAPI_FDE_EINVAL;
    }
    /* TODO: verify old, derive new salt, re-encrypt master key in header */
    (void)old_pass; (void)new_pass;
    return KAPI_FDE_OK;
}

int kapi_fde_read(kapi_fde_volume_t *vol, uint64_t sector,
                  uint32_t count, void *buf)
{
    if (!vol || !buf) {
        return KAPI_FDE_EINVAL;
    }
    if (vol->state != KAPI_FDE_STATE_UNLOCKED) {
        return KAPI_FDE_EAUTH;
    }
    /* TODO: issue blkdev read on backing_dev at sector+1, then XTS-decrypt */
    (void)sector; (void)count;
    return KAPI_FDE_OK;
}

int kapi_fde_write(kapi_fde_volume_t *vol, uint64_t sector,
                   uint32_t count, const void *buf)
{
    if (!vol || !buf) {
        return KAPI_FDE_EINVAL;
    }
    if (vol->state != KAPI_FDE_STATE_UNLOCKED) {
        return KAPI_FDE_EAUTH;
    }
    /* TODO: XTS-encrypt buf, then issue blkdev write on backing_dev */
    (void)sector; (void)count;
    return KAPI_FDE_OK;
}

int kapi_fde_zeroize_keys(void)
{
    for (int i = 0; i < KAPI_FDE_MAX_VOLUMES; i++) {
        if (kapi_fde_table[i].registered) {
            memset(kapi_fde_table[i].master_key, 0,
                   sizeof(kapi_fde_table[i].master_key));
        }
    }
    return KAPI_FDE_OK;
}
