#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <arch/interrupt.h>
#include <arch/spinlock.h>
#include <arch/net.h>
#include <string.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E
#define E1000_82540EM   0x100F
#define E1000_82574L    0x10D3
#define E1000_82543GC   0x1004
#define E1000_82545EM   0x1007

#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EECD      0x0010
#define E1000_REG_EERD      0x0014
#define E1000_REG_CTRL_EXT  0x0018
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_IMS       0x00D0
#define E1000_REG_IMC       0x00D8
#define E1000_REG_ICR       0x00C0
#define E1000_REG_RCTL      0x0100
#define E1000_REG_TCTL      0x0400
#define E1000_REG_RDTR      0x2820
#define E1000_REG_RADV      0x282C
#define E1000_REG_TIDV      0x3840
#define E1000_REG_TADV      0x384C
#define E1000_REG_ITR       0x00C4

#define E1000_CTRL_FD           0x00000001
#define E1000_CTRL_LRST         0x00000008
#define E1000_CTRL_ASDE         0x00000020
#define E1000_CTRL_SLU          0x00000040
#define E1000_CTRL_ILOS         0x00000080
#define E1000_CTRL_RFCE         0x00000080
#define E1000_CTRL_TFCE         0x00000100
#define E1000_CTRL_ENC          0x00040000
#define E1000_CTRL_SDP0         0x00800000
#define E1000_CTRL_SDP1         0x01000000
#define E1000_CTRL_RST          0x04000000
#define E1000_CTRL_PHY_RST      0x80000000

#define E1000_TCTL_EN           0x00000002
#define E1000_TCTL_PSP          0x00000008
#define E1000_TCTL_CT           0x00000FF0
#define E1000_TCTL_COLD         0x003FF000

#define E1000_RCTL_EN           0x00000002
#define E1000_RCTL_UPE          0x00000008
#define E1000_RCTL_MPE          0x00000010
#define E1000_RCTL_LPE          0x00000020
#define E1000_RCTL_BAM          0x00008000
#define E1000_RCTL_BSIZE_2048   0x00000000
#define E1000_RCTL_BSIZE_4096   0x00030000
#define E1000_RCTL_BSIZE_8192   0x00040000
#define E1000_RCTL_BSIZE_16384  0x00050000
#define E1000_RCTL_SECRC        0x04000000

#define E1000_ICR_TXDW          0x00000001
#define E1000_ICR_TXQE          0x00000002
#define E1000_ICR_LSC           0x00000004
#define E1000_ICR_RXSEQ         0x00000008
#define E1000_ICR_RXDMT0        0x00000010
#define E1000_ICR_RXO           0x00000040
#define E1000_ICR_RXCFG         0x00000080
#define E1000_ICR_INT_ASSERTED  0x80000000

#define TX_DESC_COUNT   64
#define RX_DESC_COUNT   64
#define TX_BUFFER_SIZE  2048
#define RX_BUFFER_SIZE  2048
#define MAX_PACKET_SIZE 1518

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t padding;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t bar0;
    uint32_t bar1;
    volatile uint32_t* regs;
    int irq;
    
    e1000_tx_desc_t* tx_desc_ring;
    uint8_t* tx_buffers[TX_DESC_COUNT];
    uint32_t tx_tail;
    uint32_t tx_head;
    
    e1000_rx_desc_t* rx_desc_ring;
    uint8_t* rx_buffers[RX_DESC_COUNT];
    uint32_t rx_tail;
    uint32_t rx_head;
    
    uint8_t mac_addr[6];
    int link_up;
    uint32_t link_speed;
    
    spinlock_t lock;
    int tx_irq_enabled;
    int rx_irq_enabled;
} e1000_adapter_t;

static e1000_adapter_t g_e1000;
static int g_e1000_initialized = 0;

static uint32_t e1000_read_reg(e1000_adapter_t* adapter, uint32_t offset)
{
    return adapter->regs[offset / 4];
}

static void e1000_write_reg(e1000_adapter_t* adapter, uint32_t offset, uint32_t value)
{
    adapter->regs[offset / 4] = value;
}

static int e1000_wait_eeprom(e1000_adapter_t* adapter)
{
    uint32_t eecd = e1000_read_reg(adapter, E1000_REG_EECD);
    for (int i = 0; i < 1000; i++) {
        if (eecd & 0x00000010) {
            return 0;
        }
        eecd = e1000_read_reg(adapter, E1000_REG_EECD);
    }
    return -1;
}

