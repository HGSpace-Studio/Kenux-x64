#include "time.h"
#include <string.h>
#include <arch/io.h>
#include <kapi_device.h>

/* RTC CMOS 寄存器 */
#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x02
#define RTC_HOURS     0x04
#define RTC_DAY       0x07
#define RTC_MONTH     0x08
#define RTC_YEAR      0x09
#define RTC_CENTURY   0x32
#define RTC_STATUS_B  0x0B

#define RTC_PORT_ADDR 0x70
#define RTC_PORT_DATA 0x71

/* 时区偏移：UTC+8（秒） */
#define TIMEZONE_OFFSET (8 * 3600)

static int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/* 读取 RTC 寄存器（BCD 或二进制） */
static uint8_t rtc_read(uint8_t reg)
{
    outb(RTC_PORT_ADDR, reg);
    return inb(RTC_PORT_DATA);
}

/* BCD 转二进制 */
static uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/* 判断是否为闰年 */
static int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* 计算从 1970 年到指定年份的天数 */
static long long days_since_epoch(int year, int month, int day)
{
    long long days = 0;
    int y;
    for (y = 1970; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    for (int m = 0; m < month - 1; m++) {
        days += days_in_month[m];
        if (m == 1 && is_leap_year(year)) days++;
    }
    days += day - 1;
    return days;
}

/* 获取当前毫秒时间戳 */
clock_t clock(void)
{
    return (clock_t)kapi_get_time_ms();
}

/* 获取当前秒时间戳 */
time_t time(time_t* timer)
{
    time_t t = (time_t)(kapi_get_time_ms() / 1000);
    if (timer) {
        *timer = t;
    }
    return t;
}

/* 计算时间差 */
double difftime(time_t time1, time_t time0)
{
    return (double)(time1 - time0);
}

/* 从 RTC 读取时间并转换为 struct tm */
static void rtc_to_tm(struct tm* tm, int local)
{
    uint8_t status_b = rtc_read(RTC_STATUS_B);
    int bcd_mode = !(status_b & 0x04);
    int hour_12  = !(status_b & 0x02);

    uint8_t sec  = rtc_read(RTC_SECONDS);
    uint8_t min  = rtc_read(RTC_MINUTES);
    uint8_t hour = rtc_read(RTC_HOURS);
    uint8_t day  = rtc_read(RTC_DAY);
    uint8_t mon  = rtc_read(RTC_MONTH);
    uint8_t year = rtc_read(RTC_YEAR);
    uint8_t cent = rtc_read(RTC_CENTURY);

    if (bcd_mode) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
        cent = bcd_to_bin(cent);
    }

    if (hour_12 && (hour & 0x80)) {
        hour = ((hour & 0x7F) % 12) + 12;
    }

    int full_year = cent * 100 + year;

    tm->tm_sec  = sec;
    tm->tm_min  = min;
    tm->tm_hour = hour;
    tm->tm_mday = day;
    tm->tm_mon  = mon - 1;     /* 0-11 */
    tm->tm_year = full_year - 1900;

    /* 计算星期（Zeller 公式的简化版） */
    int y = full_year;
    int m = mon;
    if (m < 3) { m += 12; y--; }
    int k = y % 100;
    int j = y / 100;
    int wday = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    tm->tm_wday = (wday + 5) % 7; /* 调整为 0=周日 */

    /* 计算年中第几天 */
    tm->tm_yday = (int)days_since_epoch(full_year, mon, day)
                - (int)days_since_epoch(full_year, 1, 1);

    tm->tm_isdst = 0;

    /* 本地时间增加时区偏移 */
    if (local) {
        long total_sec = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
        total_sec += TIMEZONE_OFFSET;
        /* 处理跨天溢出 */
        while (total_sec >= 86400) {
            total_sec -= 86400;
            tm->tm_mday++;
        }
        tm->tm_hour = total_sec / 3600;
        tm->tm_min  = (total_sec % 3600) / 60;
        tm->tm_sec  = total_sec % 60;
    }
}

/* 本地时间 */
struct tm* localtime(const time_t* timer)
{
    static struct tm tm;
    (void)timer;
    memset(&tm, 0, sizeof(tm));
    rtc_to_tm(&tm, 1);
    return &tm;
}

/* UTC 时间 */
struct tm* gmtime(const time_t* timer)
{
    static struct tm tm;
    (void)timer;
    memset(&tm, 0, sizeof(tm));
    rtc_to_tm(&tm, 0);
    return &tm;
}

/* 从 struct tm 计算时间戳 */
time_t mktime(struct tm* tm)
{
    int year = tm->tm_year + 1900;
    int mon  = tm->tm_mon + 1;
    int day  = tm->tm_mday;

    long long days = days_since_epoch(year, mon, day);
    time_t t = days * 86400
             + tm->tm_hour * 3600
             + tm->tm_min * 60
             + tm->tm_sec;
    return t;
}

