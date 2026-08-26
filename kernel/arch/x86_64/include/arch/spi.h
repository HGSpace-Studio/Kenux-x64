#ifndef ARCH_X86_64_SPI_H
#define ARCH_X86_64_SPI_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/pci.h>

#define SPI_MAX_BUSES          8
#define SPI_MAX_DEVICES        16
#define SPI_MAX_CS             8
#define SPI_NAME_MAX           32

#define SPI_MODE_0             0x00
#define SPI_MODE_1             0x01
#define SPI_MODE_2             0x02
#define SPI_MODE_3             0x03

#define SPI_CPOL               0x02
#define SPI_CPHA               0x01
#define SPI_CS_HIGH            0x04
#define SPI_LSB_FIRST          0x08
#define SPI_3WIRE              0x10
#define SPI_LOOP               0x20
#define SPI_NO_CS              0x40
#define SPI_READY              0x80

#define SPI_CS_ACTIVE_LOW      0
#define SPI_CS_ACTIVE_HIGH     1

#define SPI_TRANSFER_FULL_DUPLEX 0
#define SPI_TRANSFER_HALF_WRITE   1
#define SPI_TRANSFER_HALF_READ    2

#define SPI_SPEED_HZ_DEFAULT  1000000

typedef struct spi_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t chip_select;
    uint8_t mode;
    uint32_t max_speed_hz;
    uint8_t bits_per_word;
    uint8_t cs_polarity;
    char name[SPI_NAME_MAX];
    void* driver_data;
    int registered;
    struct spi_device* next;
} spi_device_t;

typedef struct spi_bus {
    uint16_t base_mmio;
    uint32_t mmio_size;
    uint8_t bus_id;
    uint8_t pci_bus;
    uint8_t pci_dev;
    uint8_t pci_func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t clock_hz;
    uint32_t num_cs;
    int active;
    spinlock_t lock;
    spi_device_t* devices[SPI_MAX_DEVICES];
    int device_count;

    void (*set_cs)(struct spi_bus* bus, uint8_t cs, int active);
    int (*transfer)(struct spi_bus* bus, uint8_t cs,
                   const uint8_t* tx_buf, uint8_t* rx_buf,
                   uint32_t len);
    int (*set_mode)(struct spi_bus* bus, uint8_t mode);
    int (*set_speed)(struct spi_bus* bus, uint32_t hz);
    int (*set_bits)(struct spi_bus* bus, uint8_t bits);
} spi_bus_t;

typedef struct spi_transfer {
    const uint8_t* tx_buf;
    uint8_t* rx_buf;
    uint32_t len;
    uint32_t speed_hz;
    uint8_t bits_per_word;
    uint16_t delay_usecs;
    uint8_t cs_change;
} spi_transfer_t;

int spi_bus_register(spi_bus_t* bus);
int spi_bus_unregister(uint8_t bus_id);
spi_bus_t* spi_get_bus(uint8_t bus_id);

int spi_device_register(spi_bus_t* bus, spi_device_t* device);
int spi_device_unregister(spi_bus_t* bus, uint8_t cs);
spi_device_t* spi_get_device(spi_bus_t* bus, uint8_t cs);

int spi_transfer(spi_bus_t* bus, uint8_t cs, const uint8_t* tx_buf, uint8_t* rx_buf, uint32_t len);
int spi_write(spi_bus_t* bus, uint8_t cs, const uint8_t* tx_buf, uint32_t len);
int spi_read(spi_bus_t* bus, uint8_t cs, uint8_t* rx_buf, uint32_t len);
int spi_write_then_read(spi_bus_t* bus, uint8_t cs,
                         const uint8_t* tx_buf, uint32_t tx_len,
                         uint8_t* rx_buf, uint32_t rx_len);

int spi_bus_count(void);

#endif
