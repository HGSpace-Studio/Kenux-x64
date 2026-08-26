/*
 * Kenux Advanced OS Skeleton - IOMMU implementation
 *
 * Skeleton: domain + group tables. Hardware context (VT-d root/context
 * tables, AMD-Vi device table, page-table walks) are TODO pending arch
 * probe.
 */

#include "kapi_iommu.h"
#include "kapi.h"

#include <string.h>

static kapi_iommu_domain_t kapi_iommu_domain_table[KAPI_IOMMU_MAX_DOMAINS];
static kapi_iommu_group_t  kapi_iommu_group_table[KAPI_IOMMU_MAX_GROUPS];
static int kapi_iommu_initialized = 0;

int kapi_iommu_init(void)
{
    if (kapi_iommu_initialized) {
        return KAPI_IOMMU_OK;
    }
    memset(kapi_iommu_domain_table, 0, sizeof(kapi_iommu_domain_table));
    memset(kapi_iommu_group_table, 0, sizeof(kapi_iommu_group_table));
    /* TODO: detect Intel VT-d DMAR / AMD-Vi IVRS tables and set up */
    kapi_iommu_initialized = 1;
    return KAPI_IOMMU_OK;
}

void kapi_iommu_exit(void)
{
    memset(kapi_iommu_domain_table, 0, sizeof(kapi_iommu_domain_table));
    memset(kapi_iommu_group_table, 0, sizeof(kapi_iommu_group_table));
    kapi_iommu_initialized = 0;
}

kapi_iommu_domain_t *kapi_iommu_domain_alloc(kapi_iommu_domain_type_t type)
{
    for (int i = 0; i < KAPI_IOMMU_MAX_DOMAINS; i++) {
        if (!kapi_iommu_domain_table[i].registered) {
            kapi_iommu_domain_t *d = &kapi_iommu_domain_table[i];
            memset(d, 0, sizeof(*d));
            d->id = (uint32_t)i + 1;
            d->type = type;
            d->refcount = 1;
            d->registered = 1;
            /* TODO: allocate page-table directory */
            return d;
        }
    }
    return NULL;
}

int kapi_iommu_domain_free(kapi_iommu_domain_t *domain)
{
    if (!domain || !domain->registered) {
        return KAPI_IOMMU_EINVAL;
    }
    if (domain->refcount > 1) {
        return KAPI_IOMMU_EBUSY;
    }
    /* TODO: tear down page tables, detach all devices */
    memset(domain, 0, sizeof(*domain));
    return KAPI_IOMMU_OK;
}

int kapi_iommu_group_add_device(kapi_iommu_group_t *group,
                                const kapi_iommu_device_t *dev)
{
    if (!group || !dev) {
        return KAPI_IOMMU_EINVAL;
    }
    if (group->num_devices >= KAPI_IOMMU_MAX_DEVICES) {
        return KAPI_IOMMU_ENOSPC;
    }
    group->devices[group->num_devices++] = *dev;
    return KAPI_IOMMU_OK;
}

int kapi_iommu_group_remove_device(kapi_iommu_group_t *group,
                                   uint16_t bus, uint16_t devfn)
{
    if (!group) {
        return KAPI_IOMMU_EINVAL;
    }
    for (int i = 0; i < group->num_devices; i++) {
        if (group->devices[i].bus == bus &&
            group->devices[i].device == devfn) {
            /* TODO: detach from domain */
            group->devices[i] = group->devices[group->num_devices - 1];
            group->num_devices--;
            return KAPI_IOMMU_OK;
        }
    }
    return KAPI_IOMMU_ENOENT;
}

kapi_iommu_group_t *kapi_iommu_group_find(uint32_t group_id)
{
    for (int i = 0; i < KAPI_IOMMU_MAX_GROUPS; i++) {
        if (kapi_iommu_group_table[i].id == group_id &&
            kapi_iommu_group_table[i].num_devices > 0) {
            return &kapi_iommu_group_table[i];
        }
    }
    return NULL;
}

int kapi_iommu_attach_device(kapi_iommu_domain_t *domain,
                             const kapi_iommu_device_t *dev)
{
    if (!domain || !dev) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: program context-entry / DTE to point at domain->pgd */
    domain->refcount++;
    return KAPI_IOMMU_OK;
}

int kapi_iommu_detach_device(kapi_iommu_domain_t *domain,
                             const kapi_iommu_device_t *dev)
{
    if (!domain || !dev) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: clear context-entry / DTE */
    if (domain->refcount > 0) {
        domain->refcount--;
    }
    return KAPI_IOMMU_OK;
}

int kapi_iommu_map(kapi_iommu_domain_t *domain, uint64_t iova,
                   uint64_t physical, uint64_t size, int prot)
{
    if (!domain) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: walk domain->pgd page table and install mapping for [iova, iova+size) */
    (void)iova; (void)physical; (void)size; (void)prot;
    return KAPI_IOMMU_OK;
}

int kapi_iommu_unmap(kapi_iommu_domain_t *domain, uint64_t iova,
                     uint64_t size)
{
    if (!domain) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: walk page table, remove mappings, invalidate IOTLB */
    (void)iova; (void)size;
    return KAPI_IOMMU_OK;
}

int kapi_iommu_set_passthrough(const kapi_iommu_device_t *dev)
{
    if (!dev) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: attach device to identity-mapped domain */
    return KAPI_IOMMU_OK;
}

int kapi_iommu_set_isolated(const kapi_iommu_device_t *dev)
{
    if (!dev) {
        return KAPI_IOMMU_EINVAL;
    }
    /* TODO: attach device to its own unmanaged domain */
    return KAPI_IOMMU_OK;
}
