#ifndef ARCH_X86_64_FPU_H
#define ARCH_X86_64_FPU_H

#include <arch/types.h>

#define FPU_XSAVE_AREA_SIZE    4096
#define FPU_FXSAVE_SIZE        512
#define FPU_XSAVE_HEADER_OFFSET 512

#define XCR0_X87       0x00000001
#define XCR0_SSE       0x00000002
#define XCR0_AVX       0x00000004
#define XCR0_BNDREG    0x00000008
#define XCR0_BNDCSR    0x00000010
#define XCR0_OPMASK    0x00000020
#define XCR0_ZMM_HI    0x00000040
#define XCR0_HI16_ZMM  0x00000080
#define XCR0_PT        0x00000100

#define CPUID_FPU      0x00000001
#define CPUID_SSE      0x00600000
#define CPUID_AVX      0x10000000
#define CPUID_AVX2     0x00000020
#define CPUID_FSGSBASE 0x00000001
#define CPUID_XSAVE    0x04000000
#define CPUID_OSXSAVE  0x08000000

typedef struct {
    uint32_t xcr0;
    int      has_fpu;
    int      has_sse;
    int      has_sse2;
    int      has_sse3;
    int      has_ssse3;
    int      has_sse41;
    int      has_sse42;
    int      has_avx;
    int      has_avx2;
    int      has_avx512f;
    int      has_avx512dq;
    int      has_avx512vl;
    int      has_avx512bw;
    int      has_avx512cd;
    int      has_avx512er;
    int      has_avx512pf;
    int      has_fsgsbase;
    int      has_xsave;
    int      has_xsaves;
    int      has_rdrand;
    int      has_rdseed;
    int      has_bmi1;
    int      has_bmi2;
    int      has_fma;
    int      has_aesni;
    int      has_sha;
    int      has_clflushopt;
    int      has_clwb;
    int      has_umip;
    uint32_t xsave_size;
} fpu_features_t;

void fpu_init(void);
void fpu_detect_features(fpu_features_t* features);
int  fpu_save(void* buffer);
void fpu_restore(const void* buffer);
void fpu_enable(void);
void fpu_disable(void);
void sse_enable(void);
void avx_enable(void);
uint32_t fpu_get_xcr0(void);
void fpu_set_xcr0(uint32_t mask);
uint32_t fpu_get_xsave_size(void);
void fpu_init_task(void* xsave_area);
void fpu_switch_task(void* old_area, void* new_area);
void fpu_handle_exception(void);

static inline uint64_t fpu_rdfsbase(void)
{
    uint64_t val;
    __asm__ volatile ("rdfsbase %0" : "=r" (val));
    return val;
}

static inline void fpu_wrfsbase(uint64_t val)
{
    __asm__ volatile ("wrfsbase %0" :: "r" (val));
}

static inline uint64_t fpu_rdgsbase(void)
{
    uint64_t val;
    __asm__ volatile ("rdgsbase %0" : "=r" (val));
    return val;
}

static inline void fpu_wrgsbase(uint64_t val)
{
    __asm__ volatile ("wrgsbase %0" :: "r" (val));
}

static inline uint32_t fpu_xgetbv(uint32_t index)
{
    uint32_t eax, edx;
    __asm__ volatile ("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return eax;
}

static inline void fpu_xsetbv(uint32_t index, uint64_t value)
{
    uint32_t eax = (uint32_t)value;
    uint32_t edx = (uint32_t)(value >> 32);
    __asm__ volatile ("xsetbv" :: "a"(eax), "d"(edx), "c"(index));
}

static inline int fpu_rdrand(uint64_t* val)
{
    int ok;
    __asm__ volatile ("rdrand %1; sbb %0, %0" : "=r"(ok), "=r"(*val));
    return ok & 1;
}

static inline int fpu_rdseed(uint64_t* val)
{
    int ok;
    __asm__ volatile ("rdseed %1; sbb %0, %0" : "=r"(ok), "=r"(*val));
    return ok & 1;
}

#endif