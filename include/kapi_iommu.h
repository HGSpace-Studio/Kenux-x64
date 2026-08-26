#ifndef KAPI_IOMMU_H
#define KAPI_IOMMU_H

/*
 * Kenux Advanced OS Skeleton - IOMMU (Intel VT-d / AMD-Vi)
 *
 * Hardware-assisted DMA isolation for device passthrough (KVM device
 * assignment) and secure driver isolation. Skeleton: API only.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_pci.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IOMMU_MAX_GROUPS    256
#define KAPI_IOMMU_MAX_DOMAINS   256
#define KAPI_IOMMU_MAX_DEVICES   16  /* per group */

typedef struct kapi_iommu_domain kapi_iommu_domain_t;

typedef enum {
    KAPI_IOMMU_OK            = 0,
    KAPI_IOMMU_EINVAL        = -1,
    KAPI_IOMMU_ENOMEM        = -2,
    KAPI_IOMMU_ENOENT        = -3,
    KAPI_IOMMU_EEXIST        = -4,
    KAPI_IOMMU_EBUSY         = -5,
    KAPI_IOMMU_ENOTSUP       = -6
} kapi_iommu_err_t;

typedef enum {
    KAPI_IOMMU_DOMAIN_DMA    = 0,   /* identity / shared DMA domain */
    KAPI_IOMMU_DOMAIN_UNMANAGED = 1,/* custom page table for passthrough */
    KAPI_IOMMU_DOMAIN_IDENTITY = 2  /* 1:1 mapping */
} kapi_iommu_domain_type_t;

typedef struct {
    uint16_t segment;
    uint16_t bus;
    uint16_t device;
    uint16_t function;
    kapi_iommu_domain_t *domain;
} kapi_iommu_device_t;

typedef struct {
    uint32_t id;
    uint32_t num_devices;
    kapi_iommu_device_t devices[KAPI_IOMMU_MAX_DEVICES];
    kapi_iommu_domain_t *domain;
} kapi_iommu_group_t;

struct kapi_iommu_domain {
    uint32_t id;
    kapi_iommu_domain_type_t type;
    uint64_t pgd;               /* page-table directory physical addr */
    uint32_t refcount;
    int      registered;
};

/* Subsystem lifecycle */
int kapi_iommu_init(void);
void kapi_iommu_exit(void);

/* Domain management */
kapi_iommu_domain_t *kapi_iommu_domain_alloc(kapi_iommu_domain_type_t type);
int kapi_iommu_domain_free(kapi_iommu_domain_t *domain);

/* Group management */
int kapi_iommu_group_add_device(kapi_iommu_group_t *group,
                                const kapi_iommu_device_t *dev);
int kapi_iommu_group_remove_device(kapi_iommu_group_t *group,
                                   uint16_t bus, uint16_t devfn);
kapi_iommu_group_t *kapi_iommu_group_find(uint32_t group_id);

/* Attach / detach device to domain */
int kapi_iommu_attach_device(kapi_iommu_domain_t *domain,
                             const kapi_iommu_device_t *dev);
int kapi_iommu_detach_device(kapi_iommu_domain_t *domain,
                             const kapi_iommu_device_t *dev);

/* IOMMU page table (1:1 identity map for passthrough) */
int kapi_iommu_map(kapi_iommu_domain_t *domain, uint64_t iova,
                   uint64_t physical, uint64_t size, int prot);
int kapi_iommu_unmap(kapi_iommu_domain_t *domain, uint64_t iova,
                     uint64_t size);

/* Passthrough / isolation */
int kapi_iommu_set_passthrough(const kapi_iommu_device_t *dev);
int kapi_iommu_set_isolated(const kapi_iommu_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_IOMMU_H */
