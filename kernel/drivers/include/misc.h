#ifndef KERNEL_DRIVERS_MISC_H
#define KERNEL_DRIVERS_MISC_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define HPET_BASE          0xFED00000
#define HPET_GEN_CAP       0x000
#define HPET_GEN_CONFIG    0x010
#define HPET_GEN_INT_STAT  0x020
#define HPET_MAIN_COUNTER  0x0F0
#define HPET_T0_CONFIG     0x100
#define HPET_T0_COMPARATOR 0x108

#define HPET_CAP_PERIOD_SHIFT 12
#define HPET_CAP_COUNT_SIZE   (1 << 13)
#define HPET_CAP_NUM_TIMERS_SHIFT 8
#define HPET_CAP_LEGACY      (1 << 15)

#define HPET_CONF_ENABLE     (1 << 0)
#define HPET_CONF_LEGACY     (1 << 1)

typedef struct {
    volatile uint32_t* mmio;
    uint64_t           period;
    uint32_t           num_timers;
    int                is_64bit;
    int                has_legacy;
    uint64_t           freq;
    spinlock_t         lock;
} hpet_t;

#define RTC_PORT_INDEX  0x70
#define RTC_PORT_DATA   0x71

#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY_OF_WEEK 0x06
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B
#define RTC_STATUS_C    0x0C

#define RTC_B_24HR      0x02
#define RTC_B_DM        0x04
#define RTC_B_UIE       0x10

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t day_of_week;
    int     is_24hr;
    int     is_bcd;
} rtc_time_t;

typedef struct {
    spinlock_t lock;
} rtc_t;

#define SERIAL_COM1  0x3F8
#define SERIAL,COM2  0x2F8
#define SERIAL_COM3  0x3E8
#define SERIAL_COM4  0x2E8

typedef struct {
    uint16_t    port;
    int         baud_rate;
    int         data_bits;
    int         stop_bits;
    int         parity;
    int         initialized;
    spinlock_t  lock;
} serial_t;

#define PARPORT_BASE    0x378
#define PARPORT_DATA    0
#define PARPORT_STATUS  1
#define PARPORT_CONTROL 2

typedef struct {
    uint16_t    base;
    int         mode;
    int         has_epp;
    int         has_ecp;
    spinlock_t  lock;
} parport_t;

void     hpet_init(hpet_t* hpet);
uint64_t hpet_read_counter(hpet_t* hpet);
uint64_t hpet_get_ns(hpet_t* hpet);
void     hpet_set_timer(hpet_t* hpet, int timer, uint64_t ns, int periodic);
void     hpet_enable(hpet_t* hpet);
void     hpet_disable(hpet_t* hpet);

void     rtc_init(rtc_t* rtc);
void     rtc_read_time(rtc_t* rtc, rtc_time_t* time);
uint64_t rtc_read_epoch(rtc_t* rtc);

void     serial_init(serial_t* s, uint16_t port, int baud);
int      serial_read(serial_t* s, void* buf, int count);
int      serial_write(serial_t* s, const void* buf, int count);
int      serial_available(serial_t* s);

void     parport_init(parport_t* p, uint16_t base);
void     parport_write_data(parport_t* p, uint8_t data);
uint8_t  parport_read_status(parport_t* p);

uint64_t tsc_read(void);
uint64_t tsc_get_ns(void);
void     tsc_calibrate(void);

#endif