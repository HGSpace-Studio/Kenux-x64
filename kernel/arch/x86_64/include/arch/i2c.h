#ifndef ARCH_X86_64_I2C_H
#define ARCH_X86_64_I2C_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/pci.h>

#define I2C_MAX_BUSES        8
#define I2C_MAX_RETRIES       1000
#define I2C_SMBUS_MAX_BLOCK   32

#define I2C_SMBUS_IO_BASE     0x0500
#define I2C_SMBUS_HST_STS     0x00
#define I2C_SMBUS_HST_CNT     0x02
#define I2C_SMBUS_HST_CMD     0x03
#define I2C_SMBUS_HST_ADDR    0x04
#define I2C_SMBUS_HST_D0      0x05
#define I2C_SMBUS_HST_D1      0x06
#define I2C_SMBUS_HST_BLOCK   0x07

#define I2C_SMBUS_CLASS_CODE   0x0C05

#define I2C_FLAG_QUICK         0x00
#define I2C_FLAG_BYTE          0x01
#define I2C_FLAG_BYTE_DATA    0x02
#define I2C_FLAG_WORD_DATA     0x03
#define I2C_FLAG_BLOCK_DATA    0x05
#define I2C_FLAG_I2C_BLOCK     0x06
#define I2C_FLAG_PROCESS_CALL 0x04
#define I2C_FLAG_BLOCK_PROCESS 0x07

#define I2C_SMBUS_HOST_BUSY    0x01
#define I2C_SMBUS_INTR         0x02
#define I2C_SMBUS_DEV_ERR      0x04
#define I2C_SMBUS_BUS_ERR      0x08
#define I2C_SMBUS_FAILED       0x10
#define I2C_SMBUS_SMBALERT     0x20
#define I2C_SMBUS_INUSE_STS    0x40
#define I2C_SMBUS_HOST_CLEAR   0x80

#define I2C_SMBUS_KILL         0x01
#define I2C_SMBUS_START        0x02
#define I2C_SMBUS_PEC_EN       0x04
#define I2C_SMBUS_ADDR_16BIT   0x08
#define I2C_SMBUS_BLOCK_PROC   0x10
#define I2C_SMBUS_LAST_BYTE    0x20
#define I2C_SMBUS_INTREN       0x40
#define I2C_SMBUS_SMBUS_CMD    0x80

#define I2C_STATUS_OK          0
#define I2C_STATUS_ERROR       -1
#define I2C_STATUS_BUSY        -2
#define I2C_STATUS_TIMEOUT     -3
#define I2C_STATUS_NAK         -4
#define I2C_STATUS_ARB_LOST    -5

#define I2C_START_COND        0x08
#define I2C_REP_START_COND     0x10
#define I2C_ADDR_WRITE_ACK    0x18
#define I2C_ADDR_READ_ACK     0x40
#define I2C_DATA_WRITE_ACK    0x28
#define I2C_DATA_READ_ACK     0x50
#define I2C_NACK               0x20
#define I2C_BUS_ERROR         0x00

typedef struct i2c_bus {
    uint16_t base_io;
    uint8_t bus_id;
    uint8_t pci_bus;
    uint8_t pci_dev;
    uint8_t pci_func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t pci_config;
    int active;
    spinlock_t lock;
} i2c_bus_t;

typedef struct {
    uint8_t addr;
    uint8_t cmd;
    uint8_t* data;
    uint32_t len;
    int direction;
    int pec;
    uint16_t* smbus_version;
} i2c_msg_t;

int i2c_smbus_init(void);
int i2c_smbus_read_byte(i2c_bus_t* bus, uint8_t addr, uint8_t cmd);
int i2c_smbus_write_byte(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint8_t value);
int i2c_smbus_read_word(i2c_bus_t* bus, uint8_t addr, uint8_t cmd);
int i2c_smbus_write_word(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint16_t value);
int i2c_smbus_block_read(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint8_t* buf, uint32_t max_len);
int i2c_smbus_block_write(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, const uint8_t* buf, uint32_t len);
int i2c_smbus_process_call(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint16_t value);
int i2c_smbus_quick_cmd(i2c_bus_t* bus, uint8_t addr, int read_write);

void i2c_transfer_start(i2c_bus_t* bus);
void i2c_transfer_stop(i2c_bus_t* bus);
void i2c_transfer_ack(i2c_bus_t* bus);
void i2c_transfer_nack(i2c_bus_t* bus);

i2c_bus_t* i2c_get_bus(uint8_t bus_id);
int i2c_bus_count(void);

#endif