/* 星期名称 */
static const char* wday_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* 月份名称 */
static const char* mon_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* 格式化 tm 为字符串 */
char* asctime(const struct tm* tm)
{
    static char buf[26];
    if (!tm) return NULL;

    int n = 0;
    n += 3; /* 星期简写 */
    n += 1; /* 空格 */
    n += 3; /* 月份简写 */
    n += 1; /* 空格 */
    n += 2; /* 日期 */
    n += 1; /* 空格 */
    n += 2; /* 时 */
    n += 1; /* : */
    n += 2; /* 分 */
    n += 1; /* : */
    n += 2; /* 秒 */
    n += 1; /* 空格 */
    n += 4; /* 年份 */
    n += 1; /* \n */
    n += 1; /* \0 */
    (void)n;

    /* 安全格式化，避免缓冲区溢出 */
    const char* wd = (tm->tm_wday >= 0 && tm->tm_wday <= 6)
                     ? wday_names[tm->tm_wday] : "???";
    const char* mn = (tm->tm_mon >= 0 && tm->tm_mon <= 11)
                     ? mon_names[tm->tm_mon] : "???";

    /* 手动构造字符串，不依赖 snprintf */
    char* p = buf;
    p[0] = wd[0]; p[1] = wd[1]; p[2] = wd[2]; p[3] = ' ';
    p[4] = mn[0]; p[5] = mn[1]; p[6] = mn[2]; p[7] = ' ';
    p[8]  = '0' + (tm->tm_mday / 10);
    p[9]  = '0' + (tm->tm_mday % 10);
    p[10] = ' ';
    p[11] = '0' + (tm->tm_hour / 10);
    p[12] = '0' + (tm->tm_hour % 10);
    p[13] = ':';
    p[14] = '0' + (tm->tm_min / 10);
    p[15] = '0' + (tm->tm_min % 10);
    p[16] = ':';
    p[17] = '0' + (tm->tm_sec / 10);
    p[18] = '0' + (tm->tm_sec % 10);
    p[19] = ' ';
    int yr = tm->tm_year + 1900;
    p[20] = '0' + (yr / 1000);
    p[21] = '0' + ((yr / 100) % 10);
    p[22] = '0' + ((yr / 10) % 10);
    p[23] = '0' + (yr % 10);
    p[24] = '\n';
    p[25] = '\0';

    return buf;
}

/* ctime：time_t 转字符串 */
char* ctime(const time_t* timer)
{
    struct tm* tm = localtime(timer);
    if (!tm) return NULL;
    return asctime(tm);
}

/* 格式化时间字符串 */
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm)
{
    if (!s || !format || !tm || max == 0) return 0;

    size_t i = 0;
    for (const char* f = format; *f && i < max - 1; f++) {
        if (*f != '%') {
            s[i++] = *f;
            continue;
        }
        f++;
        if (!*f) break;

        char tmp[32];
        int len = 0;
        switch (*f) {
            case 'a': /* 星期简写 */
                len = 3;
                if (tm->tm_wday >= 0 && tm->tm_wday <= 6)
                    tmp[0] = wday_names[tm->tm_wday][0],
                    tmp[1] = wday_names[tm->tm_wday][1],
                    tmp[2] = wday_names[tm->tm_wday][2];
                else
                    tmp[0] = '?', tmp[1] = '?', tmp[2] = '?';
                break;
            case 'b': /* 月份简写 */
                len = 3;
                if (tm->tm_mon >= 0 && tm->tm_mon <= 11)
                    tmp[0] = mon_names[tm->tm_mon][0],
                    tmp[1] = mon_names[tm->tm_mon][1],
                    tmp[2] = mon_names[tm->tm_mon][2];
                else
                    tmp[0] = '?', tmp[1] = '?', tmp[2] = '?';
                break;
            case 'd': /* 日 (01-31) */
                tmp[0] = '0' + (tm->tm_mday / 10);
                tmp[1] = '0' + (tm->tm_mday % 10);
                len = 2;
                break;
            case 'H': /* 时 (00-23) */
                tmp[0] = '0' + (tm->tm_hour / 10);
                tmp[1] = '0' + (tm->tm_hour % 10);
                len = 2;
                break;
            case 'M': /* 分 (00-59) */
                tmp[0] = '0' + (tm->tm_min / 10);
                tmp[1] = '0' + (tm->tm_min % 10);
                len = 2;
                break;
            case 'm': /* 月 (01-12) */
                tmp[0] = '0' + ((tm->tm_mon + 1) / 10);
                tmp[1] = '0' + ((tm->tm_mon + 1) % 10);
                len = 2;
                break;
            case 'S': /* 秒 (00-59) */
                tmp[0] = '0' + (tm->tm_sec / 10);
                tmp[1] = '0' + (tm->tm_sec % 10);
                len = 2;
                break;
            case 'Y': /* 年 (四位) */
                {
                    int yr = tm->tm_year + 1900;
                    tmp[0] = '0' + (yr / 1000);
                    tmp[1] = '0' + ((yr / 100) % 10);
                    tmp[2] = '0' + ((yr / 10) % 10);
                    tmp[3] = '0' + (yr % 10);
                    len = 4;
                }
                break;
            case '%':
                tmp[0] = '%';
                len = 1;
                break;
            default:
                tmp[0] = *f;
                len = 1;
                break;
        }
        for (int j = 0; j < len && i < max - 1; j++) {
            s[i++] = tmp[j];
        }
    }
    s[i] = '\0';
    return i;
}
