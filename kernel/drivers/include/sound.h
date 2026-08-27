#ifndef KERNEL_DRIVERS_SOUND_H
#define KERNEL_DRIVERS_SOUND_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define SOUND_MAX_CHANNELS  8
#define SOUND_MAX_RATE      192000
#define SOUND_BUF_SIZE      4096

typedef struct {
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits_per_sample;
    uint8_t  format;
} sound_format_t;

typedef struct {
    void*    buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
    spinlock_t lock;
} sound_ringbuf_t;

typedef struct sound_device sound_device_t;

struct sound_device {
    int              type;
    sound_format_t   format;
    sound_ringbuf_t  playback_buf;
    sound_ringbuf_t  capture_buf;
    int              volume_master;
    int              volume_pcm;
    int              volume_mic;
    int              muted;
    void*            hw_ctx;
    int (*start_playback)(sound_device_t* dev);
    int (*stop_playback)(sound_device_t* dev);
    int (*start_capture)(sound_device_t* dev);
    int (*stop_capture)(sound_device_t* dev);
    int (*set_volume)(sound_device_t* dev, int channel, int left, int right);
    int (*get_volume)(sound_device_t* dev, int channel, int* left, int* right);
    int (*set_format)(sound_device_t* dev, const sound_format_t* fmt);
    spinlock_t       lock;
};

void  sound_device_init(sound_device_t* dev, int type);
int   sound_write(sound_device_t* dev, const void* data, uint32_t size);
int   sound_read(sound_device_t* dev, void* data, uint32_t size);
int   sound_set_volume(sound_device_t* dev, int channel, int left, int right);
int   sound_get_volume(sound_device_t* dev, int channel, int* left, int* right);

#define AC97_VENDOR_ID    0x8086
#define AC97_DEVICE_ID    0x2415

#define AC97_RESET        0x00
#define AC97_MASTER_VOL   0x02
#define AC97_AUX_OUT_VOL  0x04
#define AC97_PCM_VOL      0x18
#define AC97_REC_SEL      0x1A
#define AC97_REC_GAIN     0x1C
#define AC97_MIC_VOL      0x1E
#define AC97_PCM_FRONT_DAC_RATE 0x2C
#define AC97_PCM_ADC_RATE 0x32
#define AC97_VENDOR_ID1   0x7C
#define AC97_VENDOR_ID2   0x7E

#define AC97_NAM_PORT     0
#define AC97_NABM_PORT    1

#define AC97_PCM_OUT_BDL  0x10
#define AC97_PCM_OUT_LVI  0x15
#define AC97_PCM_OUT_SR   0x16
#define AC97_PCM_OUT_PICB 0x18
#define AC97_PCM_OUT_CIV  0x19
#define AC97_PCM_OUT_LPIB 0x20
#define AC97_GLOBAL_CTRL  0x2C
#define AC97_GLOBAL_STA   0x30

typedef struct {
    pci_device_t*    pci_dev;
    uint16_t         nam_port;
    uint16_t         nabm_port;
    uint32_t         bdl_phys;
    void*            bdl_virt;
    void*            dma_buf;
    uint32_t         dma_phys;
    int              playing;
    int              capturing;
    sound_device_t   sound_dev;
} ac97_dev_t;

int  ac97_init(ac97_dev_t* dev, pci_device_t* pci_dev);
void ac97_shutdown(ac97_dev_t* dev);
int  ac97_start_playback(sound_device_t* dev);
int  ac97_stop_playback(sound_device_t* dev);
int  ac97_set_volume(sound_device_t* dev, int channel, int left, int right);
int  ac97_get_volume(sound_device_t* dev, int channel, int* left, int* right);
int  ac97_set_format(sound_device_t* dev, const sound_format_t* fmt);
void ac97_irq_handler(int irq, void* ctx);

#define ES1371_VENDOR_ID  0x1274
#define ES1371_DEVICE_ID  0x1371

#define ES1371_REG_CONTROL   0x00
#define ES1371_REG_STATUS    0x04
#define ES1371_REG_MEMPAGE   0x0C
#define ES1371_REG_CODEC     0x14
#define ES1371_REG_SERIAL    0x20
#define ES1371_REG_DAC1_SC   0x24
#define ES1371_REG_DAC2_SC   0x28
#define ES1371_REG_ADC_SC    0x2C

typedef struct {
    pci_device_t*    pci_dev;
    volatile uint32_t* mmio;
    uint32_t         dac1_rate;
    uint32_t         dac2_rate;
    uint32_t         adc_rate;
    void*            dma_buf;
    uint32_t         dma_phys;
    int              playing;
    sound_device_t   sound_dev;
} es1371_dev_t;

int  es1371_init(es1371_dev_t* dev, pci_device_t* pci_dev);
void es1371_shutdown(es1371_dev_t* dev);
int  es1371_start_playback(sound_device_t* dev);
int  es1371_stop_playback(sound_device_t* dev);
void es1371_irq_handler(int irq, void* ctx);

#define I2C_MAX_ADAPTERS  16

typedef struct i2c_adapter i2c_adapter_t;

struct i2c_adapter {
    int          id;
    uint32_t     speed;
    int (*xfer)(i2c_adapter_t* adapter, uint8_t addr, uint8_t* wbuf, int wlen, uint8_t* rbuf, int rlen);
    spinlock_t   lock;
};

typedef struct {
    i2c_adapter_t adapters[I2C_MAX_ADAPTERS];
    int           count;
    spinlock_t    lock;
} i2c_bus_t;

void             i2c_init(i2c_bus_t* bus);
i2c_adapter_t*   i2c_register(i2c_bus_t* bus, uint32_t speed);
int              i2c_write(i2c_adapter_t* adapter, uint8_t addr, const uint8_t* data, int len);
int              i2c_read(i2c_adapter_t* adapter, uint8_t addr, uint8_t* data, int len);
int              i2c_xfer(i2c_adapter_t* adapter, uint8_t addr, uint8_t* wbuf, int wlen, uint8_t* rbuf, int rlen);

#endif