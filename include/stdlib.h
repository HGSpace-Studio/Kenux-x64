#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#ifndef _SIZE_T_DEFINED
typedef unsigned long size_t;
#define _SIZE_T_DEFINED
#endif

#ifndef _SSIZE_T_DEFINED
typedef signed long ssize_t;
#define _SSIZE_T_DEFINED
#endif

#ifndef _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#define _PTRDIFF_T_DEFINED
#endif

#ifndef _WCHAR_T_DEFINED
typedef long wchar_t;
#define _WCHAR_T_DEFINED
#endif

#define NULL ((void*)0)
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

double atof(const char* str);
int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);
double strtod(const char* str, char** endptr);
float strtof(const char* str, char** endptr);
long strtol(const char* str, char** endptr, int base);
unsigned long strtoul(const char* str, char** endptr, int base);
long long strtoll(const char* str, char** endptr, int base);
unsigned long long strtoull(const char* str, char** endptr, int base);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
void* aligned_alloc(size_t alignment, size_t size);

void abort(void);
void exit(int status);
int atexit(void (*func)(void));
char* getenv(const char* name);
int system(const char* command);

void* bsearch(const void* key, const void* base, size_t nmemb,
              size_t size, int (*compar)(const void*, const void*));
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

int abs(int n);
long labs(long n);
long long llabs(long long n);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

int rand(void);
void srand(unsigned int seed);

int mblen(const char* s, size_t n);
size_t mbstowcs(wchar_t* pwcs, const char* s, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
size_t wcstombs(char* s, const wchar_t* pwcs, size_t n);
int wctomb(char* s, wchar_t wchar);

char* realpath(const char* path, char* resolved_path);
double drand48(void);
double erand48(unsigned short xsubi[3]);
long jrand48(unsigned short xsubi[3]);
void lcong48(unsigned short param[7]);
long lrand48(void);
long mrand48(void);
long nrand48(unsigned short xsubi[3]);
unsigned short* seed48(unsigned short seed16v[3]);
void srand48(long seedval);

#endif