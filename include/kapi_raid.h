#ifndef KAPI_RAID_H
#define KAPI_RAID_H

/*
 * Kenux Advanced OS Skeleton - Software RAID (md-style)
 *
 * Supports RAID 0/1/5/6 with online rebuild and fault reporting.
 * Layered on top of kapi_blkdev member disks. Skeleton: API only.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_blkdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_RAID_NAME_MAX        64
#define KAPI_RAID_MAX_DISKS       32
#define KAPI_RAID_MAX_ARRAYS      32
#define KAPI_RAID_CHUNK_SIZE      (64 * 1024)

typedef struct kapi_raid_array kapi_raid_array_t;

typedef enum {
    KAPI_RAID_OK            = 0,
    KAPI_RAID_EINVAL        = -1,
    KAPI_RAID_ENOMEM        = -2,
    KAPI_RAID_ENOENT        = -3,
    KAPI_RAID_EEXIST        = -4,
    KAPI_RAID_EBUSY         = -5,
    KAPI_RAID_ENOSPC        = -6,
    KAPI_RAID_EDEGRADED    = -7
} kapi_raid_err_t;

typedef enum {
    KAPI_RAID_LEVEL_0   = 0,   /* stripe      */
    KAPI_RAID_LEVEL_1   = 1,   /* mirror      */
    KAPI_RAID_LEVEL_5   = 5,   /* distributed parity */
    KAPI_RAID_LEVEL_6   = 6,   /* dual parity */
    KAPI_RAID_LEVEL_10  = 10   /* mirror+stripe */
} kapi_raid_level_t;

typedef enum {
    KAPI_RAID_DISK_OK       = 0,
    KAPI_RAID_DISK_FAULTY  = 1,
    KAPI_RAID_DISK_SPARE   = 2,
    KAPI_RAID_DISK_REBUILD = 3,
    KAPI_RAID_DISK_MISSING = 4
} kapi_raid_disk_state_t;

typedef struct {
    kapi_blkdev_t           dev;
    int                     index;
    kapi_raid_disk_state_t  state;
    uint64_t                rebuild_pos;
} kapi_raid_disk_t;

struct kapi_raid_array {
    char             name[KAPI_RAID_NAME_MAX];
    kapi_raid_level_t level;
    uint32_t         chunk_size;
    uint32_t         nr_disks;
    uint32_t         nr_spares;
    uint64_t         total_sectors;
    uint64_t         used_sectors;
    int              active;
    int              degraded;
    kapi_raid_disk_t disks[KAPI_RAID_MAX_DISKS];
};

/* Subsystem lifecycle */
int kapi_raid_init(void);
void kapi_raid_exit(void);

/* Array management */
int kapi_raid_create(const char *name, kapi_raid_level_t level,
                     uint32_t chunk_size, const kapi_blkdev_t *disks,
                     uint32_t nr_disks);
int kapi_raid_destroy(const char *name);
int kapi_raid_assemble(const char *name, const kapi_blkdev_t *disks,
                       uint32_t nr_disks);
int kapi_raid_stop(const char *name);
kapi_raid_array_t *kapi_raid_find(const char *name);

/* Disk operations */
int kapi_raid_add_disk(const char *name, kapi_blkdev_t dev, int spare);
int kapi_raid_remove_disk(const char *name, int index);
int kapi_raid_mark_faulty(const char *name, int index);
int kapi_raid_rebuild(const char *name);

/* I/O on array (delegates to member disks per level) */
int kapi_raid_read(kapi_raid_array_t *array, uint64_t sector,
                   uint32_t count, void *buf);
int kapi_raid_write(kapi_raid_array_t *array, uint64_t sector,
                    uint32_t count, const void *buf);

/* Status / sysfs-style */
int kapi_raid_get_status(const char *name, char *buf, size_t len);
int kapi_raid_list_arrays(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_RAID_H */
