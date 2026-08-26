

#include "kapi_random.h"
#include "kapi.h"

static inline int rdrand64(uint64_t* val)
{
    unsigned char ok;
    __asm__ volatile ("rdrand %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok;
}

static uint64_t tsc_entropy(void)
{
    uint64_t t1, t2, t3, t4;
    uint32_t lo, hi;
#define _rdtsc(v) do { \
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi)); \
    (v) = ((uint64_t)hi << 32) | lo; \
} while(0)
    _rdtsc(t1);
    _rdtsc(t2);
    _rdtsc(t3);
    _rdtsc(t4);
#undef _rdtsc
    return t1 ^ t2 ^ t3 ^ t4;
}

void kapi_get_random_bytes(void* buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }

    uint8_t* p = (uint8_t*)buf;
    uint64_t val;

    for (size_t i = 0; i < len; i++) {
        if (rdrand64(&val)) {
            p[i] = (uint8_t)(val >> ((i % 8) * 8));
        } else {
            p[i] = (uint8_t)(tsc_entropy() >> ((i % 8) * 8));
        }
    }
}

uint32_t kapi_get_random_u32(void)
{
    uint64_t val;
    if (rdrand64(&val)) {
        return (uint32_t)val;
    }
    return (uint32_t)tsc_entropy();
}

uint64_t kapi_get_random_u64(void)
{
    uint64_t val;
    if (rdrand64(&val)) {
        return val;
    }
    return tsc_entropy();
}
