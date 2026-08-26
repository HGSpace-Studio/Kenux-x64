#include <arch/rtc.h>
#include <arch/rtc.h>
#include <arch/io.h>

#define RTC_INDEX 0x70
#define RTC_DATA 0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_CENTURY 0x32
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

static uint8_t rtc_last_seconds = 0;

void rtc_init(void)
{
    uint8_t status_b = rtc_get_register(RTC_STATUS_B);
    status_b |= 0x80;
    rtc_set_register(RTC_STATUS_B, status_b);
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

void rtc_get_time(rtc_time_t* time)
{
    while (rtc_get_register(RTC_STATUS_A) & 0x80) {
    }

    uint8_t status_b = rtc_get_register(RTC_STATUS_B);
    uint8_t format = (status_b & 0x04) ? 0 : 1;

    time->seconds = rtc_get_register(RTC_SECONDS);
    time->minutes = rtc_get_register(RTC_MINUTES);
    time->hours = rtc_get_register(RTC_HOURS);
    time->day = rtc_get_register(RTC_DAY);
    time->month = rtc_get_register(RTC_MONTH);
    time->year = rtc_get_register(RTC_YEAR);
    time->century = rtc_get_register(RTC_CENTURY);

    if (format) {
        time->seconds = bcd_to_binary(time->seconds);
        time->minutes = bcd_to_binary(time->minutes);
        time->hours = bcd_to_binary(time->hours);
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);
        time->century = bcd_to_binary(time->century);
    }

    if (!(status_b & 0x02)) {
        if (time->hours & 0x80) {
            time->hours = ((time->hours & 0x7F) + 12) % 24;
        }
    }

    time->year += time->century * 100;
}

void rtc_set_time(rtc_time_t* time)
{
    uint8_t status_b = rtc_get_register(RTC_STATUS_B);
    status_b |= 0x80;
    rtc_set_register(RTC_STATUS_B, status_b);

    uint8_t status_a = rtc_get_register(RTC_STATUS_A);
    status_a |= 0x80;
    rtc_set_register(RTC_STATUS_A, status_a);

    rtc_set_register(RTC_SECONDS, time->seconds);
    rtc_set_register(RTC_MINUTES, time->minutes);
    rtc_set_register(RTC_HOURS, time->hours);
    rtc_set_register(RTC_DAY, time->day);
    rtc_set_register(RTC_MONTH, time->month);
    rtc_set_register(RTC_YEAR, time->year);
    rtc_set_register(RTC_CENTURY, time->century);

    status_a &= 0x7F;
    rtc_set_register(RTC_STATUS_A, status_a);

    status_b &= 0x7F;
    rtc_set_register(RTC_STATUS_B, status_b);
}

uint8_t rtc_get_register(uint8_t reg)
{
    outb(RTC_INDEX, reg);
    return inb(RTC_DATA);
}

void rtc_set_register(uint8_t reg, uint8_t value)
{
    outb(RTC_INDEX, reg);
    outb(RTC_DATA, value);
}
