#ifndef KERNEL_ARCH_X86_64_CPU_FEATURES_H
#define KERNEL_ARCH_X86_64_CPU_FEATURES_H

#include <arch/types.h>

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_result_t;

static inline void cpuid(uint32_t leaf, uint32_t subleaf, cpuid_result_t* result)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
    result->eax = a; result->ebx = b; result->ecx = c; result->edx = d;
}

typedef struct {
    int fpu;
    int fpu_fxsr;
    int fpu_xsave;
    int sse;
    int sse2;
    int sse3;
    int ssse3;
    int sse4_1;
    int sse4_2;
    int avx;
    int avx2;
    int avx512f;
    int avx512dq;
    int avx512cd;
    int avx512bw;
    int avx512vl;
    int avx512ifma;
    int avx512vbmi;
    int avx512vpopcntdq;
    int avx512_4vnniw;
    int avx512_4fmaps;
    int xsaveopt;
    int xsaves;
    int xrstors;
    uint32_t xsave_size;
    int fma;
    int bmi1;
    int bmi2;
    int aes;
    int pclmulqdq;
    int rdrand;
    int rdseed;
    int sha;
    int vaes;
    int vpclmulqdq;
    int gfni;
    int adx;
    int mwait;
    int mwaitx;
    int clflush;
    int clflushopt;
    int clwb;
    int prefetchw;
    int prefetchwt1;
    int rdtsc;
    int rdtscp;
    int invariant_tsc;
    int cmpxchg16b;
    int lahf_sahf;
    int fsgsbase;
    int erms;
    int smep;
    int smap;
    int nx;
    uint64_t cr0;
    uint64_t cr4;
    uint64_t xcr0;
} cpu_features_t;

void cpu_features_detect(cpu_features_t* feat);
void cpu_features_enable(cpu_features_t* feat);
void cpu_fpu_init(void);
void cpu_fpu_save(void* buf);
void cpu_fpu_restore(const void* buf);
void cpu_sse_init(void);
void cpu_avx_init(void);
void cpu_xsave(void* buf);
void cpu_xrstor(const void* buf);
uint64_t cpu_xsave_size(void);
void cpu_set_fxsr(int enable);
void cpu_set_xsave(int enable);
void cpu_set_sse(int enable);
void cpu_set_avx(int enable);
void cpu_set_smep(int enable);
void cpu_set_smap(int enable);
void cpu_set_nx(int enable);

#endif