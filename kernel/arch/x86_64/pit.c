#include <arch/pit.h>
#include <arch/pit.h>
#include <arch/io.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

#define PIT_CMD_COUNTER0 0x00
#define PIT_CMD_COUNTER1 0x40
#define PIT_CMD_COUNTER2 0x80
#define PIT_CMD_READ_LOAD 0xC0
#define PIT_CMD_MODE 0x20
#define PIT_CMD_BINARY 0x00
#define PIT_CMD_BCD 0x01

#define PIT_RATE 1193182

static uint64_t pit_ticks = 0;

void pit_init(void)
{
    uint8_t mode = PIT_CMD_COUNTER0 | PIT_CMD_READ_LOAD | PIT_CMD_MODE | PIT_CMD_BINARY;
    outb(PIT_COMMAND, mode);

    uint32_t divisor = PIT_RATE / 1000;
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void pit_sleep(uint64_t ms)
{
    uint64_t start = pit_get_ticks();
    while (pit_get_ticks() - start < ms) {
    }
}

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}

void pit_set_rate(uint32_t hz)
{
    uint32_t divisor = PIT_RATE / hz;
    uint8_t mode = PIT_CMD_COUNTER0 | PIT_CMD_READ_LOAD | PIT_CMD_MODE | PIT_CMD_BINARY;
    outb(PIT_COMMAND, mode);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
