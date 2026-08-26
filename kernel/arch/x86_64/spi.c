#include <arch/spi.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/pci.h>
#include <string.h>

static spi_bus_t spi_buses[SPI_MAX_BUSES];
static int spi_bus_count_val = 0;
static spinlock_t spi_global_lock = SPINLOCK_INIT;

static inline void spi_bitbang_set_cs(spi_bus_t* bus, uint8_t cs, int active)
{
    if (!bus || !bus->active || cs >= SPI_MAX_CS) return;
    if (bus->set_cs) {
        bus->set_cs(bus, cs, active);
    }
}

static inline int spi_bitbang_wait_ready(spi_bus_t* bus)
{
    if (bus->base_mmio) {
        uint32_t timeout = 1000000;
        while (timeout--) {
            uint32_t status = inl(bus->base_mmio + 0x0C);
            if (status & 0x01) return 0;
            __asm__ volatile("pause");
        }
        return -1;
    }
    return 0;
}

int spi_bus_register(spi_bus_t* bus)
{
    if (!bus) return -1;
    if (spi_bus_count_val >= SPI_MAX_BUSES) return -1;

    spin_lock(&spi_global_lock);

    for (int i = 0; i < spi_bus_count_val; i++) {
        if (spi_buses[i].bus_id == bus->bus_id) {
            spin_unlock(&spi_global_lock);
            return -1;
        }
    }

    bus->bus_id = (uint8_t)spi_bus_count_val;
    bus->device_count = 0;
    bus->active = 1;
    spin_init(&bus->lock);

    for (int i = 0; i < SPI_MAX_DEVICES; i++) {
        bus->devices[i] = NULL;
    }

    if (bus->set_mode) {
        bus->set_mode(bus, SPI_MODE_0);
    }
    if (bus->set_bits) {
        bus->set_bits(bus, 8);
    }
    if (bus->set_speed && bus->clock_hz) {
        bus->set_speed(bus, bus->clock_hz / 2);
    }

    spi_buses[spi_bus_count_val] = *bus;
    spi_bus_count_val++;

    spin_unlock(&spi_global_lock);
    return 0;
}

int spi_bus_unregister(uint8_t bus_id)
{
    if (bus_id >= spi_bus_count_val) return -1;

    spin_lock(&spi_global_lock);

    spi_bus_t* bus = &spi_buses[bus_id];
    bus->active = 0;

    for (int i = 0; i < bus->device_count; i++) {
        bus->devices[i] = NULL;
    }
    bus->device_count = 0;

    spin_unlock(&spi_global_lock);
    return 0;
}

spi_bus_t* spi_get_bus(uint8_t bus_id)
{
    if (bus_id >= spi_bus_count_val) return NULL;
    if (!spi_buses[bus_id].active) return NULL;
    return &spi_buses[bus_id];
}

int spi_device_register(spi_bus_t* bus, spi_device_t* device)
{
    if (!bus || !device) return -1;
    if (!bus->active) return -1;
    if (device->chip_select >= SPI_MAX_CS) return -1;
    if (bus->device_count >= SPI_MAX_DEVICES) return -1;

    spin_lock(&bus->lock);

    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i] && bus->devices[i]->chip_select == device->chip_select) {
            spin_unlock(&bus->lock);
            return -1;
        }
    }

    device->registered = 1;
    bus->devices[bus->device_count] = device;
    bus->device_count++;

    spin_unlock(&bus->lock);
    return 0;
}

int spi_device_unregister(spi_bus_t* bus, uint8_t cs)
{
    if (!bus || !bus->active) return -1;

    spin_lock(&bus->lock);

    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i] && bus->devices[i]->chip_select == cs) {
            bus->devices[i]->registered = 0;
            for (int j = i; j < bus->device_count - 1; j++) {
                bus->devices[j] = bus->devices[j + 1];
            }
            bus->devices[bus->device_count - 1] = NULL;
            bus->device_count--;
            spin_unlock(&bus->lock);
            return 0;
        }
    }

    spin_unlock(&bus->lock);
    return -1;
}

