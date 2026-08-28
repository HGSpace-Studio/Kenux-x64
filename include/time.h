#ifndef TIME_H
#define TIME_H

#define CLOCKS_PER_SEC 1000000L

typedef long clock_t;

#ifndef _TIME_T_DEFINED
typedef long time_t;
#define _TIME_T_DEFINED
#endif

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
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm* tm);
time_t time(time_t* timer);
time_t timegm(struct tm* tm);
char* asctime(const struct tm* tm);
char* ctime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
size_t strftime(char* s, size_t maxsize, const char* format, const struct tm* tp);
char* strptime(const char* s, const char* format, struct tm* tm);

#endif