#include "ahci.h"
#include <arch/memory.h>
#include <string.h>

static volatile uint32_t* ahci_port_reg(volatile uint32_t* hba, int port, uint32_t offset)
{
    return (volatile uint32_t*)((uintptr_t)hba + 0x100 + port * 0x80 + offset);
}

static void ahci_wait(volatile uint32_t* reg, uint32_t mask, uint32_t val, uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms * 1000; i++) {
        if ((*reg & mask) == val) return;
        for (volatile int d = 0; d < 100; d++);
    }
}

int ahci_port_cmd_slot(ahci_port_t* port)
{
    if (!port || !port->port_regs) return -1;
    volatile uint32_t* ci = &port->port_regs[AHCI_PX_CI / 4];
    volatile uint32_t* sact = &port->port_regs[AHCI_PX_SACT / 4];
    uint32_t slots = *ci | *sact;
    for (int i = 0; i < AHCI_MAX_SLOTS; i++) {
        if (!(slots & (1 << i))) return i;
    }
    return -1;
}

int ahci_port_start(ahci_hba_t* hba, int port)
{
    if (!hba || port < 0 || port >= AHCI_MAX_PORTS) return -1;
    ahci_port_t* p = &hba->ports[port];
    if (!p->port_regs) return -2;

    volatile uint32_t* cmd = &p->port_regs[AHCI_PX_CMD / 4];
    while (*cmd & AHCI_CMD_CR);

    uint32_t cmd_val = *cmd;
    cmd_val |= AHCI_CMD_FRE | AHCI_CMD_ST;
    *cmd = cmd_val;

    return 0;
}

int ahci_port_stop(ahci_hba_t* hba, int port)
{
    if (!hba || port < 0 || port >= AHCI_MAX_PORTS) return -1;
    ahci_port_t* p = &hba->ports[port];
    if (!p->port_regs) return -2;

    volatile uint32_t* cmd = &p->port_regs[AHCI_PX_CMD / 4];
    *cmd &= ~AHCI_CMD_ST;

    while (*cmd & AHCI_CMD_CR);
    *cmd &= ~AHCI_CMD_FRE;
    while (*cmd & AHCI_CMD_FR);

    return 0;
}

int ahci_identify(ahci_port_t* port, uint16_t* identify)
{
    if (!port || !identify || !port->port_regs) return -1;

    spinlock_acquire(&port->lock);
    int slot = ahci_port_cmd_slot(port);
    if (slot < 0) { spinlock_release(&port->lock); return -2; }

    ahci_cmd_header_t* hdr = &port->cmd_headers[slot];
    memset(hdr, 0, sizeof(ahci_cmd_header_t));
    hdr->flags = 5;
    hdr->prdt_length = 1;
    hdr->cmd_table_addr = (uint64_t)(uintptr_t)port->cmd_tables;

    ahci_cmd_table_t* tbl = port->cmd_tables;
    memset(tbl, 0, sizeof(ahci_cmd_table_t));
    tbl->cfis[0] = 0x27;
    tbl->cfis[1] = 0;
    tbl->cfis[2] = 0xEC;
    tbl->cfis[3] = 0;
    tbl->cfis[4] = 0;
    tbl->cfis[5] = 0;
    tbl->cfis[6] = 0xA0;
    tbl->cfis[7] = 0;
    tbl->cfis[15] = 0x05;

    port->prdt[0].data_base = (uint64_t)(uintptr_t)identify;
    port->prdt[0].data_byte_count = 511;

    volatile uint32_t* ci = &port->port_regs[AHCI_PX_CI / 4];
    *ci = 1 << slot;

    for (int i = 0; i < 1000000; i++) {
        if (!(*ci & (1 << slot))) break;
    }

    volatile uint32_t* tfd = &port->port_regs[AHCI_PX_TFD / 4];
    int result = (*tfd & (AHCI_TFD_STS_ERR | AHCI_TFD_STS_BSY)) ? -3 : 0;

    spinlock_release(&port->lock);
    return result;
}

