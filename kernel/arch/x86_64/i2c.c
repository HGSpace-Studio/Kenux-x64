#include <arch/i2c.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <string.h>

static i2c_bus_t i2c_buses[I2C_MAX_BUSES];
static int i2c_bus_count_val = 0;
static spinlock_t i2c_global_lock = SPINLOCK_INIT;

static inline uint8_t i2c_smbus_read(uint16_t base, uint8_t offset)
{
    return inb(base + offset);
}

static inline void i2c_smbus_write(uint16_t base, uint8_t offset, uint8_t value)
{
    outb(base + offset, value);
}

static inline uint16_t i2c_smbus_read_word_io(uint16_t base, uint8_t offset)
{
    return inw(base + offset);
}

static inline void i2c_smbus_write_word_io(uint16_t base, uint8_t offset, uint16_t value)
{
    outw(base + offset, value);
}

static int i2c_smbus_wait_until_ready(i2c_bus_t* bus)
{
    uint32_t timeout = I2C_MAX_RETRIES;
    while (timeout--) {
        uint8_t status = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_STS);
        if (!(status & I2C_SMBUS_HOST_BUSY)) return I2C_STATUS_OK;
        for (volatile int d = 0; d < 10; d++);
    }
    return I2C_STATUS_TIMEOUT;
}

static int i2c_smbus_wait_until_done(i2c_bus_t* bus)
{
    uint32_t timeout = I2C_MAX_RETRIES;
    while (timeout--) {
        uint8_t status = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_STS);
        if (!(status & I2C_SMBUS_HOST_BUSY)) {
            if (status & I2C_SMBUS_FAILED) return I2C_STATUS_ERROR;
            if (status & I2C_SMBUS_BUS_ERR) return I2C_STATUS_ARB_LOST;
            if (status & I2C_SMBUS_DEV_ERR) return I2C_STATUS_NAK;
            return I2C_STATUS_OK;
        }
        for (volatile int d = 0; d < 10; d++);
    }

    uint8_t status = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_STS);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, I2C_SMBUS_KILL);
    for (volatile int d = 0; d < 100; d++);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_STS, I2C_SMBUS_HOST_CLEAR);

    return I2C_STATUS_TIMEOUT;
}

static void i2c_smbus_clear_status(i2c_bus_t* bus)
{
    uint8_t status = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_STS);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_STS, status);
    for (volatile int d = 0; d < 10; d++);
}

static int i2c_smbus_execute(i2c_bus_t* bus, uint8_t addr, uint8_t cmd,
                              uint8_t protocol, uint8_t read_write,
                              uint8_t data_byte, uint16_t data_word)
{
    if (!bus || !bus->active) return I2C_STATUS_ERROR;

    spin_lock(&bus->lock);

    int ret = i2c_smbus_wait_until_ready(bus);
    if (ret != I2C_STATUS_OK) {
        spin_unlock(&bus->lock);
        return ret;
    }

    i2c_smbus_clear_status(bus);

    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_ADDR, (addr << 1) | read_write);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CMD, cmd);

    if (protocol == I2C_FLAG_BYTE_DATA || protocol == I2C_FLAG_BYTE) {
        i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_D0, data_byte);
    } else if (protocol == I2C_FLAG_WORD_DATA || protocol == I2C_FLAG_PROCESS_CALL) {
        i2c_smbus_write_word_io(bus->base_io, I2C_SMBUS_HST_D0, data_word);
    }

    uint8_t control = protocol | I2C_SMBUS_START | I2C_SMBUS_SMBUS_CMD;
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, control);

    ret = i2c_smbus_wait_until_done(bus);

    spin_unlock(&bus->lock);
    return ret;
}