static int e1000_read_eeprom_byte(e1000_adapter_t* adapter, uint8_t addr, uint8_t* data)
{
    if (e1000_wait_eeprom(adapter) != 0) return -1;
    
    e1000_write_reg(adapter, E1000_REG_EERD, 0x00000001 | ((uint32_t)addr << 8));
    
    if (e1000_wait_eeprom(adapter) != 0) return -1;
    
    uint32_t val = e1000_read_reg(adapter, E1000_REG_EERD);
    *data = (uint8_t)(val >> 16);
    return 0;
}

static int e1000_read_mac_address(e1000_adapter_t* adapter)
{
    uint8_t eeprom_mac[6];
    
    if (e1000_read_eeprom_byte(adapter, 0, &eeprom_mac[0]) == 0 &&
        e1000_read_eeprom_byte(adapter, 1, &eeprom_mac[1]) == 0 &&
        e1000_read_eeprom_byte(adapter, 2, &eeprom_mac[2]) == 0 &&
        e1000_read_eeprom_byte(adapter, 3, &eeprom_mac[3]) == 0 &&
        e1000_read_eeprom_byte(adapter, 4, &eeprom_mac[4]) == 0 &&
        e1000_read_eeprom_byte(adapter, 5, &eeprom_mac[5]) == 0) {
        
        adapter->mac_addr[0] = eeprom_mac[0];
        adapter->mac_addr[1] = eeprom_mac[1];
        adapter->mac_addr[2] = eeprom_mac[2];
        adapter->mac_addr[3] = eeprom_mac[3];
        adapter->mac_addr[4] = eeprom_mac[4];
        adapter->mac_addr[5] = eeprom_mac[5];
        return 0;
    }
    
    for (int i = 0; i < 3; i++) {
        uint32_t mac_low = e1000_read_reg(adapter, 0x5400 + i * 4);
        uint32_t mac_high = e1000_read_reg(adapter, 0x5404 + i * 4);
        
        if ((mac_low == 0xFFFFFFFF && mac_high == 0xFFFFFFFF) ||
            (mac_low == 0 && mac_high == 0)) {
            continue;
        }
        
        adapter->mac_addr[i * 2] = (uint8_t)(mac_low >> (i * 8)) & 0xFF;
        adapter->mac_addr[i * 2 + 1] = (uint8_t)(mac_low >> (i * 8 + 8)) & 0xFF;
    }
    
    return 0;
}

static void e1000_tx_init(e1000_adapter_t* adapter)
{
    uint64_t desc_phys;
    
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        if (adapter->tx_buffers[i]) {
            memory_free(adapter->tx_buffers[i]);
            adapter->tx_buffers[i] = NULL;
        }
        adapter->tx_buffers[i] = (uint8_t*)memory_alloc(TX_BUFFER_SIZE);
        if (!adapter->tx_buffers[i]) {
            continue;
        }
    }
    
    desc_phys = (uint64_t)memory_alloc_physical(1);
    if (!desc_phys) return;
    
    adapter->tx_desc_ring = (e1000_tx_desc_t*)desc_phys;
    memset((void*)adapter->tx_desc_ring, 0, sizeof(e1000_tx_desc_t) * TX_DESC_COUNT);
    
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        adapter->tx_desc_ring[i].addr = (uint64_t)memory_alloc_physical(1);
        adapter->tx_desc_ring[i].cmd = 0;
        adapter->tx_desc_ring[i].status = 0x01;
    }
    
    e1000_write_reg(adapter, E1000_REG_TDBAL, (uint32_t)desc_phys);
    e1000_write_reg(adapter, E1000_REG_TDBAH, (uint32_t)(desc_phys >> 32));
    e1000_write_reg(adapter, E1000_REG_TDLEN, sizeof(e1000_tx_desc_t) * TX_DESC_COUNT);
    e1000_write_reg(adapter, E1000_REG_TDH, 0);
    e1000_write_reg(adapter, E1000_REG_TDT, 0);
    
    adapter->tx_tail = 0;
    adapter->tx_head = 0;
}