int ahci_read(ahci_port_t* port, uint64_t lba, void* buf, uint32_t count)
{
    if (!port || !buf || !port->port_regs) return -1;

    spinlock_acquire(&port->lock);
    int slot = ahci_port_cmd_slot(port);
    if (slot < 0) { spinlock_release(&port->lock); return -2; }

    ahci_cmd_header_t* hdr = &port->cmd_headers[slot];
    memset(hdr, 0, sizeof(ahci_cmd_header_t));
    hdr->flags = 5 | AHCI_CMD_HDR_READ;
    hdr->prdt_length = 1;
    hdr->cmd_table_addr = (uint64_t)(uintptr_t)port->cmd_tables;

    ahci_cmd_table_t* tbl = port->cmd_tables;
    memset(tbl, 0, sizeof(ahci_cmd_table_t));

    tbl->cfis[0] = 0x27;
    tbl->cfis[1] = 1;
    tbl->cfis[2] = 0x24;
    tbl->cfis[3] = 0;
    tbl->cfis[4] = (uint8_t)(lba & 0xFF);
    tbl->cfis[5] = (uint8_t)((lba >> 8) & 0xFF);
    tbl->cfis[6] = (uint8_t)((lba >> 16) & 0xFF);
    tbl->cfis[7] = 0xE0 | (uint8_t)((lba >> 24) & 0x0F);
    tbl->cfis[8] = (uint8_t)((lba >> 24) & 0xFF);
    tbl->cfis[9] = (uint8_t)((lba >> 32) & 0xFF);
    tbl->cfis[10] = (uint8_t)((lba >> 40) & 0xFF);
    tbl->cfis[12] = (uint8_t)(count & 0xFF);
    tbl->cfis[13] = (uint8_t)((count >> 8) & 0xFF);

    port->prdt[0].data_base = (uint64_t)(uintptr_t)buf;
    port->prdt[0].data_byte_count = count * 512 - 1;

    volatile uint32_t* ci = &port->port_regs[AHCI_PX_CI / 4];
    *ci = 1 << slot;

    for (int i = 0; i < 10000000; i++) {
        if (!(*ci & (1 << slot))) break;
    }

    volatile uint32_t* tfd = &port->port_regs[AHCI_PX_TFD / 4];
    int result = (*tfd & (AHCI_TFD_STS_ERR | AHCI_TFD_STS_BSY)) ? -3 : (int)(count * 512);

    spinlock_release(&port->lock);
    return result;
}

int ahci_write(ahci_port_t* port, uint64_t lba, const void* buf, uint32_t count)
{
    if (!port || !buf || !port->port_regs) return -1;

    spinlock_acquire(&port->lock);
    int slot = ahci_port_cmd_slot(port);
    if (slot < 0) { spinlock_release(&port->lock); return -2; }

    ahci_cmd_header_t* hdr = &port->cmd_headers[slot];
    memset(hdr, 0, sizeof(ahci_cmd_header_t));
    hdr->flags = 5 | AHCI_CMD_HDR_WRITE;
    hdr->prdt_length = 1;
    hdr->cmd_table_addr = (uint64_t)(uintptr_t)port->cmd_tables;

    ahci_cmd_table_t* tbl = port->cmd_tables;
    memset(tbl, 0, sizeof(ahci_cmd_table_t));

    tbl->cfis[0] = 0x27;
    tbl->cfis[1] = 1;
    tbl->cfis[2] = 0x34;
    tbl->cfis[3] = 0;
    tbl->cfis[4] = (uint8_t)(lba & 0xFF);
    tbl->cfis[5] = (uint8_t)((lba >> 8) & 0xFF);
    tbl->cfis[6] = (uint8_t)((lba >> 16) & 0xFF);
    tbl->cfis[7] = 0xE0 | (uint8_t)((lba >> 24) & 0x0F);
    tbl->cfis[8] = (uint8_t)((lba >> 24) & 0xFF);
    tbl->cfis[9] = (uint8_t)((lba >> 32) & 0xFF);
    tbl->cfis[10] = (uint8_t)((lba >> 40) & 0xFF);
    tbl->cfis[12] = (uint8_t)(count & 0xFF);
    tbl->cfis[13] = (uint8_t)((count >> 8) & 0xFF);

    port->prdt[0].data_base = (uint64_t)(uintptr_t)buf;
    port->prdt[0].data_byte_count = count * 512 - 1;

    volatile uint32_t* ci = &port->port_regs[AHCI_PX_CI / 4];
    *ci = 1 << slot;

    for (int i = 0; i < 10000000; i++) {
        if (!(*ci & (1 << slot))) break;
    }

    volatile uint32_t* tfd = &port->port_regs[AHCI_PX_TFD / 4];
    int result = (*tfd & (AHCI_TFD_STS_ERR | AHCI_TFD_STS_BSY)) ? -3 : (int)(count * 512);

    spinlock_release(&port->lock);
    return result;
}