spi_device_t* spi_get_device(spi_bus_t* bus, uint8_t cs)
{
    if (!bus || !bus->active) return NULL;

    spin_lock(&bus->lock);

    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i] && bus->devices[i]->chip_select == cs) {
            spi_device_t* dev = bus->devices[i];
            spin_unlock(&bus->lock);
            return dev;
        }
    }

    spin_unlock(&bus->lock);
    return NULL;
}

int spi_transfer(spi_bus_t* bus, uint8_t cs, const uint8_t* tx_buf, uint8_t* rx_buf, uint32_t len)
{
    if (!bus || !bus->active) return -1;
    if (len == 0) return 0;
    if (cs >= SPI_MAX_CS) return -1;

    spin_lock(&bus->lock);

    spi_bitbang_set_cs(bus, cs, 1);

    if (bus->transfer) {
        int ret = bus->transfer(bus, cs, tx_buf, rx_buf, len);
        if (ret != 0) {
            spi_bitbang_set_cs(bus, cs, 0);
            spin_unlock(&bus->lock);
            return ret;
        }
    } else if (bus->base_mmio) {
        for (uint32_t i = 0; i < len; i++) {
            uint8_t tx_byte = tx_buf ? tx_buf[i] : 0xFF;

            outb(bus->base_mmio + 0x00, tx_byte);

            spi_bitbang_wait_ready(bus);

            if (rx_buf) {
                rx_buf[i] = (uint8_t)inb(bus->base_mmio + 0x00);
            }
        }
    }

    spi_bitbang_set_cs(bus, cs, 0);

    spin_unlock(&bus->lock);
    return 0;
}

int spi_write(spi_bus_t* bus, uint8_t cs, const uint8_t* tx_buf, uint32_t len)
{
    return spi_transfer(bus, cs, tx_buf, NULL, len);
}

int spi_read(spi_bus_t* bus, uint8_t cs, uint8_t* rx_buf, uint32_t len)
{
    return spi_transfer(bus, cs, NULL, rx_buf, len);
}

int spi_write_then_read(spi_bus_t* bus, uint8_t cs,
                         const uint8_t* tx_buf, uint32_t tx_len,
                         uint8_t* rx_buf, uint32_t rx_len)
{
    if (!bus || !bus->active) return -1;

    spin_lock(&bus->lock);

    spi_bitbang_set_cs(bus, cs, 1);

    if (bus->transfer) {
        int ret = bus->transfer(bus, cs, tx_buf, NULL, tx_len);
        if (ret != 0) {
            spi_bitbang_set_cs(bus, cs, 0);
            spin_unlock(&bus->lock);
            return ret;
        }
    } else {
        for (uint32_t i = 0; i < tx_len; i++) {
            uint8_t tx_byte = tx_buf ? tx_buf[i] : 0xFF;
            outb(bus->base_mmio + 0x00, tx_byte);
            spi_bitbang_wait_ready(bus);
        }
    }

    if (bus->transfer) {
        int ret = bus->transfer(bus, cs, NULL, rx_buf, rx_len);
        if (ret != 0) {
            spi_bitbang_set_cs(bus, cs, 0);
            spin_unlock(&bus->lock);
            return ret;
        }
    } else {
        for (uint32_t i = 0; i < rx_len; i++) {
            outb(bus->base_mmio + 0x00, 0xFF);
            spi_bitbang_wait_ready(bus);
            if (rx_buf) {
                rx_buf[i] = (uint8_t)inb(bus->base_mmio + 0x00);
            }
        }
    }

    spi_bitbang_set_cs(bus, cs, 0);

    spin_unlock(&bus->lock);
    return 0;
}

int spi_bus_count(void)
{
    return spi_bus_count_val;
}
