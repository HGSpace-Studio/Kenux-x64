#ifndef KAPI_LVM_H
#define KAPI_LVM_H

/*
 * Kenux Advanced OS Skeleton - Logical Volume Manager (LVM)
 *
 * Provides physical volume / volume group / logical volume abstractions
 * layered on top of kapi_blkdev. Supports online resize and snapshot
 * creation via copy-on-write. Skeleton: API surface only.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_blkdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_LVM_NAME_MAX        128
#define KAPI_LVM_UUID_LEN        32
#define KAPI_LVM_MAX_PVS          256
#define KAPI_LVM_MAX_VGS          32
#define KAPI_LVM_MAX_LVS_PER_VG   128
#define KAPI_LVM_SECTOR_SIZE      512

typedef struct kapi_pv  kapi_pv_t;
typedef struct kapi_vg  kapi_vg_t;
typedef struct kapi_lv  kapi_lv_t;

typedef enum {
    KAPI_LVM_OK            = 0,
    KAPI_LVM_EINVAL        = -1,
    KAPI_LVM_ENOMEM        = -2,
    KAPI_LVM_ENOENT        = -3,
    KAPI_LVM_EEXIST        = -4,
    KAPI_LVM_EBUSY         = -5,
    KAPI_LVM_ENOSPC        = -6
} kapi_lvm_err_t;

typedef enum {
    KAPI_LVM_LV_INACTIVE = 0,
    KAPI_LVM_LV_ACTIVE   = 1,
    KAPI_LVM_LV_SNAPSHOT = 2
} kapi_lvm_lv_state_t;

struct kapi_pv {
    char     name[KAPI_LVM_NAME_MAX];
    char     uuid[KAPI_LVM_UUID_LEN];
    char     vg_name[KAPI_LVM_NAME_MAX];
    kapi_blkdev_t dev;
    uint64_t total_sectors;
    uint64_t free_sectors;
    uint64_t pe_start;        /* physical extent start */
    uint32_t pe_size;         /* extent size in sectors */
    uint32_t pe_count;
    uint32_t pe_free;
    int      registered;
};

struct kapi_vg {
    char     name[KAPI_LVM_NAME_MAX];
    char     uuid[KAPI_LVM_UUID_LEN];
    uint32_t pe_size;
    uint32_t pv_count;
    uint32_t lv_count;
    uint64_t total_sectors;
    uint64_t free_sectors;
    int      active;
    kapi_pv_t *pvs[KAPI_LVM_MAX_PVS];
    kapi_lv_t *lvs[KAPI_LVM_MAX_LVS_PER_VG];
};

struct kapi_lv {
    char     name[KAPI_LVM_NAME_MAX];
    char     uuid[KAPI_LVM_UUID_LEN];
    char     vg_name[KAPI_LVM_NAME_MAX];
    uint64_t size_sectors;
    uint32_t le_count;
    int      state;           /* kapi_lvm_lv_state_t */
    int      readonly;
    kapi_vg_t *vg;
};

/* Subsystem lifecycle */
int kapi_lvm_init(void);
void kapi_lvm_exit(void);

/* Physical volume operations */
int kapi_lvm_pv_create(kapi_blkdev_t dev, const char *name);
int kapi_lvm_pv_remove(const char *name);
int kapi_lvm_pv_scan(void);
kapi_pv_t *kapi_lvm_pv_find(const char *name);

/* Volume group operations */
int kapi_lvm_vg_create(const char *name, uint32_t pe_size);
int kapi_lvm_vg_remove(const char *name);
int kapi_lvm_vg_extend(const char *vg_name, const char *pv_name);
int kapi_lvm_vg_reduce(const char *vg_name, const char *pv_name);
kapi_vg_t *kapi_lvm_vg_find(const char *name);

/* Logical volume operations */
int kapi_lvm_lv_create(const char *vg_name, const char *lv_name,
                       uint64_t size_sectors);
int kapi_lvm_lv_remove(const char *vg_name, const char *lv_name);
int kapi_lvm_lv_resize(const char *vg_name, const char *lv_name,
                       uint64_t new_size_sectors, int online);
int kapi_lvm_lv_activate(const char *vg_name, const char *lv_name);
int kapi_lvm_lv_deactivate(const char *vg_name, const char *lv_name);
kapi_lv_t *kapi_lvm_lv_find(const char *vg_name, const char *lv_name);

/* Snapshots (COW) */
int kapi_lvm_snapshot_create(const char *vg_name, const char *origin,
                             const char *snap_name, uint64_t size_sectors);
int kapi_lvm_snapshot_remove(const char *vg_name, const char *snap_name);
int kapi_lvm_snapshot_merge(const char *vg_name, const char *snap_name);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_LVM_H */
