#ifndef ARCH_X86_64_UART_H
#define ARCH_X86_64_UART_H

#include <arch/types.h>

#define UART_COM1_BASE 0x3F8
#define UART_COM2_BASE 0x2F8
#define UART_COM3_BASE 0x3E8
#define UART_COM4_BASE 0x2E8

#define UART_COM1_IRQ 4
#define UART_COM2_IRQ 3
#define UART_COM3_IRQ 4
#define UART_COM4_IRQ 3

#define UART_REG_DATA       0
#define UART_REG_IER        1
#define UART_REG_IIR        2
#define UART_REG_FCR        2
#define UART_REG_LCR        3
#define UART_REG_MCR        4
#define UART_REG_LSR        5
#define UART_REG_MSR        6
#define UART_REG_SCR        7
#define UART_REG_DLL        0
#define UART_REG_DLH        1

#define UART_IER_RDAI       0x01
#define UART_IER_THRI       0x02
#define UART_IER_RLSI       0x04
#define UART_IER_MSI        0x08

#define UART_IIR_NO_INT     0x01
#define UART_IIR_ID_MASK    0x0E
#define UART_IIR_MSI        0x00
#define UART_IIR_THRI       0x02
#define UART_IIR_RDAI       0x04
#define UART_IIR_RLSI       0x06
#define UART_IIR_CTOI       0x0C

#define UART_FCR_ENABLE     0x01
#define UART_FCR_RX_CLR     0x02
#define UART_FCR_TX_CLR     0x04
#define UART_FCR_DMA        0x08
#define UART_FCR_1_BYTE     0x00
#define UART_FCR_4_BYTE     0x40
#define UART_FCR_8_BYTE     0x80
#define UART_FCR_14_BYTE    0xC0

#define UART_LCR_5_BITS     0x00
#define UART_LCR_6_BITS     0x01
#define UART_LCR_7_BITS     0x02
#define UART_LCR_8_BITS     0x03
#define UART_LCR_1_STOP     0x00
#define UART_LCR_2_STOP     0x04
#define UART_LCR_NO_PARITY  0x00
#define UART_LCR_ODD_PARITY 0x08
#define UART_LCR_EVEN_PARITY 0x18
#define UART_LCR_BREAK      0x40
#define UART_LCR_DLAB       0x80

#define UART_LSR_DR         0x01
#define UART_LSR_OE         0x02
#define UART_LSR_PE         0x04
#define UART_LSR_FE         0x08
#define UART_LSR_BI         0x10
#define UART_LSR_THRE       0x20
#define UART_LSR_TEMT       0x40
#define UART_LSR_RXFE       0x80

#define UART_MCR_DTR        0x01
#define UART_MCR_RTS        0x02
#define UART_MCR_OUT1       0x04
#define UART_MCR_OUT2       0x08
#define UART_MCR_LOOP       0x10
#define UART_MCR_AFC        (UART_MCR_RTS | UART_MCR_OUT2)

#define UART_MSR_DCTS       0x01
#define UART_MSR_DDSR       0x02
#define UART_MSR_TERI       0x04
#define UART_MSR_DDCD       0x08
#define UART_MSR_CTS        0x10
#define UART_MSR_DSR        0x20
#define UART_MSR_RI         0x40
#define UART_MSR_DCD        0x80

#define UART_BAUD_BASE      115200

#define UART_FIFO_DEPTH     16

#define UART_DETECT_TIMEOUT 500000

#define UART_BUFFER_SIZE    4096

typedef enum {
    UART_COM1 = 0,
    UART_COM2 = 1,
    UART_COM3 = 2,
    UART_COM4 = 3,
    UART_PORT_MAX = 4
} uart_port_enum_t;

typedef struct {
    uint8_t data[UART_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} uart_buffer_t;

typedef struct {
    uint16_t base;
    uint32_t baud;
    uint8_t irq;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t fcr_trigger;
    int initialized;
    int irq_enabled;
    int hw_flow_control;
    uart_buffer_t rx_buffer;
    uart_buffer_t tx_buffer;
} uart_port_t;

void uart_init(void);
int uart_init_port(uart_port_enum_t port, uint32_t baud);
int uart_detect(uint16_t base);
void uart_putc(uart_port_enum_t port, char c);
char uart_getc(uart_port_enum_t port);
int uart_getc_timeout(uart_port_enum_t port, uint32_t timeout_us);
void uart_puts(uart_port_enum_t port, const char* s);
void uart_irq_handler(uart_port_enum_t port);
int uart_irq_register(uart_port_enum_t port);
int uart_set_baud(uart_port_enum_t port, uint32_t baud);
void uart_set_flow_control(uart_port_enum_t port, int enable);
int uart_rx_available(uart_port_enum_t port);
void uart_flush(uart_port_enum_t port);
void uart_enable_interrupts(uart_port_enum_t port);
void uart_disable_interrupts(uart_port_enum_t port);
void uart_init_default(void);
void uart_putc_default(char c);
void uart_puts_default(const char* str);
char uart_getc_default(void);
int uart_poll_default(char* c);
void uart_irq_handler_default(void);

#endif
