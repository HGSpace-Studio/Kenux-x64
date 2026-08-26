/*
 * Kenux Advanced OS Skeleton - LVM implementation
 *
 * Skeleton: maintains PV/VG/LV tables and validates operations against
 * capacity. Backed by kapi_blkdev for I/O. TODO: extent allocation maps,
 * COW snapshot device, on-disk metadata.
 */

#include "kapi_lvm.h"
#include "kapi.h"

#include <string.h>

static kapi_pv_t kapi_lvm_pv_table[KAPI_LVM_MAX_PVS];
static kapi_vg_t kapi_lvm_vg_table[KAPI_LVM_MAX_VGS];
static int       kapi_lvm_initialized = 0;

static kapi_pv_t *pv_slot_alloc(void)
{
    for (int i = 0; i < KAPI_LVM_MAX_PVS; i++) {
        if (!kapi_lvm_pv_table[i].registered) {
            return &kapi_lvm_pv_table[i];
        }
    }
    return NULL;
}

int kapi_lvm_init(void)
{
    if (kapi_lvm_initialized) {
        return KAPI_LVM_OK;
    }
    memset(kapi_lvm_pv_table, 0, sizeof(kapi_lvm_pv_table));
    memset(kapi_lvm_vg_table, 0, sizeof(kapi_lvm_vg_table));
    kapi_lvm_initialized = 1;
    return KAPI_LVM_OK;
}

void kapi_lvm_exit(void)
{
    memset(kapi_lvm_pv_table, 0, sizeof(kapi_lvm_pv_table));
    memset(kapi_lvm_vg_table, 0, sizeof(kapi_lvm_vg_table));
    kapi_lvm_initialized = 0;
}

int kapi_lvm_pv_create(kapi_blkdev_t dev, const char *name)
{
    if (!dev || !name) {
        return KAPI_LVM_EINVAL;
    }
    if (kapi_lvm_pv_find(name)) {
        return KAPI_LVM_EEXIST;
    }
    kapi_pv_t *pv = pv_slot_alloc();
    if (!pv) {
        return KAPI_LVM_ENOMEM;
    }
    strncpy(pv->name, name, KAPI_LVM_NAME_MAX - 1);
    pv->dev = dev;
    /* TODO: read capacity + write PV header at LBA0 */
    pv->registered = 1;
    return KAPI_LVM_OK;
}

int kapi_lvm_pv_remove(const char *name)
{
    kapi_pv_t *pv = kapi_lvm_pv_find(name);
    if (!pv) {
        return KAPI_LVM_ENOENT;
    }
    if (pv->vg_name[0] != '\0') {
        return KAPI_LVM_EBUSY;
    }
    memset(pv, 0, sizeof(*pv));
    return KAPI_LVM_OK;
}

int kapi_lvm_pv_scan(void)
{
    /* TODO: enumerate blkdevs and detect PV labels */
    return KAPI_LVM_OK;
}

kapi_pv_t *kapi_lvm_pv_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_LVM_MAX_PVS; i++) {
        if (kapi_lvm_pv_table[i].registered &&
            strncmp(kapi_lvm_pv_table[i].name, name,
                    KAPI_LVM_NAME_MAX) == 0) {
            return &kapi_lvm_pv_table[i];
        }
    }
    return NULL;
}

int kapi_lvm_vg_create(const char *name, uint32_t pe_size)
{
    if (!name) {
        return KAPI_LVM_EINVAL;
    }
    if (kapi_lvm_vg_find(name)) {
        return KAPI_LVM_EEXIST;
    }
    for (int i = 0; i < KAPI_LVM_MAX_VGS; i++) {
        if (!kapi_lvm_vg_table[i].active) {
            kapi_vg_t *vg = &kapi_lvm_vg_table[i];
            memset(vg, 0, sizeof(*vg));
            strncpy(vg->name, name, KAPI_LVM_NAME_MAX - 1);
            vg->pe_size = pe_size ? pe_size : (4 * 1024); /* sectors */
            vg->active = 1;
            return KAPI_LVM_OK;
        }
    }
    return KAPI_LVM_ENOMEM;
}

int kapi_lvm_vg_remove(const char *name)
{
    kapi_vg_t *vg = kapi_lvm_vg_find(name);
    if (!vg) {
        return KAPI_LVM_ENOENT;
    }
    if (vg->lv_count > 0) {
        return KAPI_LVM_EBUSY;
    }
    memset(vg, 0, sizeof(*vg));
    return KAPI_LVM_OK;
}

int kapi_lvm_vg_extend(const char *vg_name, const char *pv_name)
{
    kapi_vg_t *vg = kapi_lvm_vg_find(vg_name);
    kapi_pv_t *pv = kapi_lvm_pv_find(pv_name);
    if (!vg || !pv) {
        return KAPI_LVM_ENOENT;
    }
    if (pv->vg_name[0] != '\0') {
        return KAPI_LVM_EBUSY;
    }
    if (vg->pv_count >= KAPI_LVM_MAX_PVS) {
        return KAPI_LVM_ENOSPC;
    }
    vg->pvs[vg->pv_count++] = pv;
    strncpy(pv->vg_name, vg_name, KAPI_LVM_NAME_MAX - 1);
    vg->total_sectors += pv->total_sectors;
    vg->free_sectors  += pv->free_sectors;
    return KAPI_LVM_OK;
}