int ahci_init(ahci_hba_t* hba, pci_device_t* pci_dev)
{
    if (!hba || !pci_dev) return -1;

    memset(hba, 0, sizeof(ahci_hba_t));
    spin_init(&hba->lock);
    hba->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    hba->hba_regs = (volatile uint32_t*)pci_map_bar(pci_dev, 5);
    if (!hba->hba_regs) return -2;

    hba->cap = hba->hba_regs[AHCI_HBA_CAP / 4];
    hba->cap2 = hba->hba_regs[AHCI_HBA_CAP2 / 4];
    hba->port_impl = hba->hba_regs[AHCI_HBA_PI / 4];
    hba->n_cmd_slots = (int)((hba->cap & 0x1F00) >> 8) + 1;
    hba->is_64bit = (hba->cap & (1 << 31)) ? 1 : 0;

    uint32_t ghc = hba->hba_regs[AHCI_HBA_GHC / 4];
    if (!(ghc & AHCI_GHC_AE)) {
        hba->hba_regs[AHCI_HBA_GHC / 4] |= AHCI_GHC_AE;
    }
    hba->hba_regs[AHCI_HBA_GHC / 4] |= AHCI_GHC_IE;

    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (!(hba->port_impl & (1 << i))) continue;

        ahci_port_t* p = &hba->ports[i];
        p->port_num = i;
        p->port_regs = (volatile uint32_t*)((uintptr_t)hba->hba_regs + 0x100 + i * 0x80);
        spin_init(&p->lock);

        uint32_t ssts = p->port_regs[AHCI_PX_SSTS / 4];
        uint8_t det = (uint8_t)(ssts & 0x0F);
        if (det != 3 && det != 1) continue;

        p->sig = p->port_regs[AHCI_PX_SIG / 4];
        p->present = 1;

        ahci_port_stop(hba, i);

        p->cmd_headers = (ahci_cmd_header_t*)memory_alloc_aligned(
            sizeof(ahci_cmd_header_t) * hba->n_cmd_slots, 1024);
        p->cmd_tables = (ahci_cmd_table_t*)memory_alloc_aligned(
            sizeof(ahci_cmd_table_t) * hba->n_cmd_slots, 128);
        p->prdt = (ahci_prdt_entry_t*)memory_alloc_aligned(
            sizeof(ahci_prdt_entry_t) * AHCI_PRDT_MAX * hba->n_cmd_slots, 128);

        if (!p->cmd_headers || !p->cmd_tables || !p->prdt) continue;

        p->port_regs[AHCI_PX_CLB / 4] = (uint32_t)(uintptr_t)p->cmd_headers;
        p->port_regs[AHCI_PX_CLBU / 4] = (uint32_t)((uintptr_t)p->cmd_headers >> 32);
        p->port_regs[AHCI_PX_FB / 4] = 0;
        p->port_regs[AHCI_PX_FBU / 4] = 0;

        for (int s = 0; s < hba->n_cmd_slots; s++) {
            p->cmd_headers[s].cmd_table_addr = (uint64_t)(uintptr_t)&p->cmd_tables[s];
        }

        ahci_port_start(hba, i);

        uint16_t identify[256];
        int result = ahci_identify(p, identify);
        if (result == 0) {
            p->sector_size = 512;
            uint64_t lba48 = (uint64_t)identify[100] | ((uint64_t)identify[101] << 16) |
                             ((uint64_t)identify[102] << 32) | ((uint64_t)identify[103] << 48);
            p->sector_count = lba48 > 0 ? lba48 : (uint32_t)(identify[60] | (identify[61] << 16));
            if (identify[106] & 0x0C00) {
                p->sector_size = (uint32_t)identify[117] | ((uint32_t)identify[118] << 16);
                if (p->sector_size < 512) p->sector_size = 512;
            }
        }

        hba->n_ports++;
    }

    return 0;
}

void ahci_shutdown(ahci_hba_t* hba)
{
    if (!hba) return;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (hba->ports[i].present) {
            ahci_port_stop(hba, i);
        }
    }
}

void ahci_irq_handler(int irq, void* ctx)
{
    ahci_hba_t* hba = (ahci_hba_t*)ctx;
    if (!hba) return;

    uint32_t is = hba->hba_regs[AHCI_HBA_IS / 4];
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (is & (1 << i)) {
            ahci_port_t* p = &hba->ports[i];
            if (p->port_regs) {
                p->port_regs[AHCI_PX_IS / 4] = 0xFFFFFFFF;
            }
        }
    }
    hba->hba_regs[AHCI_HBA_IS / 4] = is;
    (void)irq;
}