static void e1000_rx_init(e1000_adapter_t* adapter)
{
    uint64_t desc_phys;
    
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        if (adapter->rx_buffers[i]) {
            memory_free(adapter->rx_buffers[i]);
            adapter->rx_buffers[i] = NULL;
        }
        adapter->rx_buffers[i] = (uint8_t*)memory_alloc(RX_BUFFER_SIZE);
        if (!adapter->rx_buffers[i]) {
            continue;
        }
    }
    
    desc_phys = (uint64_t)memory_alloc_physical(1);
    if (!desc_phys) return;
    
    adapter->rx_desc_ring = (e1000_rx_desc_t*)desc_phys;
    memset((void*)adapter->rx_desc_ring, 0, sizeof(e1000_rx_desc_t) * RX_DESC_COUNT);
    
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        adapter->rx_desc_ring[i].addr = (uint64_t)memory_alloc_physical(1);
        adapter->rx_desc_ring[i].length = RX_BUFFER_SIZE;
        adapter->rx_desc_ring[i].status = 0;
    }
    
    e1000_write_reg(adapter, E1000_REG_RDBAL, (uint32_t)desc_phys);
    e1000_write_reg(adapter, E1000_REG_RDBAH, (uint32_t)(desc_phys >> 32));
    e1000_write_reg(adapter, E1000_REG_RDLEN, sizeof(e1000_rx_desc_t) * RX_DESC_COUNT);
    e1000_write_reg(adapter, E1000_REG_RDH, 0);
    e1000_write_reg(adapter, E1000_REG_RDT, RX_DESC_COUNT - 1);
    
    adapter->rx_tail = 0;
    adapter->rx_head = 0;
}

static void e1000_hw_start(e1000_adapter_t* adapter)
{
    uint32_t ctrl = e1000_read_reg(adapter, E1000_REG_CTRL);
    ctrl |= E1000_CTRL_FD | E1000_CTRL_SLU | E1000_CTRL_ASDE;
    ctrl &= ~(E1000_CTRL_ILOS | E1000_CTRL_LRST);
    e1000_write_reg(adapter, E1000_REG_CTRL, ctrl);
    
    uint32_t tctl = e1000_read_reg(adapter, E1000_REG_TCTL);
    tctl |= E1000_TCTL_EN | E1000_TCTL_PSP;
    tctl &= ~(E1000_TCTL_CT | E1000_TCTL_COLD);
    tctl |= 0x10 << 4;
    tctl |= 0x40 << 12;
    e1000_write_reg(adapter, E1000_REG_TCTL, tctl);
    
    uint32_t rctl = e1000_read_reg(adapter, E1000_REG_RCTL);
    rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC;
    rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_LPE);
    e1000_write_reg(adapter, E1000_REG_RCTL, rctl);
}

static void e1000_enable_interrupts(e1000_adapter_t* adapter)
{
    e1000_write_reg(adapter, E1000_REG_IMS, 
                    E1000_ICR_TXDW | E1000_ICR_TXQE | E1000_ICR_LSC |
                    E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_INT_ASSERTED);
}

static void e1000_check_link_status(e1000_adapter_t* adapter)
{
    uint32_t status = e1000_read_reg(adapter, E1000_REG_STATUS);
    
    if (status & 0x02) {
        adapter->link_up = 1;
        
        if (status & 0x80) {
            adapter->link_speed = 1000;
        } else if (status & 0x40) {
            adapter->link_speed = 100;
        } else {
            adapter->link_speed = 10;
        }
    } else {
        adapter->link_up = 0;
        adapter->link_speed = 0;
    }
}

static int e1000_tx_packet(e1000_adapter_t* adapter, const void* data, uint64_t len)
{
    if (!adapter->link_up) return -1;
    if (len > TX_BUFFER_SIZE || len == 0) return -1;
    
    spin_lock(&adapter->lock);
    
    uint32_t next_tail = (adapter->tx_tail + 1) % TX_DESC_COUNT;
    if (next_tail == adapter->tx_head) {
        spin_unlock(&adapter->lock);
        return -1;
    }
    
    memcpy(adapter->tx_buffers[adapter->tx_tail], data, len);
    
    adapter->tx_desc_ring[adapter->tx_tail].addr = (uint64_t)memory_alloc_physical(1);
    adapter->tx_desc_ring[adapter->tx_tail].length = (uint16_t)len;
    adapter->tx_desc_ring[adapter->tx_tail].cmd = 0x0B;
    adapter->tx_desc_ring[adapter->tx_tail].status = 0;
    
    e1000_write_reg(adapter, E1000_REG_TDT, next_tail);
    adapter->tx_tail = next_tail;
    
    for (int wait = 0; wait < 1000; wait++) {
        uint32_t tdh = e1000_read_reg(adapter, E1000_REG_TDH);
        uint32_t tdt = e1000_read_reg(adapter, E1000_REG_TDT);
        
        if (adapter->tx_head != tdh) {
            adapter->tx_head = tdh;
            break;
        }
        
        if (adapter->tx_desc_ring[adapter->tx_head].status & 0x01) {
            adapter->tx_head = (adapter->tx_head + 1) % TX_DESC_COUNT;
            break;
        }
    }
    
    spin_unlock(&adapter->lock);
    return (int)len;
}

