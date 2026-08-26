#ifndef KAPI_IO_H
#define KAPI_IO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IO_MEM_UNCACHED     0x00
#define KAPI_IO_MEM_WRITECOMBINE 0x01
#define KAPI_IO_MEM_WRITEBACK    0x02
#define KAPI_IO_MEM_WRITETHROUGH 0x03

#define KAPI_IOMAP_READ          0x01
#define KAPI_IOMAP_WRITE         0x02
#define KAPI_IOMAP_EXEC          0x04

typedef struct {
    uint64_t phys_addr;
    uint64_t virt_addr;
    size_t   size;
    uint32_t flags;
    int      valid;
} kapi_iomap_t;

uint8_t  kapi_inb(uint16_t port);
uint16_t kapi_inw(uint16_t port);
uint32_t kapi_inl(uint16_t port);

void kapi_outb(uint16_t port, uint8_t value);
void kapi_outw(uint16_t port, uint16_t value);
void kapi_outl(uint16_t port, uint32_t value);

uint8_t  kapi_read8(uint64_t addr);
uint16_t kapi_read16(uint64_t addr);
uint32_t kapi_read32(uint64_t addr);
uint64_t kapi_read64(uint64_t addr);

void kapi_write8(uint64_t addr, uint8_t value);
void kapi_write16(uint64_t addr, uint16_t value);
void kapi_write32(uint64_t addr, uint32_t value);
void kapi_write64(uint64_t addr, uint64_t value);

kapi_iomap_t* kapi_ioremap(uint64_t phys_addr, size_t size, uint32_t flags);
void          kapi_iounmap(kapi_iomap_t* map);

int kapi_io_init(void);

#ifdef __cplusplus
}
#endif

#endif