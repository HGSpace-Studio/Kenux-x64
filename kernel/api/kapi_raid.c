/*
 * Kenux Advanced OS Skeleton - Software RAID implementation
 *
 * Skeleton: array/disk tables, status tracking, fault reporting. Stripe,
 * mirror, and parity I/O dispatch are TODO pending the blkdev layer.
 */

#include "kapi_raid.h"
#include "kapi.h"

#include <string.h>

static kapi_raid_array_t kapi_raid_table[KAPI_RAID_MAX_ARRAYS];
static int kapi_raid_initialized = 0;

static kapi_raid_array_t *raid_slot_alloc(void)
{
    for (int i = 0; i < KAPI_RAID_MAX_ARRAYS; i++) {
        if (!kapi_raid_table[i].active) {
            return &kapi_raid_table[i];
        }
    }
    return NULL;
}

int kapi_raid_init(void)
{
    if (kapi_raid_initialized) {
        return KAPI_RAID_OK;
    }
    memset(kapi_raid_table, 0, sizeof(kapi_raid_table));
    kapi_raid_initialized = 1;
    return KAPI_RAID_OK;
}

void kapi_raid_exit(void)
{
    memset(kapi_raid_table, 0, sizeof(kapi_raid_table));
    kapi_raid_initialized = 0;
}

int kapi_raid_create(const char *name, kapi_raid_level_t level,
                     uint32_t chunk_size, const kapi_blkdev_t *disks,
                     uint32_t nr_disks)
{
    if (!name || !disks || nr_disks == 0) {
        return KAPI_RAID_EINVAL;
    }
    if (kapi_raid_find(name)) {
        return KAPI_RAID_EEXIST;
    }
    /* Minimum disk count per level */
    uint32_t min_disks = (level == KAPI_RAID_LEVEL_0) ? 1 :
                         (level == KAPI_RAID_LEVEL_1) ? 2 :
                         (level == KAPI_RAID_LEVEL_5) ? 3 :
                         (level == KAPI_RAID_LEVEL_6) ? 4 :
                         (level == KAPI_RAID_LEVEL_10) ? 4 : 0;
    if (min_disks == 0) {
        return KAPI_RAID_EINVAL;
    }
    if (nr_disks < min_disks) {
        return KAPI_RAID_EINVAL;
    }
    if (nr_disks > KAPI_RAID_MAX_DISKS) {
        return KAPI_RAID_ENOSPC;
    }
    kapi_raid_array_t *a = raid_slot_alloc();
    if (!a) {
        return KAPI_RAID_ENOMEM;
    }
    memset(a, 0, sizeof(*a));
    strncpy(a->name, name, KAPI_RAID_NAME_MAX - 1);
    a->level = level;
    a->chunk_size = chunk_size ? chunk_size : KAPI_RAID_CHUNK_SIZE;
    a->nr_disks = nr_disks;
    for (uint32_t i = 0; i < nr_disks; i++) {
        a->disks[i].dev = disks[i];
        a->disks[i].index = (int)i;
        a->disks[i].state = KAPI_RAID_DISK_OK;
    }
    /* TODO: compute capacity per level */
    a->active = 1;
    return KAPI_RAID_OK;
}

int kapi_raid_destroy(const char *name)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a) {
        return KAPI_RAID_ENOENT;
    }
    memset(a, 0, sizeof(*a));
    return KAPI_RAID_OK;
}

int kapi_raid_assemble(const char *name, const kapi_blkdev_t *disks,
                       uint32_t nr_disks)
{
    (void)name; (void)disks; (void)nr_disks;
    /* TODO: read on-disk superblock and match member disks */
    return KAPI_RAID_ENOTSUP;
}

int kapi_raid_stop(const char *name)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a) {
        return KAPI_RAID_ENOENT;
    }
    a->active = 0;
    return KAPI_RAID_OK;
}

kapi_raid_array_t *kapi_raid_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_RAID_MAX_ARRAYS; i++) {
        if (kapi_raid_table[i].active &&
            strncmp(kapi_raid_table[i].name, name,
                    KAPI_RAID_NAME_MAX) == 0) {
            return &kapi_raid_table[i];
        }
    }
    return NULL;
}

int kapi_raid_add_disk(const char *name, kapi_blkdev_t dev, int spare)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a) {
        return KAPI_RAID_ENOENT;
    }
    if (a->nr_disks + a->nr_spares >= KAPI_RAID_MAX_DISKS) {
        return KAPI_RAID_ENOSPC;
    }
    kapi_raid_disk_t *d = &a->disks[a->nr_disks + a->nr_spares];
    d->dev = dev;
    d->index = -1;
    d->state = spare ? KAPI_RAID_DISK_SPARE : KAPI_RAID_DISK_OK;
    if (spare) {
        a->nr_spares++;
    } else {
        d->index = (int)a->nr_disks++;
    }
    return KAPI_RAID_OK;
}

int kapi_raid_remove_disk(const char *name, int index)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a || index < 0 || index >= (int)(a->nr_disks + a->nr_spares)) {
        return KAPI_RAID_ENOENT;
    }
    /* TODO: flush + wait for outstanding I/O */
    a->disks[index].state = KAPI_RAID_DISK_MISSING;
    return KAPI_RAID_OK;
}

int kapi_raid_mark_faulty(const char *name, int index)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a || index < 0 || index >= (int)(a->nr_disks + a->nr_spares)) {
        return KAPI_RAID_ENOENT;
    }
    a->disks[index].state = KAPI_RAID_DISK_FAULTY;
    a->degraded = 1;
    return KAPI_RAID_OK;
}

int kapi_raid_rebuild(const char *name)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a) {
        return KAPI_RAID_ENOENT;
    }
    /* TODO: find faulty+spare pair and resync */
    return KAPI_RAID_OK;
}

int kapi_raid_read(kapi_raid_array_t *array, uint64_t sector,
                   uint32_t count, void *buf)
{
    if (!array || !buf) {
        return KAPI_RAID_EINVAL;
    }
    /* TODO: stripe/mirror/parity dispatch by array->level */
    (void)sector; (void)count;
    return KAPI_RAID_OK;
}

int kapi_raid_write(kapi_raid_array_t *array, uint64_t sector,
                    uint32_t count, const void *buf)
{
    if (!array || !buf) {
        return KAPI_RAID_EINVAL;
    }
    /* TODO: stripe/mirror/parity dispatch by array->level */
    (void)sector; (void)count;
    return KAPI_RAID_OK;
}

int kapi_raid_get_status(const char *name, char *buf, size_t len)
{
    kapi_raid_array_t *a = kapi_raid_find(name);
    if (!a || !buf) {
        return KAPI_RAID_EINVAL;
    }
    /* TODO: format a sysfs-style status string */
    if (len < 16) {
        return KAPI_RAID_ENOSPC;
    }
    strncpy(buf, "active/degraded", len - 1);
    buf[len - 1] = '\0';
    return KAPI_RAID_OK;
}

int kapi_raid_list_arrays(char *buf, size_t len)
{
    if (!buf) {
        return KAPI_RAID_EINVAL;
    }
    size_t off = 0;
    for (int i = 0; i < KAPI_RAID_MAX_ARRAYS; i++) {
        if (kapi_raid_table[i].active) {
            size_t need = strlen(kapi_raid_table[i].name) + 2;
            if (off + need > len) {
                return KAPI_RAID_ENOSPC;
            }
            off += (size_t)snprintf(buf + off, len - off, "%s ",
                                    kapi_raid_table[i].name);
        }
    }
    return KAPI_RAID_OK;
}
