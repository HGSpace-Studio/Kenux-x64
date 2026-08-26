#include <arch/pcspk.h>
#include <arch/io.h>
#include <timer.h>

#define PIT_CHANNEL2     0x42
#define PIT_COMMAND      0x43
#define PC_SPEAKER_PORT  0x61
#define PIT_BASE_HZ      1193180U

static uint8_t pcspk_ready = 0;

void pcspk_init(void)
{
    pcspk_stop();
    pcspk_ready = 1;
}

void pcspk_stop(void)
{
    uint8_t v = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, (uint8_t)(v & 0xFC));
}

void pcspk_tone(uint32_t frequency_hz)
{
    if (!pcspk_ready) pcspk_init();
    if (frequency_hz == 0) {
        pcspk_stop();
        return;
    }
    if (frequency_hz < 37) frequency_hz = 37;
    if (frequency_hz > 20000) frequency_hz = 20000;

    uint32_t divisor = PIT_BASE_HZ / frequency_hz;
    if (divisor == 0) divisor = 1;

    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t v = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, (uint8_t)(v | 0x03));
}

void pcspk_play_tone(uint32_t frequency_hz, uint32_t duration_ms)
{
    if (duration_ms == 0) return;
    if (frequency_hz == 0) {
        pcspk_stop();
    } else {
        pcspk_tone(frequency_hz);
    }
    msleep(duration_ms);
    pcspk_stop();
}
