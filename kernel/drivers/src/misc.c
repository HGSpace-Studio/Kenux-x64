#include "misc.h"
#include <arch/io.h>
#include <string.h>

static uint64_t _tsc_freq = 0;
static uint64_t _tsc_ns_per_tick = 0;

void hpet_init(hpet_t* hpet)
{
    if (!hpet) return;
    memset(hpet, 0, sizeof(hpet_t));
    spin_init(&hpet->lock);

    hpet->mmio = (volatile uint32_t*)HPET_BASE;
    uint64_t cap = ((uint64_t)hpet->mmio[HPET_GEN_CAP / 4 + 1] << 32) | hpet->mmio[HPET_GEN_CAP / 4];

    hpet->period = (cap >> HPET_CAP_PERIOD_SHIFT) & 0xFFFFFFFFF;
    hpet->num_timers = (uint32_t)((cap >> HPET_CAP_NUM_TIMERS_SHIFT) & 0x1F) + 1;
    hpet->is_64bit = (cap & HPET_CAP_COUNT_SIZE) ? 1 : 0;
    hpet->has_legacy = (cap & HPET_CAP_LEGACY) ? 1 : 0;

    if (hpet->period > 0) hpet->freq = 1000000000000000ULL / hpet->period;
}

uint64_t hpet_read_counter(hpet_t* hpet)
{
    if (!hpet || !hpet->mmio) return 0;
    if (hpet->is_64bit) {
        return ((uint64_t)hpet->mmio[HPET_MAIN_COUNTER / 4 + 1] << 32) |
               hpet->mmio[HPET_MAIN_COUNTER / 4];
    }
    return hpet->mmio[HPET_MAIN_COUNTER / 4];
}

uint64_t hpet_get_ns(hpet_t* hpet)
{
    if (!hpet || hpet->period == 0) return 0;
    uint64_t counter = hpet_read_counter(hpet);
    return (counter * hpet->period) / 1000000;
}

void hpet_set_timer(hpet_t* hpet, int timer, uint64_t ns, int periodic)
{
    if (!hpet || !hpet->mmio || timer < 0) return;
    spinlock_acquire(&hpet->lock);

    uint32_t offset = HPET_T0_CONFIG + timer * 0x20;
    uint32_t config = hpet->mmio[offset / 4];
    config |= 0x04;
    if (periodic) config |= 0x02;
    else config &= ~0x02;
    hpet->mmio[offset / 4] = config;

    uint64_t comparator = (ns * 1000000) / hpet->period;
    hpet->mmio[(offset + 8) / 4] = (uint32_t)comparator;
    hpet->mmio[(offset + 8) / 4 + 1] = (uint32_t)(comparator >> 32);

    spinlock_release(&hpet->lock);
}

void hpet_enable(hpet_t* hpet)
{
    if (!hpet || !hpet->mmio) return;
    hpet->mmio[HPET_GEN_CONFIG / 4] |= HPET_CONF_ENABLE | HPET_CONF_LEGACY;
}

void hpet_disable(hpet_t* hpet)
{
    if (!hpet || !hpet->mmio) return;
    hpet->mmio[HPET_GEN_CONFIG / 4] &= ~HPET_CONF_ENABLE;
}

static uint8_t bcd_to_bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }

void rtc_init(rtc_t* rtc)
{
    if (!rtc) return;
    memset(rtc, 0, sizeof(rtc_t));
    spin_init(&rtc->lock);
}

void rtc_read_time(rtc_t* rtc, rtc_time_t* time)
{
    if (!rtc || !time) return;
    spinlock_acquire(&rtc->lock);

    outb(RTC_PORT_INDEX, RTC_STATUS_B);
    uint8_t reg_b = inb(RTC_PORT_DATA);
    int is_bcd = !(reg_b & RTC_B_DM);
    time->is_24hr = (reg_b & RTC_B_24HR) ? 1 : 0;
    time->is_bcd = is_bcd;

    outb(RTC_PORT_INDEX, RTC_STATUS_A);
    while (inb(RTC_PORT_DATA) & 0x80);

    outb(RTC_PORT_INDEX, RTC_SECONDS);
    time->seconds = inb(RTC_PORT_DATA);
    outb(RTC_PORT_INDEX, RTC_MINUTES);
    time->minutes = inb(RTC_PORT_DATA);
    outb(RTC_PORT_INDEX, RTC_HOURS);
    time->hours = inb(RTC_PORT_DATA);
    outb(RTC_PORT_INDEX, RTC_DAY);
    time->day = inb(RTC_PORT_DATA);
    outb(RTC_PORT_INDEX, RTC_MONTH);
    time->month = inb(RTC_PORT_DATA);
    outb(RTC_PORT_INDEX, RTC_YEAR);
    time->year = inb(RTC_PORT_DATA);

    if (is_bcd) {
        time->seconds = bcd_to_bin(time->seconds);
        time->minutes = bcd_to_bin(time->minutes);
        time->hours = bcd_to_bin(time->hours);
        time->day = bcd_to_bin(time->day);
        time->month = bcd_to_bin(time->month);
        time->year = bcd_to_bin(time->year);
    }

    if (!time->is_24hr && (time->hours & 0x80)) {
        time->hours = ((time->hours & 0x7F) + 12) % 24;
    }

    time->year += 2000;

    spinlock_release(&rtc->lock);
}