int i2c_smbus_init(void)
{
    i2c_bus_count_val = 0;
    spin_init(&i2c_global_lock);
    for (int i = 0; i < I2C_MAX_BUSES; i++) {
        i2c_buses[i].active = 0;
        spin_init(&i2c_buses[i].lock);
    }

    int found = 0;

    for (int b = 0; b < PCI_MAX_BUSSES && i2c_bus_count_val < I2C_MAX_BUSES; b++) {
        for (int d = 0; d < PCI_MAX_DEVICES && i2c_bus_count_val < I2C_MAX_BUSES; d++) {
            for (int f = 0; f < PCI_MAX_FUNCTIONS; f++) {
                uint32_t class_reg = pci_read_config(b, d, f, 0x08);
                if (class_reg == 0xFFFFFFFF) continue;

                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass = (class_reg >> 16) & 0xFF;

                if (class_code != 0x0C || subclass != 0x05) continue;

                uint32_t id_reg = pci_read_config(b, d, f, 0x00);
                uint16_t vid = id_reg & 0xFFFF;
                uint16_t did = (id_reg >> 16) & 0xFFFF;

                uint16_t cmd_reg = (uint16_t)pci_read_config(b, d, f, 0x04);
                cmd_reg |= 0x01;
                pci_write_config(b, d, f, 0x04, cmd_reg);

                uint16_t base = 0;
                uint8_t header_type = (uint8_t)(pci_read_config(b, d, f, 0x0C) & 0xFF);
                if ((header_type & 0x7F) == 0) {
                    uint32_t bar4 = pci_read_config(b, d, f, 0x20);
                    if (bar4 & 0x01) {
                        base = bar4 & 0xFFFC;
                    } else {
                        base = (uint16_t)(bar4 & 0x0000FFFF);
                    }
                }

                if (base == 0) continue;

                i2c_bus_t* bus = &i2c_buses[i2c_bus_count_val];
                bus->base_io = base;
                bus->bus_id = (uint8_t)i2c_bus_count_val;
                bus->pci_bus = (uint8_t)b;
                bus->pci_dev = (uint8_t)d;
                bus->pci_func = (uint8_t)f;
                bus->vendor_id = vid;
                bus->device_id = did;
                bus->pci_config = pci_read_config(b, d, f, 0x90);
                bus->active = 1;

                i2c_smbus_clear_status(bus);
                i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, 0);

                i2c_bus_count_val++;
                found++;
            }
        }
    }

    return found;
}

int i2c_smbus_read_byte(i2c_bus_t* bus, uint8_t addr, uint8_t cmd)
{
    int ret = i2c_smbus_execute(bus, addr, cmd, I2C_FLAG_BYTE_DATA, 1, 0, 0);
    if (ret != I2C_STATUS_OK) return ret;
    return (int)i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_D0);
}

int i2c_smbus_write_byte(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint8_t value)
{
    return i2c_smbus_execute(bus, addr, cmd, I2C_FLAG_BYTE_DATA, 0, value, 0);
}

int i2c_smbus_read_word(i2c_bus_t* bus, uint8_t addr, uint8_t cmd)
{
    int ret = i2c_smbus_execute(bus, addr, cmd, I2C_FLAG_WORD_DATA, 1, 0, 0);
    if (ret != I2C_STATUS_OK) return ret;
    return (int)i2c_smbus_read_word_io(bus->base_io, I2C_SMBUS_HST_D0);
}

int i2c_smbus_write_word(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint16_t value)
{
    return i2c_smbus_execute(bus, addr, cmd, I2C_FLAG_WORD_DATA, 0, 0, value);
}

int i2c_smbus_block_read(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint8_t* buf, uint32_t max_len)
{
    if (!bus || !buf || max_len > I2C_SMBUS_MAX_BLOCK) return I2C_STATUS_ERROR;
    if (max_len == 0) return I2C_STATUS_ERROR;

    spin_lock(&bus->lock);

    int ret = i2c_smbus_wait_until_ready(bus);
    if (ret != I2C_STATUS_OK) {
        spin_unlock(&bus->lock);
        return ret;
    }

    i2c_smbus_clear_status(bus);

    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_ADDR, (addr << 1) | 1);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CMD, cmd);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_D0, (uint8_t)(max_len & 0x1F));

    uint8_t control = I2C_FLAG_BLOCK_DATA | I2C_SMBUS_START | I2C_SMBUS_SMBUS_CMD;
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, control);

    ret = i2c_smbus_wait_until_done(bus);
    if (ret != I2C_STATUS_OK) {
        spin_unlock(&bus->lock);
        return ret;
    }

    uint8_t block_len = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_D0);
    if (block_len == 0 || block_len > I2C_SMBUS_MAX_BLOCK) {
        spin_unlock(&bus->lock);
        return I2C_STATUS_ERROR;
    }
    if (block_len > max_len) block_len = (uint8_t)max_len;

    for (uint32_t i = 0; i < block_len; i++) {
        buf[i] = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_BLOCK + i);
    }

    spin_unlock(&bus->lock);
    return (int)block_len;
}

