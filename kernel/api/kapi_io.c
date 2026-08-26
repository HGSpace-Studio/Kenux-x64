#include "kapi_io.h"
#include "kapi.h"

#include <arch/io.h>
#include <arch/memory.h>
#include <string.h>

#define KAPI_MAX_IOMAP 64

static kapi_iomap_t kapi_iomap_table[KAPI_MAX_IOMAP];
static int kapi_io_initialized = 0;

int kapi_io_init(void)
{
    if (kapi_io_initialized) {
        return KAPI_OK;
    }

    memset(kapi_iomap_table, 0, sizeof(kapi_iomap_table));
    kapi_io_initialized = 1;
    return KAPI_OK;
}

uint8_t kapi_inb(uint16_t port)
{
    return inb(port);
}

uint16_t kapi_inw(uint16_t port)
{
    return inw(port);
}

uint32_t kapi_inl(uint16_t port)
{
    return inl(port);
}

void kapi_outb(uint16_t port, uint8_t value)
{
    outb(port, value);
}

void kapi_outw(uint16_t port, uint16_t value)
{
    outw(port, value);
}

void kapi_outl(uint16_t port, uint32_t value)
{
    outl(port, value);
}

uint8_t kapi_read8(uint64_t addr)
{
    volatile uint8_t* ptr = (volatile uint8_t*)(uintptr_t)addr;
    return *ptr;
}

uint16_t kapi_read16(uint64_t addr)
{
    volatile uint16_t* ptr = (volatile uint16_t*)(uintptr_t)addr;
    return *ptr;
}

uint32_t kapi_read32(uint64_t addr)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(uintptr_t)addr;
    return *ptr;
}

uint64_t kapi_read64(uint64_t addr)
{
    volatile uint64_t* ptr = (volatile uint64_t*)(uintptr_t)addr;
    return *ptr;
}

void kapi_write8(uint64_t addr, uint8_t value)
{
    volatile uint8_t* ptr = (volatile uint8_t*)(uintptr_t)addr;
    *ptr = value;
}

void kapi_write16(uint64_t addr, uint16_t value)
{
    volatile uint16_t* ptr = (volatile uint16_t*)(uintptr_t)addr;
    *ptr = value;
}

void kapi_write32(uint64_t addr, uint32_t value)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(uintptr_t)addr;
    *ptr = value;
}

void kapi_write64(uint64_t addr, uint64_t value)
{
    volatile uint64_t* ptr = (volatile uint64_t*)(uintptr_t)addr;
    *ptr = value;
}

kapi_iomap_t* kapi_ioremap(uint64_t phys_addr, size_t size, uint32_t flags)
{
    if (phys_addr == 0 || size == 0) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MAX_IOMAP; i++) {
        if (!kapi_iomap_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    void* virt = memory_alloc(size);
    if (!virt) {
        return NULL;
    }

    kapi_iomap_table[slot].phys_addr = phys_addr;
    kapi_iomap_table[slot].virt_addr = (uint64_t)(uintptr_t)virt;
    kapi_iomap_table[slot].size = size;
    kapi_iomap_table[slot].flags = flags;
    kapi_iomap_table[slot].valid = 1;

    return &kapi_iomap_table[slot];
}

void kapi_iounmap(kapi_iomap_t* map)
{
    if (!map || !map->valid) {
        return;
    }

    memory_free((void*)(uintptr_t)map->virt_addr);
    map->valid = 0;
    map->phys_addr = 0;
    map->virt_addr = 0;
    map->size = 0;
}