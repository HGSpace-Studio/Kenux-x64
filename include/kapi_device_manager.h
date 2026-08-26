#ifndef KAPI_DEVICE_MANAGER_H
#define KAPI_DEVICE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_pci.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Device types */
#define KAPI_DEVICE_TYPE_UNKNOWN    0
#define KAPI_DEVICE_TYPE_STORAGE    1
#define KAPI_DEVICE_TYPE_NETWORK    2
#define KAPI_DEVICE_TYPE_GPU       3
#define KAPI_DEVICE_TYPE_INPUT     4
#define KAPI_DEVICE_TYPE_SERIAL    5
#define KAPI_DEVICE_TYPE_USB       6
#define KAPI_DEVICE_TYPE_AUDIO     7
#define KAPI_DEVICE_TYPE_MISC      8

/* Device power states */
#define KAPI_DEVICE_POWER_UNKNOWN  0
#define KAPI_DEVICE_POWER_D0       1  /* Fully on */
#define KAPI_DEVICE_POWER_D1       2  /* Low latency L3 */
#define KAPI_DEVICE_POWER_D2       3  /* Low power L2 */
#define KAPI_DEVICE_POWER_D3       4  /* Off */

/* Device hotplug states */
#define KAPI_DEVICE_HOTPLUG_NONE   0
#define KAPI_DEVICE_HOTPLUG_INSERT 1
#define KAPI_DEVICE_HOTPLUG_REMOVE 2

typedef struct kapi_device kapi_device_t;

/* Device information structure */
typedef struct {
    char name[64];
    char driver_name[64];
    char description[128];
    uint32_t device_type;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t subsystem_vendor_id;
    uint32_t subsystem_device_id;
    uint64_t dma_mask;
    uint32_t power_state;
    uint8_t  irq_count;
    uint32_t irqs[32];
    uint64_t mem_start;
    uint64_t mem_end;
    uint64_t mem_size;
    uint8_t  capabilities[64]; /* Bitmask of supported capabilities */
    int      online;
} kapi_device_info_t;

/* Device capabilities */
#define KAPI_DEVICE_CAP_DMA        (1 << 0)
#define KAPI_DEVICE_CAP_MSI        (1 << 1)
#define KAPI_DEVICE_CAP_MSIX       (1 << 2)
#define KAPI_DEVICE_CAP_PM         (1 << 3)
#define KAPI_DEVICE_CAP_HOTPLUG    (1 << 4)
#define KAPI_DEVICE_CAP_VIRTUAL    (1 << 5)
#define KAPI_DEVICE_CAP_CRYPTO     (1 << 6)

/* Device manager operations */
typedef struct {
    int (*probe)(kapi_device_t* dev);
    int (*remove)(kapi_device_t* dev);
    int (*suspend)(kapi_device_t* dev, uint32_t state);
    int (*resume)(kapi_device_t* dev, uint32_t state);
    int (*enable_dma)(kapi_device_t* dev, uint64_t mask);
    void (*irq_handler)(kapi_device_t* dev, uint32_t irq);
} kapi_device_ops_t;

/* Device structure */
struct kapi_device {
    kapi_device_t*            next;           /* Next device in list */
    char                     name[64];       /* Device name */
    char                     bus_name[32];   /* Bus name (PCI, USB, etc.) */
    uint32_t                 device_type;    /* Device type */
    uint32_t                 device_id;      /* Device identifier */
    kapi_device_ops_t*       ops;            /* Device operations */
    void*                    driver_data;    /* Driver-specific data */
    uint32_t                 ref_count;      /* Reference count */
    uint32_t                 power_state;    /* Current power state */
    uint64_t                 dma_mask;       /* DMA mask */
    kapi_device_info_t       info;           /* Device information */
};

/* Device manager API */
int kapi_device_manager_init(void);
void kapi_device_manager_cleanup(void);

int kapi_device_register(kapi_device_t* dev);
int kapi_device_unregister(kapi_device_t* dev);
kapi_device_t* kapi_device_find(const char* name);
kapi_device_t* kapi_device_find_by_type(uint32_t device_type, uint32_t index);
int kapi_device_enable(kapi_device_t* dev);
int kapi_device_disable(kapi_device_t* dev);
int kapi_device_set_power_state(kapi_device_t* dev, uint32_t state);
int kapi_device_get_power_state(kapi_device_t* dev, uint32_t* state);
int kapi_device_query_info(kapi_device_t* dev, kapi_device_info_t* info);
int kapi_device_request_irq(kapi_device_t* dev, uint32_t irq, 
                           void (*handler)(kapi_device_t*, uint32_t));
int kapi_device_free_irq(kapi_device_t* dev, uint32_t irq);

/* Device hotplug API */
int kapi_device_hotplug_register(uint32_t device_type, 
                                int (*handler)(kapi_device_t*, uint32_t));
int kapi_device_hotplug_unregister(uint32_t device_type);

/* Device enumeration */
int kapi_device_enumerate_pci(void);
int kapi_device_enumerate_usb(void);
int kapi_device_enumerate_storage(void);

/* Device notification system */
typedef struct {
    kapi_device_t* device;
    uint32_t event_type;
    uint32_t event_data;
} kapi_device_event_t;

#define KAPI_DEVICE_EVENT_ADD      1
#define KAPI_DEVICE_EVENT_REMOVE   2
#define KAPI_DEVICE_EVENT_SUSPEND  3
#define KAPI_DEVICE_EVENT_RESUME   4
#define KAPI_DEVICE_EVENT_PM       5

typedef void (*kapi_device_notifier_t)(const kapi_device_event_t* event);

int kapi_device_notifier_register(kapi_device_notifier_t notifier);
int kapi_device_notifier_unregister(kapi_device_notifier_t notifier);

/* Device statistics */
typedef struct {
    uint64_t reads;
    uint64_t writes;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t errors;
    uint64_t interrupts;
    uint64_t dma_requests;
} kapi_device_stats_t;

int kapi_device_get_stats(kapi_device_t* dev, kapi_device_stats_t* stats);

/* Device debugging */
int kapi_device_dump_info(kapi_device_t* dev);
int kapi_device_dump_all(void);

#ifdef __cplusplus
}
#endif

#endif