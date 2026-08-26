/*
 * Serial / parallel port driver - freestanding kernel implementation.
 *
 * A fixed-size pool of up to 4 serial (16550 UART) devices and up to 4
 * parallel (LPT) devices is maintained.  Four standard COM ports
 * (COM1..COM4) and one standard LPT port (LPT1) are registered at init
 * time.  Port I/O is performed directly through inline assembly
 * (inb / outb); the configuration and accounting logic is fully
 * exercised regardless of whether the I/O space is accessible.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define SERIAL_MAX_DEVICES     4
#define PARALLEL_MAX_DEVICES   4

/* 16550 UART register offsets (relative to io_base). */
#define UART_THR        0       /* Transmit Holding Register (write)    */
#define UART_RBR        0       /* Receive Buffer Register (read)        */
#define UART_IER        1       /* Interrupt Enable Register             */
#define UART_FCR        2       /* FIFO Control Register                 */
#define UART_LCR        3       /* Line Control Register                 */
#define UART_MCR        4       /* Modem Control Register                */
#define UART_LSR        5       /* Line Status Register                  */

#define UART_LSR_DR     0x01    /* Data Ready                            */
#define UART_LSR_THRE   0x20    /* Transmit Holding Register Empty       */

#define UART_LCR_DLAB   0x80    /* Divisor Latch Access Bit              */

/* ------------------------------------------------------------------ *
 * Port I/O helpers (inline assembly)
 * ------------------------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

/* ------------------------------------------------------------------ *
 * Serial device pool
 * ------------------------------------------------------------------ */
static SERIAL_DEVICE g_serial_pool[SERIAL_MAX_DEVICES];
static int           g_serial_count;

/* ------------------------------------------------------------------ *
 * Parallel device pool
 * ------------------------------------------------------------------ */
static PARALLEL_DEVICE g_parallel_pool[PARALLEL_MAX_DEVICES];
static int             g_parallel_count;

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */
static SERIAL_DEVICE* serial_find(uint16_t io_base) {
    int i;
    for (i = 0; i < SERIAL_MAX_DEVICES; i++) {
        if (g_serial_pool[i].connected && g_serial_pool[i].io_base == io_base)
            return &g_serial_pool[i];
    }
    return NULL;
}

