#include <arch/uart.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/interrupt.h>
#include <string.h>

uart_port_t uart_ports[UART_PORT_MAX];
spinlock_t uart_locks[UART_PORT_MAX];
uart_port_enum_t uart_default_port = UART_COM1;

static const uint16_t uart_port_bases[UART_PORT_MAX] = {
    UART_COM1_BASE,
    UART_COM2_BASE,
    UART_COM3_BASE,
    UART_COM4_BASE
};

static const uint8_t uart_port_irqs[UART_PORT_MAX] = {
    UART_COM1_IRQ,
    UART_COM2_IRQ,
    UART_COM3_IRQ,
    UART_COM4_IRQ
};

static int uart_buffer_put(uart_buffer_t* buf, uint8_t data)
{
    uint32_t next = (buf->head + 1) % UART_BUFFER_SIZE;
    if (next == buf->tail)
        return -1;
    buf->data[buf->head] = data;
    buf->head = next;
    buf->count++;
    return 0;
}

static int uart_buffer_get(uart_buffer_t* buf, uint8_t* data)
{
    if (buf->head == buf->tail)
        return -1;
    *data = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % UART_BUFFER_SIZE;
    buf->count--;
    return 0;
}

int uart_detect(uint16_t base)
{
    uint8_t scratch;
    int i;

    outb(base + UART_REG_SCR, 0xAA);
    for (i = 0; i < UART_DETECT_TIMEOUT; i++) {
        if (inb(base + UART_REG_SCR) == 0xAA)
            break;
    }
    if (i >= UART_DETECT_TIMEOUT)
        return -1;

    outb(base + UART_REG_SCR, 0x55);
    for (i = 0; i < UART_DETECT_TIMEOUT; i++) {
        if (inb(base + UART_REG_SCR) == 0x55)
            break;
    }
    if (i >= UART_DETECT_TIMEOUT)
        return -1;

    scratch = inb(base + UART_REG_LCR);
    outb(base + UART_REG_LCR, scratch | UART_LCR_DLAB);
    outb(base + UART_REG_DLL, 0x00);
    outb(base + UART_REG_DLH, 0x00);
    outb(base + UART_REG_LCR, scratch);

    return 0;
}

int uart_set_baud(uart_port_enum_t port, uint32_t baud)
{
    uart_port_t* p;
    uint16_t divisor;
    uint8_t lcr;

    if (port >= UART_PORT_MAX)
        return -1;

    p = &uart_ports[port];
    if (!p->initialized)
        return -1;

    if (baud == 0 || baud > UART_BAUD_BASE)
        return -1;

    divisor = (uint16_t)(UART_BAUD_BASE / baud);
    if (divisor == 0)
        return -1;

    spin_lock(&uart_locks[port]);

    lcr = inb(p->base + UART_REG_LCR);
    outb(p->base + UART_REG_LCR, lcr | UART_LCR_DLAB);
    outb(p->base + UART_REG_DLL, divisor & 0xFF);
    outb(p->base + UART_REG_DLH, (divisor >> 8) & 0xFF);
    outb(p->base + UART_REG_LCR, lcr);

    p->baud = baud;

    spin_unlock(&uart_locks[port]);

    return 0;
}

void uart_set_flow_control(uart_port_enum_t port, int enable)
{
    uart_port_t* p;
    uint8_t mcr;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized)
        return;

    spin_lock(&uart_locks[port]);

    mcr = inb(p->base + UART_REG_MCR);
    if (enable) {
        mcr |= UART_MCR_AFC;
        p->hw_flow_control = 1;
    } else {
        mcr &= ~(UART_MCR_RTS | UART_MCR_OUT2);
        p->hw_flow_control = 0;
    }
    outb(p->base + UART_REG_MCR, mcr);

    spin_unlock(&uart_locks[port]);
}

int uart_init_port(uart_port_enum_t port, uint32_t baud)
{
    uart_port_t* p;
    uint16_t divisor;

    if (port >= UART_PORT_MAX)
        return -1;

    p = &uart_ports[port];

    if (uart_detect(uart_port_bases[port]) != 0)
        return -1;

    spin_init(&uart_locks[port]);
    spin_lock(&uart_locks[port]);

    memset(p, 0, sizeof(uart_port_t));
    p->base = uart_port_bases[port];
    p->irq = uart_port_irqs[port];
    p->baud = baud;
    p->data_bits = 8;
    p->stop_bits = 1;
    p->parity = 0;
    p->fcr_trigger = UART_FCR_14_BYTE;
    p->initialized = 0;
    p->irq_enabled = 0;
    p->hw_flow_control = 0;

    outb(p->base + UART_REG_IER, 0x00);

    outb(p->base + UART_REG_LCR, UART_LCR_DLAB);
    divisor = (uint16_t)(UART_BAUD_BASE / baud);
    outb(p->base + UART_REG_DLL, divisor & 0xFF);
    outb(p->base + UART_REG_DLH, (divisor >> 8) & 0xFF);

    outb(p->base + UART_REG_LCR,
         UART_LCR_8_BITS | UART_LCR_1_STOP | UART_LCR_NO_PARITY);

    outb(p->base + UART_REG_FCR,
         UART_FCR_ENABLE | UART_FCR_RX_CLR | UART_FCR_TX_CLR | p->fcr_trigger);

    outb(p->base + UART_REG_MCR, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);

    p->initialized = 1;

    spin_unlock(&uart_locks[port]);

    return 0;
}

