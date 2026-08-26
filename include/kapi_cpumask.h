

#ifndef KAPI_CPUMASK_H
#define KAPI_CPUMASK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_NR_CPUS 64

typedef struct {
    uint64_t bits[KAPI_NR_CPUS / 64];
} kapi_cpumask_t;

void kapi_cpumask_clear(kapi_cpumask_t* mask);

void kapi_cpumask_set_cpu(int cpu, kapi_cpumask_t* mask);

void kapi_cpumask_clear_cpu(int cpu, kapi_cpumask_t* mask);

int kapi_cpumask_test_cpu(int cpu, const kapi_cpumask_t* mask);

int kapi_cpumask_first(const kapi_cpumask_t* mask);

int kapi_cpumask_next(int cpu, const kapi_cpumask_t* mask);

int kapi_cpumask_empty(const kapi_cpumask_t* mask);

int kapi_cpumask_full(const kapi_cpumask_t* mask);

int kapi_cpumask_weight(const kapi_cpumask_t* mask);

void kapi_cpumask_setall(kapi_cpumask_t* mask);

void kapi_cpumask_and(kapi_cpumask_t* dst, const kapi_cpumask_t* src1, const kapi_cpumask_t* src2);

void kapi_cpumask_or(kapi_cpumask_t* dst, const kapi_cpumask_t* src1, const kapi_cpumask_t* src2);

#ifdef __cplusplus
}
#endif

#endif
