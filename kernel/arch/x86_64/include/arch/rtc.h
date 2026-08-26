#ifndef ARCH_X86_64_RTC_H
#define ARCH_X86_64_RTC_H

#include <arch/types.h>

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t century;
} rtc_time_t;

void rtc_init(void);
void rtc_get_time(rtc_time_t* time);
void rtc_set_time(rtc_time_t* time);
uint8_t rtc_get_register(uint8_t reg);
void rtc_set_register(uint8_t reg, uint8_t value);

#endif