static int e1000_rx_packet(e1000_adapter_t* adapter, void* buf, uint64_t buf_size)
{
    spin_lock(&adapter->lock);
    
    uint32_t rdh = e1000_read_reg(adapter, E1000_REG_RDH);
    
    if (adapter->rx_head == rdh) {
        spin_unlock(&adapter->lock);
        return 0;
    }
    
    e1000_rx_desc_t* desc = &adapter->rx_desc_ring[adapter->rx_head];
    
    if (!(desc->status & 0x01)) {
        spin_unlock(&adapter->lock);
        return 0;
    }
    
    uint16_t len = desc->length;
    if (len > buf_size) len = (uint16_t)buf_size;
    
    memcpy(buf, adapter->rx_buffers[adapter->rx_head], len);
    
    desc->status = 0;
    
    uint32_t next_head = (adapter->rx_head + 1) % RX_DESC_COUNT;
    e1000_write_reg(adapter, E1000_REG_RDT, adapter->rx_head);
    adapter->rx_head = next_head;
    
    spin_unlock(&adapter->lock);
    return (int)len;
}

static void e1000_interrupt_handler(void* arg)
{
    e1000_adapter_t* adapter = (e1000_adapter_t*)arg;
    
    uint32_t icr = e1000_read_reg(adapter, E1000_REG_ICR);
    
    if (icr & E1000_ICR_LSC) {
        e1000_check_link_status(adapter);
    }
    
    if (icr & E1000_ICR_TXDW) {
    }
    
    if (icr & E1000_ICR_RXDMT0) {
    }
}

static int e1000_tx_from_net(const void* data, uint64_t len)
{
    if (!g_e1000_initialized) return -1;
    return e1000_tx_packet(&g_e1000, data, len);
}

int e1000_driver_init(void)
{
    uint8_t bus, dev, func;
    uint16_t device_id;
    uint32_t bar0, bar1;
    
    int found = -1;
    uint16_t ids[] = {
        E1000_DEVICE_ID, E1000_82540EM, E1000_82574L,
        E1000_82543GC, E1000_82545EM, 0
    };
    
    for (int i = 0; ids[i] != 0; i++) {
        if (pci_find_device(E1000_VENDOR_ID, ids[i], &bus, &dev, &func) == 0) {
            found = 0;
            device_id = ids[i];
            break;
        }
    }
    
    if (found < 0) {
        return -1;
    }
    
    g_e1000.bus = bus;
    g_e1000.device = dev;
    g_e1000.function = func;
    
    bar0 = pci_read_bar(bus, dev, func, 0);
    bar1 = pci_read_bar(bus, dev, func, 1);
    
    uint8_t irq_pin = pci_read_config_byte(bus, dev, func, 0x3D);
    uint8_t irq_line = pci_read_config_byte(bus, dev, func, 0x3C);
    
    g_e1000.bar0 = bar0;
    g_e1000.bar1 = bar1;
    g_e1000.irq = irq_line;
    
    uint32_t cmd = pci_read_config_word(bus, dev, func, 0x04);
    cmd |= 0x0007;
    pci_write_config_word(bus, dev, func, 0x04, cmd);
    
    if (bar0 & 0x01) {
        g_e1000.regs = (volatile uint32_t*)(uintptr_t)(bar0 & ~0x03);
    } else {
        g_e1000.regs = (volatile uint32_t*)(uintptr_t)(bar0 & ~0x0F);
    }
    
    spin_init(&g_e1000.lock);
    
    uint32_t ctrl = e1000_read_reg(&g_e1000, E1000_REG_CTRL);
    ctrl |= E1000_CTRL_RST;
    e1000_write_reg(&g_e1000, E1000_REG_CTRL, ctrl);
    
    for (int i = 0; i < 1000; i++) {
        ctrl = e1000_read_reg(&g_e1000, E1000_REG_CTRL);
        if (!(ctrl & E1000_CTRL_RST)) break;
    }
    
    e1000_read_mac_address(&g_e1000);
    
    e1000_tx_init(&g_e1000);
    e1000_rx_init(&g_e1000);
    
    e1000_hw_start(&g_e1000);
    
    e1000_check_link_status(&g_e1000);
    
    e1000_enable_interrupts(&g_e1000);
    
    net_register_tx(e1000_tx_from_net);
    
    g_e1000_initialized = 1;
    
    return 0;
}

int e1000_get_mac(uint8_t* mac_out)
{
    if (!g_e1000_initialized) return -1;
    memcpy(mac_out, g_e1000.mac_addr, 6);
    return 0;
}

int e1000_is_link_up(void)
{
    if (!g_e1000_initialized) return 0;
    return g_e1000.link_up;
}