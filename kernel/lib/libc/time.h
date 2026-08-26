#ifndef LIBC_TIME_H
#define LIBC_TIME_H

#include <arch/types.h>

#define CLOCKS_PER_SEC 1000000
#define CLK_TCK CLOCKS_PER_SEC

typedef long clock_t;

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

clock_t clock(void);
time_t time(time_t* timer);
double difftime(time_t time1, time_t time0);
struct tm* localtime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
time_t mktime(struct tm* tm);
char* asctime(const struct tm* tm);
char* ctime(const time_t* timer);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);

#endif