int kapi_lvm_vg_reduce(const char *vg_name, const char *pv_name)
{
    kapi_vg_t *vg = kapi_lvm_vg_find(vg_name);
    if (!vg) {
        return KAPI_LVM_ENOENT;
    }
    for (uint32_t i = 0; i < vg->pv_count; i++) {
        if (strncmp(vg->pvs[i]->name, pv_name, KAPI_LVM_NAME_MAX) == 0) {
            /* TODO: migrate extents off PV before remove */
            vg->pvs[i]->vg_name[0] = '\0';
            vg->pvs[i] = vg->pvs[vg->pv_count - 1];
            vg->pvs[--vg->pv_count] = NULL;
            return KAPI_LVM_OK;
        }
    }
    return KAPI_LVM_ENOENT;
}

kapi_vg_t *kapi_lvm_vg_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_LVM_MAX_VGS; i++) {
        if (kapi_lvm_vg_table[i].active &&
            strncmp(kapi_lvm_vg_table[i].name, name,
                    KAPI_LVM_NAME_MAX) == 0) {
            return &kapi_lvm_vg_table[i];
        }
    }
    return NULL;
}

int kapi_lvm_lv_create(const char *vg_name, const char *lv_name,
                       uint64_t size_sectors)
{
    kapi_vg_t *vg = kapi_lvm_vg_find(vg_name);
    if (!vg || !lv_name) {
        return KAPI_LVM_ENOENT;
    }
    if (vg->free_sectors < size_sectors) {
        return KAPI_LVM_ENOSPC;
    }
    if (vg->lv_count >= KAPI_LVM_MAX_LVS_PER_VG) {
        return KAPI_LVM_ENOMEM;
    }
    /* TODO: allocate extents; allocate persistent lv struct */
    kapi_lv_t lv;
    memset(&lv, 0, sizeof(lv));
    strncpy(lv.name, lv_name, KAPI_LVM_NAME_MAX - 1);
    strncpy(lv.vg_name, vg_name, KAPI_LVM_NAME_MAX - 1);
    lv.size_sectors = size_sectors;
    lv.state = KAPI_LVM_LV_INACTIVE;
    lv.vg = vg;
    /* NOTE: a real impl would persist this in the LV metadata area. */
    vg->free_sectors -= size_sectors;
    return KAPI_LVM_OK;
}

int kapi_lvm_lv_remove(const char *vg_name, const char *lv_name)
{
    (void)vg_name; (void)lv_name;
    /* TODO: locate LV, free extents, invalidate */
    return KAPI_LVM_OK;
}

int kapi_lvm_lv_resize(const char *vg_name, const char *lv_name,
                       uint64_t new_size_sectors, int online)
{
    (void)vg_name; (void)lv_name; (void)new_size_sectors; (void)online;
    /* TODO: extend/reduce extent map; handle online flag */
    return KAPI_LVM_OK;
}

int kapi_lvm_lv_activate(const char *vg_name, const char *lv_name)
{
    (void)vg_name; (void)lv_name;
    /* TODO: register a blkdev front-end for the LV */
    return KAPI_LVM_OK;
}

int kapi_lvm_lv_deactivate(const char *vg_name, const char *lv_name)
{
    (void)vg_name; (void)lv_name;
    return KAPI_LVM_OK;
}

kapi_lv_t *kapi_lvm_lv_find(const char *vg_name, const char *lv_name)
{
    kapi_vg_t *vg = kapi_lvm_vg_find(vg_name);
    if (!vg || !lv_name) {
        return NULL;
    }
    for (uint32_t i = 0; i < vg->lv_count; i++) {
        if (vg->lvs[i] && strncmp(vg->lvs[i]->name, lv_name,
                                  KAPI_LVM_NAME_MAX) == 0) {
            return vg->lvs[i];
        }
    }
    return NULL;
}

int kapi_lvm_snapshot_create(const char *vg_name, const char *origin,
                             const char *snap_name, uint64_t size_sectors)
{
    (void)vg_name; (void)origin; (void)snap_name; (void)size_sectors;
    /* TODO: allocate COW device + exception table */
    return KAPI_LVM_OK;
}

int kapi_lvm_snapshot_remove(const char *vg_name, const char *snap_name)
{
    (void)vg_name; (void)snap_name;
    return KAPI_LVM_OK;
}

int kapi_lvm_snapshot_merge(const char *vg_name, const char *snap_name)
{
    (void)vg_name; (void)snap_name;
    /* TODO: merge COW exceptions back into origin */
    return KAPI_LVM_OK;
}