int i2c_smbus_block_write(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, const uint8_t* buf, uint32_t len)
{
    if (!bus || !buf || len == 0 || len > I2C_SMBUS_MAX_BLOCK) return I2C_STATUS_ERROR;

    spin_lock(&bus->lock);

    int ret = i2c_smbus_wait_until_ready(bus);
    if (ret != I2C_STATUS_OK) {
        spin_unlock(&bus->lock);
        return ret;
    }

    i2c_smbus_clear_status(bus);

    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_ADDR, (addr << 1) | 0);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CMD, cmd);
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_D0, (uint8_t)(len & 0x1F));

    for (uint32_t i = 0; i < len; i++) {
        i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_BLOCK + i, buf[i]);
    }

    uint8_t control = I2C_FLAG_BLOCK_DATA | I2C_SMBUS_START | I2C_SMBUS_SMBUS_CMD;
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, control);

    ret = i2c_smbus_wait_until_done(bus);
    if (ret != I2C_STATUS_OK) {
        spin_unlock(&bus->lock);
        return ret;
    }

    spin_unlock(&bus->lock);
    return I2C_STATUS_OK;
}

int i2c_smbus_process_call(i2c_bus_t* bus, uint8_t addr, uint8_t cmd, uint16_t value)
{
    int ret = i2c_smbus_execute(bus, addr, cmd, I2C_FLAG_PROCESS_CALL, 0, 0, value);
    if (ret != I2C_STATUS_OK) return ret;
    return (int)i2c_smbus_read_word_io(bus->base_io, I2C_SMBUS_HST_D0);
}

int i2c_smbus_quick_cmd(i2c_bus_t* bus, uint8_t addr, int read_write)
{
    return i2c_smbus_execute(bus, addr, 0, I2C_FLAG_QUICK, read_write & 1, 0, 0);
}

void i2c_transfer_start(i2c_bus_t* bus)
{
    if (!bus || !bus->active) return;
    spin_lock(&bus->lock);
    i2c_smbus_clear_status(bus);
    uint8_t control = I2C_FLAG_BYTE | I2C_SMBUS_START | I2C_SMBUS_SMBUS_CMD;
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, control);
    spin_unlock(&bus->lock);
}

void i2c_transfer_stop(i2c_bus_t* bus)
{
    if (!bus || !bus->active) return;
    spin_lock(&bus->lock);
    uint8_t status = i2c_smbus_read(bus->base_io, I2C_SMBUS_HST_STS);
    if (status & I2C_SMBUS_HOST_BUSY) {
        i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, I2C_SMBUS_KILL);
        for (volatile int d = 0; d < 100; d++);
        i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_STS, I2C_SMBUS_HOST_CLEAR);
    }
    spin_unlock(&bus->lock);
}

void i2c_transfer_ack(i2c_bus_t* bus)
{
    if (!bus || !bus->active) return;
    spin_lock(&bus->lock);
    i2c_smbus_clear_status(bus);
    uint8_t control = I2C_SMBUS_START | I2C_SMBUS_LAST_BYTE | I2C_SMBUS_SMBUS_CMD;
    i2c_smbus_write(bus->base_io, I2C_SMBUS_HST_CNT, control);
    spin_unlock(&bus->lock);
}

void i2c_transfer_nack(i2c_bus_t* bus)
{
    if (!bus || !bus->active) return;
    i2c_transfer_stop(bus);
}

i2c_bus_t* i2c_get_bus(uint8_t bus_id)
{
    if (bus_id >= i2c_bus_count_val) return NULL;
    if (!i2c_buses[bus_id].active) return NULL;
    return &i2c_buses[bus_id];
}

int i2c_bus_count(void)
{
    return i2c_bus_count_val;
}
