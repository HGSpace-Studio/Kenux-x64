

#ifndef KAPI_DEVICE_H
#define KAPI_DEVICE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_device* kapi_dev_t;

#define KAPI_DEV_UNKNOWN     0
#define KAPI_DEV_BLOCK       1
#define KAPI_DEV_CHAR        2
#define KAPI_DEV_NET         3
#define KAPI_DEV_SOUND       4
#define KAPI_DEV_VIDEO       5
#define KAPI_DEV_INPUT       6
#define KAPI_DEV_BUS         7

#define KAPI_IOC_MAGIC       'K'
#define KAPI_IOC_GET_INFO    0x01
#define KAPI_IOC_RESET       0x02
#define KAPI_IOC_POWER_OFF   0x03
#define KAPI_IOC_POWER_ON    0x04
#define KAPI_IOC_SET_CONFIG  0x10
#define KAPI_IOC_GET_CONFIG  0x11

typedef struct {
    char     name[64];
    int      type;
    uint32_t vendor_id;
    uint32_t device_id;
    uint64_t capacity;
    uint32_t block_size;
    int      status;
} kapi_dev_info_t;

typedef struct {
    uint64_t src;
    uint64_t dst;
    uint64_t size;
    uint32_t flags;
} kapi_dma_desc_t;

kapi_dev_t kapi_dev_open(const char* name, int mode);

int kapi_dev_close(kapi_dev_t dev);

int64_t kapi_dev_read(kapi_dev_t dev, void* buf, size_t count);

int64_t kapi_dev_write(kapi_dev_t dev, const void* buf, size_t count);

int kapi_dev_ioctl(kapi_dev_t dev, uint32_t cmd, void* arg);

int kapi_dev_get_info(kapi_dev_t dev, kapi_dev_info_t* info);

int kapi_dev_count(void);

int kapi_dev_list(kapi_dev_info_t* infos, int count);

int kapi_irq_register(int irq, void (*handler)(int, void*), void* arg);

int kapi_irq_unregister(int irq);

int kapi_dma_submit(kapi_dma_desc_t* desc, int count);

int kapi_dma_wait(uint64_t timeout_ms);

uint64_t kapi_get_time_ms(void);

uint64_t kapi_get_time_us(void);

int kapi_timer_set(uint64_t interval_ms, void (*callback)(void*), void* arg, int periodic);

int kapi_timer_cancel(int timer_id);

#ifdef __cplusplus
}
#endif

#endif