void uart_init(void)
{
    int i;

    for (i = 0; i < UART_PORT_MAX; i++) {
        memset(&uart_ports[i], 0, sizeof(uart_port_t));
        spin_init(&uart_locks[i]);
    }

    for (i = 0; i < UART_PORT_MAX; i++) {
        if (uart_init_port((uart_port_enum_t)i, 115200) == 0) {
            uart_irq_register((uart_port_enum_t)i);
        }
    }
}

void uart_init_default(void)
{
    if (uart_init_port(uart_default_port, 115200) == 0) {
        uart_irq_register(uart_default_port);
    }
}

void uart_putc(uart_port_enum_t port, char c)
{
    uart_port_t* p;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized)
        return;

    spin_lock(&uart_locks[port]);

    while (!(inb(p->base + UART_REG_LSR) & UART_LSR_THRE))
        ;

    outb(p->base + UART_REG_DATA, (uint8_t)c);

    spin_unlock(&uart_locks[port]);
}

char uart_getc(uart_port_enum_t port)
{
    uart_port_t* p;
    uint8_t c;

    if (port >= UART_PORT_MAX)
        return 0;

    p = &uart_ports[port];
    if (!p->initialized)
        return 0;

    spin_lock(&uart_locks[port]);

    if (p->irq_enabled) {
        while (uart_buffer_get(&p->rx_buffer, &c) != 0) {
            spin_unlock(&uart_locks[port]);
            __asm__ volatile ("pause");
            spin_lock(&uart_locks[port]);
        }
    } else {
        while (!(inb(p->base + UART_REG_LSR) & UART_LSR_DR))
            ;
        c = inb(p->base + UART_REG_DATA);
    }

    spin_unlock(&uart_locks[port]);

    return (char)c;
}

int uart_getc_timeout(uart_port_enum_t port, uint32_t timeout_us)
{
    uart_port_t* p;
    uint8_t c;
    uint32_t i;

    if (port >= UART_PORT_MAX)
        return -1;

    p = &uart_ports[port];
    if (!p->initialized)
        return -1;

    spin_lock(&uart_locks[port]);

    if (p->irq_enabled) {
        for (i = 0; i < timeout_us; i++) {
            if (uart_buffer_get(&p->rx_buffer, &c) == 0) {
                spin_unlock(&uart_locks[port]);
                return (int)c;
            }
            spin_unlock(&uart_locks[port]);
            __asm__ volatile ("pause");
            spin_lock(&uart_locks[port]);
        }
    } else {
        for (i = 0; i < timeout_us; i++) {
            if (inb(p->base + UART_REG_LSR) & UART_LSR_DR) {
                c = inb(p->base + UART_REG_DATA);
                spin_unlock(&uart_locks[port]);
                return (int)c;
            }
            __asm__ volatile ("pause");
        }
    }

    spin_unlock(&uart_locks[port]);

    return -1;
}

void uart_puts(uart_port_enum_t port, const char* s)
{
    if (s == NULL)
        return;

    while (*s) {
        if (*s == '\n')
            uart_putc(port, '\r');
        uart_putc(port, *s);
        s++;
    }
}

int uart_rx_available(uart_port_enum_t port)
{
    uart_port_t* p;
    int count;

    if (port >= UART_PORT_MAX)
        return 0;

    p = &uart_ports[port];
    if (!p->initialized)
        return 0;

    spin_lock(&uart_locks[port]);

    if (p->irq_enabled) {
        count = (int)p->rx_buffer.count;
    } else {
        count = (inb(p->base + UART_REG_LSR) & UART_LSR_DR) ? 1 : 0;
    }

    spin_unlock(&uart_locks[port]);

    return count;
}

void uart_flush(uart_port_enum_t port)
{
    uart_port_t* p;
    int i;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized)
        return;

    spin_lock(&uart_locks[port]);

    for (i = 0; i < UART_FIFO_DEPTH; i++) {
        if (!(inb(p->base + UART_REG_LSR) & UART_LSR_DR))
            break;
        inb(p->base + UART_REG_DATA);
    }

    p->rx_buffer.head = 0;
    p->rx_buffer.tail = 0;
    p->rx_buffer.count = 0;

    p->tx_buffer.head = 0;
    p->tx_buffer.tail = 0;
    p->tx_buffer.count = 0;

    spin_unlock(&uart_locks[port]);
}

