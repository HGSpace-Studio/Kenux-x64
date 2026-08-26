#include <arch/ahci.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <string.h>

static ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
static uint8_t ahci_controller_count = 0;
static uint64_t ahci_port_dma[AHCI_MAX_CONTROLLERS][AHCI_MAX_PORTS];

static inline uint32_t ahci_r32(uint32_t base, uint32_t off)
{
    return *(volatile uint32_t*)(uintptr_t)(base + off);
}

static inline void ahci_w32(uint32_t base, uint32_t off, uint32_t v)
{
    *(volatile uint32_t*)(uintptr_t)(base + off) = v;
}

#define AHCI_GHC     0x04
#define AHCI_PI      0x0C
#define AHCI_POFF    0x100
#define AHCI_PSIZE   0x80
#define AHCI_PCMD    0x18
#define AHCI_PTFD    0x20
#define AHCI_PSIG    0x24
#define AHCI_PSSTS   0x28
#define AHCI_PCI     0x38
#define AHCI_PIS     0x10

#define CMD_ST       0x0001
#define CMD_FRE      0x0010
#define CMD_FR       0x4000
#define CMD_CR       0x8000

#define TFD_BSY      0x80
#define TFD_DRQ      0x08

#define FIS_H2D      0x27
#define ATA_READ_DMA_EXT  0x25
#define ATA_WRITE_DMA_EXT 0x35

typedef struct {
    uint16_t cfl:5, a:1, w:1, p:1, r:1, b:1, c:1, rsv0:1, pmp:4, prdtl:16;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv[4];
} __attribute__((packed)) ahci_ch_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t cmd;
    uint8_t feat;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t dev;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feat_exp;
    uint8_t cnt_lo;
    uint8_t cnt_hi;
    uint8_t icc;
    uint8_t ctl;
    uint32_t rsv;
} __attribute__((packed)) fis_h2d_t;

typedef struct {
    fis_h2d_t cfis;
    uint8_t   pad1[0x40 - sizeof(fis_h2d_t)];
    uint8_t   acmd[0x10];
    uint8_t   pad2[0x20];
    struct {
        uint32_t dba;
        uint32_t dbau;
        uint32_t rsv;
        uint32_t dbc_i;
    } __attribute__((packed)) prd;
} __attribute__((packed)) ahci_ct_t;

static int ahci_port_cmd(ahci_controller_t* ctrl, uint8_t port, uint64_t lba, void* buf, int write)
{
    if (port >= AHCI_MAX_PORTS) return -1;
    if (!(ctrl->ports_implemented & (1U << port))) return -1;

    uint32_t pbase = ctrl->base + AHCI_POFF + port * AHCI_PSIZE;
    if (ahci_r32(ctrl->base, pbase + AHCI_PTFD) & (TFD_BSY | TFD_DRQ)) return -1;

    uint64_t page = ahci_port_dma[ctrl - ahci_controllers][port];
    if (page == 0) return -1;

    ahci_ch_t* ch = (ahci_ch_t*)(uintptr_t)page;
    ahci_ct_t* ct = (ahci_ct_t*)(uintptr_t)(page + 0x500);
    uint8_t* fis = (uint8_t*)(uintptr_t)(page + 0x400);

    memset(ch, 0, sizeof(*ch));
    memset(ct, 0, sizeof(*ct));
    memset(fis, 0, 256);

    ch->cfl = 5;
    ch->w = write ? 1 : 0;
    ch->prdtl = 1;
    ch->ctba = (uint32_t)(page + 0x500);
    ch->ctbau = 0;

    ct->cfis.type = FIS_H2D;
    ct->cfis.flags = 0x80;
    ct->cfis.cmd = write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT;
    ct->cfis.lba0 = (uint8_t)lba;
    ct->cfis.lba1 = (uint8_t)(lba >> 8);
    ct->cfis.lba2 = (uint8_t)(lba >> 16);
    ct->cfis.dev = 0x40;
    ct->cfis.lba3 = (uint8_t)(lba >> 24);
    ct->cfis.lba4 = (uint8_t)(lba >> 32);
    ct->cfis.lba5 = (uint8_t)(lba >> 40);
    ct->cfis.cnt_lo = 1;
    ct->cfis.cnt_hi = 0;

    uint64_t buf_phys = (uint64_t)(uintptr_t)buf;
    ct->prd.dba = (uint32_t)buf_phys;
    ct->prd.dbau = (uint32_t)(buf_phys >> 32);
    ct->prd.dbc_i = (511U | (1U << 31));

    ahci_w32(ctrl->base, pbase + AHCI_PIS, 0xFFFFFFFF);
    ahci_w32(ctrl->base, pbase + AHCI_PCI, 1);

    for (int i = 0; i < 1000000; i++) {
        if (!(ahci_r32(ctrl->base, pbase + AHCI_PCI) & 1)) {
            if (ahci_r32(ctrl->base, pbase + AHCI_PIS) & (1U << 30)) {
                ahci_w32(ctrl->base, pbase + AHCI_PIS, (1U << 30));
                return -1;
            }
            return 0;
        }
    }

    return -1;
}