uint64_t rtc_read_epoch(rtc_t* rtc)
{
    rtc_time_t time;
    rtc_read_time(rtc, &time);

    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t y = time.year;
    uint32_t leaps = (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    uint64_t days = (uint64_t)(y - 1) * 365 + leaps;
    days += days_before_month[time.month - 1];
    if (time.month > 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) days++;
    days += time.day - 1;

    return days * 86400 + time.hours * 3600 + time.minutes * 60 + time.seconds;
}

void serial_init(serial_t* s, uint16_t port, int baud)
{
    if (!s) return;
    memset(s, 0, sizeof(serial_t));
    spin_init(&s->lock);
    s->port = port;
    s->baud_rate = baud;
    s->data_bits = 8;
    s->stop_bits = 1;
    s->parity = 0;

    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    uint16_t divisor = 115200 / baud;
    outb(port + 0, (uint8_t)(divisor & 0xFF));
    outb(port + 1, (uint8_t)(divisor >> 8));
    outb(port + 3, 0x03);
    outb(port + 2, 0xC7);
    outb(port + 4, 0x0B);
    s->initialized = 1;
}

int serial_read(serial_t* s, void* buf, int count)
{
    if (!s || !buf || !s->initialized) return -1;
    uint8_t* dst = (uint8_t*)buf;
    int read = 0;
    spinlock_acquire(&s->lock);
    for (int i = 0; i < count; i++) {
        if (!(inb(s->port + 5) & 0x01)) break;
        dst[i] = inb(s->port);
        read++;
    }
    spinlock_release(&s->lock);
    return read;
}

int serial_write(serial_t* s, const void* buf, int count)
{
    if (!s || !buf || !s->initialized) return -1;
    const uint8_t* src = (const uint8_t*)buf;
    spinlock_acquire(&s->lock);
    for (int i = 0; i < count; i++) {
        while (!(inb(s->port + 5) & 0x20));
        outb(s->port, src[i]);
    }
    spinlock_release(&s->lock);
    return count;
}

int serial_available(serial_t* s)
{
    if (!s || !s->initialized) return 0;
    return (inb(s->port + 5) & 0x01) ? 1 : 0;
}

void parport_init(parport_t* p, uint16_t base)
{
    if (!p) return;
    memset(p, 0, sizeof(parport_t));
    spin_init(&p->lock);
    p->base = base;
}

void parport_write_data(parport_t* p, uint8_t data)
{
    if (!p) return;
    spinlock_acquire(&p->lock);
    outb(p->base + PARPORT_DATA, data);
    spinlock_release(&p->lock);
}

uint8_t parport_read_status(parport_t* p)
{
    if (!p) return 0;
    return inb(p->base + PARPORT_STATUS);
}

uint64_t tsc_read(void)
{
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

uint64_t tsc_get_ns(void)
{
    if (_tsc_freq == 0) return tsc_read() / 1;
    uint64_t tsc = tsc_read();
    return (tsc * 1000000000ULL) / _tsc_freq;
}

void tsc_calibrate(void)
{
    hpet_t hpet;
    hpet_init(&hpet);
    hpet_enable(&hpet);

    uint64_t start_hpet = hpet_read_counter(&hpet);
    uint64_t start_tsc = tsc_read();

    for (volatile int i = 0; i < 10000000; i++);

    uint64_t end_hpet = hpet_read_counter(&hpet);
    uint64_t end_tsc = tsc_read();

    uint64_t hpet_delta = end_hpet - start_hpet;
    uint64_t tsc_delta = end_tsc - start_tsc;

    if (hpet_delta > 0 && hpet.period > 0) {
        uint64_t hpet_ns = (hpet_delta * hpet.period) / 1000000;
        if (hpet_ns > 0) _tsc_freq = (tsc_delta * 1000000000ULL) / hpet_ns;
    }

    if (_tsc_freq == 0) _tsc_freq = 2000000000ULL;
    _tsc_ns_per_tick = (1000000000ULL + _tsc_freq / 2) / _tsc_freq;
}