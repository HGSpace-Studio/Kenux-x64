

#include "kapi_cpumask.h"
#include "kapi.h"

#include <string.h>

void kapi_cpumask_clear(kapi_cpumask_t* mask)
{
    if (mask) {
        memset(mask->bits, 0, sizeof(mask->bits));
    }
}

void kapi_cpumask_set_cpu(int cpu, kapi_cpumask_t* mask)
{
    if (mask && cpu >= 0 && cpu < KAPI_NR_CPUS) {
        mask->bits[cpu >> 6] |= (1ULL << (cpu & 63));
    }
}

void kapi_cpumask_clear_cpu(int cpu, kapi_cpumask_t* mask)
{
    if (mask && cpu >= 0 && cpu < KAPI_NR_CPUS) {
        mask->bits[cpu >> 6] &= ~(1ULL << (cpu & 63));
    }
}

int kapi_cpumask_test_cpu(int cpu, const kapi_cpumask_t* mask)
{
    if (mask && cpu >= 0 && cpu < KAPI_NR_CPUS) {
        return (mask->bits[cpu >> 6] >> (cpu & 63)) & 1ULL;
    }
    return 0;
}

int kapi_cpumask_first(const kapi_cpumask_t* mask)
{
    if (!mask) {
        return KAPI_NR_CPUS;
    }
    for (int i = 0; i < KAPI_NR_CPUS; i++) {
        if (kapi_cpumask_test_cpu(i, mask)) {
            return i;
        }
    }
    return KAPI_NR_CPUS;
}

int kapi_cpumask_next(int cpu, const kapi_cpumask_t* mask)
{
    if (!mask) {
        return KAPI_NR_CPUS;
    }
    for (int i = cpu + 1; i < KAPI_NR_CPUS; i++) {
        if (kapi_cpumask_test_cpu(i, mask)) {
            return i;
        }
    }
    return KAPI_NR_CPUS;
}

int kapi_cpumask_empty(const kapi_cpumask_t* mask)
{
    if (!mask) {
        return 1;
    }
    for (int i = 0; i < KAPI_NR_CPUS / 64; i++) {
        if (mask->bits[i]) {
            return 0;
        }
    }
    return 1;
}

int kapi_cpumask_full(const kapi_cpumask_t* mask)
{
    if (!mask) {
        return 0;
    }
    for (int i = 0; i < KAPI_NR_CPUS; i++) {
        if (!kapi_cpumask_test_cpu(i, mask)) {
            return 0;
        }
    }
    return 1;
}

int kapi_cpumask_weight(const kapi_cpumask_t* mask)
{
    if (!mask) {
        return 0;
    }
    int weight = 0;
    for (int i = 0; i < KAPI_NR_CPUS; i++) {
        if (kapi_cpumask_test_cpu(i, mask)) {
            weight++;
        }
    }
    return weight;
}

void kapi_cpumask_setall(kapi_cpumask_t* mask)
{
    if (mask) {
        memset(mask->bits, 0xFF, sizeof(mask->bits));
    }
}

void kapi_cpumask_and(kapi_cpumask_t* dst, const kapi_cpumask_t* src1, const kapi_cpumask_t* src2)
{
    if (!dst || !src1 || !src2) {
        return;
    }
    for (int i = 0; i < KAPI_NR_CPUS / 64; i++) {
        dst->bits[i] = src1->bits[i] & src2->bits[i];
    }
}

void kapi_cpumask_or(kapi_cpumask_t* dst, const kapi_cpumask_t* src1, const kapi_cpumask_t* src2)
{
    if (!dst || !src1 || !src2) {
        return;
    }
    for (int i = 0; i < KAPI_NR_CPUS / 64; i++) {
        dst->bits[i] = src1->bits[i] | src2->bits[i];
    }
}