void ahci_init(void)
{
    memset(ahci_controllers, 0, sizeof(ahci_controllers));
    ahci_controller_count = 0;
    memset(ahci_port_dma, 0, sizeof(ahci_port_dma));

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t class_code_reg = pci_read_config(b, d, f, 0x08);
                if (class_code_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint8_t class_code = (class_code_reg >> 24) & 0xFF;
                uint8_t subclass = (class_code_reg >> 16) & 0xFF;
                if (class_code == 0x01 && subclass == 0x06) {
                    if (ahci_controller_count < AHCI_MAX_CONTROLLERS) {
                        ahci_controller_t* ctrl = &ahci_controllers[ahci_controller_count++];
                        ctrl->bus = b;
                        ctrl->device = d;
                        ctrl->function = f;
                        ctrl->base = pci_read_config(b, d, f, 0x24) & 0xFFFFFFF0;
                        ctrl->capabilities = ahci_r32(ctrl->base, 0x00);
                        ctrl->ports_implemented = ahci_r32(ctrl->base, AHCI_PI);

                        uint32_t ghc = ahci_r32(ctrl->base, AHCI_GHC);
                        if (!(ghc & (1U << 31))) {
                            ahci_w32(ctrl->base, AHCI_GHC, ghc | (1U << 31));
                        }

                        for (int p = 0; p < AHCI_MAX_PORTS; p++) {
                            if (ctrl->ports_implemented & (1U << p)) {
                                uint32_t cmd_off = AHCI_POFF + p * AHCI_PSIZE + AHCI_PCMD;
                                uint32_t cmd = ahci_r32(ctrl->base, cmd_off);
                                if (cmd & CMD_ST) {
                                    ahci_w32(ctrl->base, cmd_off, cmd & ~CMD_ST);
                                    while (ahci_r32(ctrl->base, cmd_off) & CMD_CR) {}
                                }
                                if (cmd & CMD_FRE) {
                                    ahci_w32(ctrl->base, cmd_off, cmd & ~CMD_FRE);
                                    while (ahci_r32(ctrl->base, cmd_off) & CMD_FR) {}
                                }

                                uint64_t phys = memory_alloc_physical(1);
                                if (phys == 0) continue;
                                memset((void*)(uintptr_t)phys, 0, PAGE_SIZE);
                                ahci_port_dma[ahci_controller_count - 1][p] = phys;

                                ahci_w32(ctrl->base, AHCI_POFF + p * AHCI_PSIZE + 0x00, (uint32_t)phys);
                                ahci_w32(ctrl->base, AHCI_POFF + p * AHCI_PSIZE + 0x04, (uint32_t)(phys >> 32));
                                ahci_w32(ctrl->base, AHCI_POFF + p * AHCI_PSIZE + 0x08, (uint32_t)(phys + 0x400));
                                ahci_w32(ctrl->base, AHCI_POFF + p * AHCI_PSIZE + 0x0C, 0);
                                ahci_w32(ctrl->base, AHCI_POFF + p * AHCI_PSIZE + AHCI_PIS, 0xFFFFFFFF);
                                ahci_w32(ctrl->base, cmd_off, CMD_FRE | CMD_ST);

                                ctrl->ports[p] = (ahci_port_t*)(uintptr_t)(ctrl->base + AHCI_POFF + p * AHCI_PSIZE);
                            }
                        }
                    }
                }
            }
        }
    }
}

ahci_controller_t* ahci_get_controller(uint8_t index)
{
    if (index < ahci_controller_count) {
        return &ahci_controllers[index];
    }
    return NULL;
}

uint8_t ahci_get_controller_count(void)
{
    return ahci_controller_count;
}

int ahci_read_sector(uint8_t port, uint64_t lba, void* buffer)
{
    if (ahci_controller_count == 0) return -1;
    if (!buffer) return -1;
    return ahci_port_cmd(&ahci_controllers[0], port, lba, buffer, 0);
}

int ahci_write_sector(uint8_t port, uint64_t lba, const void* buffer)
{
    if (ahci_controller_count == 0) return -1;
    if (!buffer) return -1;
    return ahci_port_cmd(&ahci_controllers[0], port, lba, (void*)buffer, 1);
}