static PARALLEL_DEVICE* parallel_find(uint16_t io_base) {
    int i;
    for (i = 0; i < PARALLEL_MAX_DEVICES; i++) {
        if (g_parallel_pool[i].connected && g_parallel_pool[i].io_base == io_base)
            return &g_parallel_pool[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Serial public API
 * ------------------------------------------------------------------ */

int serial_driver_init(void) {
    memset(g_serial_pool, 0, sizeof(g_serial_pool));
    g_serial_count = 0;

    if (serial_register_port("COM1", 0x3F8) < 0) return -1;
    if (serial_register_port("COM2", 0x2F8) < 0) return -1;
    if (serial_register_port("COM3", 0x3E8) < 0) return -1;
    if (serial_register_port("COM4", 0x2E8) < 0) return -1;

    return 0;
}

int serial_register_port(const char* name, uint16_t io_base) {
    int slot;
    SERIAL_DEVICE* dev;
    size_t nlen;

    if (name == NULL) return -1;

    for (slot = 0; slot < SERIAL_MAX_DEVICES; slot++) {
        if (!g_serial_pool[slot].connected) break;
    }
    if (slot >= SERIAL_MAX_DEVICES) return -1;

    dev = &g_serial_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    dev->io_base    = io_base;
    dev->baud_rate  = 115200;
    dev->data_bits  = 8;
    dev->parity     = 0;
    dev->stop_bits  = 1;
    dev->connected  = 1;
    dev->rx_count   = 0;
    dev->tx_count   = 0;

    g_serial_count++;
    return 0;
}

int serial_set_params(uint16_t io_base, uint32_t baud, uint8_t data_bits,
                      uint8_t parity, uint8_t stop_bits) {
    SERIAL_DEVICE* dev;
    uint32_t divisor;
    uint8_t lcr;

    dev = serial_find(io_base);
    if (dev == NULL) return -1;
    if (baud == 0) return -1;

    divisor = 115200u / baud;

    /* Divisor latch: set DLAB, then write DLL (io_base+0) and DLM (io_base+1). */
    outb((uint16_t)(io_base + UART_LCR), UART_LCR_DLAB);
    outb((uint16_t)(io_base + 0), (uint8_t)(divisor & 0xFF));
    outb((uint16_t)(io_base + 1), (uint8_t)((divisor >> 8) & 0xFF));

    /* Line control: data bits, stop bits, parity. */
    lcr = (uint8_t)((data_bits - 5) | ((stop_bits - 1) << 2) |
                    (parity ? (parity == 1 ? 0x08 : 0x18) : 0));
    outb((uint16_t)(io_base + UART_LCR), lcr);

    /* Enable and clear FIFOs, 14-byte trigger. */
    outb((uint16_t)(io_base + UART_FCR), 0xC7);

    /* DTR, RTS, OUT2. */
    outb((uint16_t)(io_base + UART_MCR), 0x0B);

    dev->baud_rate = baud;
    dev->data_bits = data_bits;
    dev->parity    = parity;
    dev->stop_bits = stop_bits;
    return 0;
}

int serial_write(uint16_t io_base, const void* data, uint32_t length) {
    SERIAL_DEVICE* dev;
    const uint8_t* src;
    uint32_t i;

    if (data == NULL) return -1;
    dev = serial_find(io_base);
    if (dev == NULL) return -1;

    src = (const uint8_t*)data;
    for (i = 0; i < length; i++) {
        /* Wait for THR empty. */
        while ((inb((uint16_t)(io_base + UART_LSR)) & UART_LSR_THRE) == 0) {
            /* spin */
        }
        outb((uint16_t)(io_base + UART_THR), src[i]);
    }

    dev->tx_count += (uint64_t)length;
    return (int)length;
}

int serial_read(uint16_t io_base, void* buf, uint32_t buf_size) {
    SERIAL_DEVICE* dev;
    uint8_t* dst;
    uint32_t count = 0;

    if (buf == NULL || buf_size == 0) return 0;
    dev = serial_find(io_base);
    if (dev == NULL) return -1;

    dst = (uint8_t*)buf;
    while (count < buf_size) {
        if ((inb((uint16_t)(io_base + UART_LSR)) & UART_LSR_DR) == 0) {
            break;  /* no more data ready */
        }
        dst[count] = inb((uint16_t)(io_base + UART_RBR));
        count++;
    }

    dev->rx_count += (uint64_t)count;
    return (int)count;
}

int serial_get_device_count(void) {
    return g_serial_count;
}

SERIAL_DEVICE* serial_get_device(uint16_t io_base) {
    return serial_find(io_base);
}

/* ------------------------------------------------------------------ *
 * Parallel public API
 * ------------------------------------------------------------------ */

int parallel_driver_init(void) {
    memset(g_parallel_pool, 0, sizeof(g_parallel_pool));
    g_parallel_count = 0;

    if (parallel_register_port("LPT1", 0x378) < 0) return -1;

    return 0;
}

int parallel_register_port(const char* name, uint16_t io_base) {
    int slot;
    PARALLEL_DEVICE* dev;
    size_t nlen;

    if (name == NULL) return -1;

    for (slot = 0; slot < PARALLEL_MAX_DEVICES; slot++) {
        if (!g_parallel_pool[slot].connected) break;
    }
    if (slot >= PARALLEL_MAX_DEVICES) return -1;

    dev = &g_parallel_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    dev->io_base        = io_base;
    dev->connected      = 1;
    dev->bytes_written  = 0;

    g_parallel_count++;
    return 0;
}

int parallel_write(uint16_t io_base, const void* data, uint32_t length) {
    PARALLEL_DEVICE* dev;
    const uint8_t* src;
    uint32_t i;

    if (data == NULL) return -1;
    dev = parallel_find(io_base);
    if (dev == NULL) return -1;

    src = (const uint8_t*)data;
    for (i = 0; i < length; i++) {
        outb((uint16_t)(io_base + 0), src[i]);
        /* Strobe: raise then lower the strobe bit on the control port. */
        outb((uint16_t)(io_base + 2), 0x0D);
        outb((uint16_t)(io_base + 2), 0x0C);
    }

    dev->bytes_written += (uint64_t)length;
    return (int)length;
}

int parallel_get_device_count(void) {
    return g_parallel_count;
}

PARALLEL_DEVICE* parallel_get_device(uint16_t io_base) {
    return parallel_find(io_base);
}