void uart_irq_handler(uart_port_enum_t port)
{
    uart_port_t* p;
    uint8_t iir;
    uint8_t lsr;
    uint8_t data;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized || !p->irq_enabled)
        return;

    spin_lock(&uart_locks[port]);

    for (;;) {
        iir = inb(p->base + UART_REG_IIR);
        if (iir & UART_IIR_NO_INT)
            break;

        switch (iir & UART_IIR_ID_MASK) {
            case UART_IIR_RLSI:
                lsr = inb(p->base + UART_REG_LSR);
                (void)lsr;
                break;

            case UART_IIR_RDAI:
            case UART_IIR_CTOI:
                while (inb(p->base + UART_REG_LSR) & UART_LSR_DR) {
                    data = inb(p->base + UART_REG_DATA);
                    uart_buffer_put(&p->rx_buffer, data);
                }
                break;

            case UART_IIR_THRI:
                while (uart_buffer_get(&p->tx_buffer, &data) == 0) {
                    if (!(inb(p->base + UART_REG_LSR) & UART_LSR_THRE)) {
                        uart_buffer_put(&p->tx_buffer, data);
                        break;
                    }
                    outb(p->base + UART_REG_DATA, data);
                }
                if (p->tx_buffer.count == 0) {
                    uint8_t ier = inb(p->base + UART_REG_IER);
                    ier &= ~UART_IER_THRI;
                    outb(p->base + UART_REG_IER, ier);
                }
                break;

            case UART_IIR_MSI:
                lsr = inb(p->base + UART_REG_MSR);
                (void)lsr;
                break;

            default:
                break;
        }
    }

    spin_unlock(&uart_locks[port]);
}

static void uart_com1_irq_stub(void)
{
    uart_irq_handler(UART_COM1);
}

static void uart_com2_irq_stub(void)
{
    uart_irq_handler(UART_COM2);
}

static void uart_com3_irq_stub(void)
{
    uart_irq_handler(UART_COM3);
}

static void uart_com4_irq_stub(void)
{
    uart_irq_handler(UART_COM4);
}

int uart_irq_register(uart_port_enum_t port)
{
    uart_port_t* p;
    void* stub;

    if (port >= UART_PORT_MAX)
        return -1;

    p = &uart_ports[port];
    if (!p->initialized)
        return -1;

    switch (port) {
        case UART_COM1:
            stub = (void*)uart_com1_irq_stub;
            break;
        case UART_COM2:
            stub = (void*)uart_com2_irq_stub;
            break;
        case UART_COM3:
            stub = (void*)uart_com3_irq_stub;
            break;
        case UART_COM4:
            stub = (void*)uart_com4_irq_stub;
            break;
        default:
            return -1;
    }

    interrupt_register(p->irq, stub);

    return 0;
}

void uart_enable_interrupts(uart_port_enum_t port)
{
    uart_port_t* p;
    uint8_t ier;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized)
        return;

    spin_lock(&uart_locks[port]);

    ier = UART_IER_RDAI | UART_IER_RLSI | UART_IER_MSI;
    outb(p->base + UART_REG_IER, ier);

    p->irq_enabled = 1;

    spin_unlock(&uart_locks[port]);
}

void uart_disable_interrupts(uart_port_enum_t port)
{
    uart_port_t* p;

    if (port >= UART_PORT_MAX)
        return;

    p = &uart_ports[port];
    if (!p->initialized)
        return;

    spin_lock(&uart_locks[port]);

    outb(p->base + UART_REG_IER, 0x00);

    p->irq_enabled = 0;

    spin_unlock(&uart_locks[port]);
}

void uart_putc_default(char c)
{
    uart_putc(uart_default_port, c);
}

void uart_puts_default(const char* str)
{
    uart_puts(uart_default_port, str);
}

char uart_getc_default(void)
{
    return uart_getc(uart_default_port);
}

int uart_poll_default(char* c)
{
    uart_port_t* p;
    int ret = -1;

    if (c == NULL)
        return -1;

    p = &uart_ports[uart_default_port];
    if (!p->initialized)
        return -1;

    spin_lock(&uart_locks[uart_default_port]);

    if (p->irq_enabled) {
        uint8_t data;
        if (uart_buffer_get(&p->rx_buffer, &data) == 0) {
            *c = (char)data;
            ret = 0;
        }
    } else {
        if (inb(p->base + UART_REG_LSR) & UART_LSR_DR) {
            *c = (char)inb(p->base + UART_REG_DATA);
            ret = 0;
        }
    }

    spin_unlock(&uart_locks[uart_default_port]);

    return ret;
}

void uart_irq_handler_default(void)
{
    uart_irq_handler(uart_default_port);
}
