#include "cpu_features.h"
#include <string.h>

static uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static void write_cr0(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr0" :: "r"(val));
}

static uint64_t read_cr4(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static void write_cr4(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr4" :: "r"(val));
}

static uint64_t read_xcr0(void)
{
    uint32_t low, high;
    __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    return ((uint64_t)high << 32) | low;
}

static void write_xcr0(uint64_t val)
{
    __asm__ volatile("xsetbv" :: "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(0));
}

void cpu_features_detect(cpu_features_t* feat)
{
    if (!feat) return;
    memset(feat, 0, sizeof(cpu_features_t));

    feat->cr0 = read_cr0();
    feat->cr4 = read_cr4();

    cpuid_result_t r;
    cpuid(0, 0, &r);
    uint32_t max_leaf = r.eax;

    if (max_leaf >= 1) {
        cpuid(1, 0, &r);
        feat->fpu = (r.edx >> 0) & 1;
        feat->fpu_fxsr = (r.edx >> 24) & 1;
        feat->sse = (r.edx >> 25) & 1;
        feat->sse2 = (r.edx >> 26) & 1;
        feat->sse3 = (r.ecx >> 0) & 1;
        feat->ssse3 = (r.ecx >> 9) & 1;
        feat->sse4_1 = (r.ecx >> 19) & 1;
        feat->sse4_2 = (r.ecx >> 20) & 1;
        feat->avx = (r.ecx >> 28) & 1;
        feat->fma = (r.ecx >> 12) & 1;
        feat->aes = (r.ecx >> 25) & 1;
        feat->pclmulqdq = (r.ecx >> 1) & 1;
        feat->rdrand = (r.ecx >> 30) & 1;
        feat->cmpxchg16b = (r.ecx >> 13) & 1;
        feat->lahf_sahf = (r.ecx >> 0) & 1;
        feat->fsgsbase = (r.ecx >> 0) & 1;
        feat->xsaveopt = (r.ecx >> 0) & 1;
        feat->clflush = (r.edx >> 19) & 1;
        feat->rdtsc = (r.edx >> 4) & 1;

        if (feat->avx && ((r.ecx >> 27) & 1)) {
            feat->xcr0 = read_xcr0();
            feat->fpu_xsave = 1;
        }
    }

    if (max_leaf >= 7) {
        cpuid(7, 0, &r);
        feat->bmi1 = (r.ebx >> 3) & 1;
        feat->bmi2 = (r.ebx >> 8) & 1;
        feat->avx2 = (r.ebx >> 5) & 1;
        feat->erms = (r.ebx >> 9) & 1;
        feat->rdseed = (r.ebx >> 18) & 1;
        feat->adx = (r.ebx >> 19) & 1;
        feat->sha = (r.ebx >> 29) & 1;
        feat->clflushopt = (r.ebx >> 23) & 1;
        feat->clwb = (r.ebx >> 24) & 1;
        feat->avx512f = (r.ebx >> 16) & 1;
        feat->avx512dq = (r.ebx >> 17) & 1;
        feat->avx512cd = (r.ebx >> 28) & 1;
        feat->avx512bw = (r.ebx >> 30) & 1;
        feat->avx512vl = (r.ebx >> 31) & 1;
        feat->avx512ifma = (r.ebx >> 21) & 1;
        feat->prefetchw = (r.ebx >> 0) & 1;

        feat->vaes = (r.ecx >> 9) & 1;
        feat->vpclmulqdq = (r.ecx >> 10) & 1;
        feat->gfni = (r.ecx >> 8) & 1;
        feat->avx512vbmi = (r.ecx >> 1) & 1;
        feat->avx512vpopcntdq = (r.ecx >> 14) & 1;
        feat->xsaves = (r.ecx >> 3) & 1;
        feat->xrstors = (r.ecx >> 4) & 1;
        feat->mwaitx = (r.ecx >> 0) & 1;

        feat->avx512_4vnniw = (r.edx >> 2) & 1;
        feat->avx512_4fmaps = (r.edx >> 3) & 1;
    }

    if (max_leaf >= 0x80000000) {
        cpuid(0x80000000, 0, &r);
        uint32_t max_ext = r.eax;

        if (max_ext >= 0x80000001) {
            cpuid(0x80000001, 0, &r);
            feat->rdtscp = (r.edx >> 27) & 1;
            feat->nx = (r.edx >> 20) & 1;
            feat->lahf_sahf = (r.ecx >> 0) & 1;
            feat->mwait = (r.ecx >> 0) & 1;
        }

        if (max_ext >= 0x80000007) {
            cpuid(0x80000007, 0, &r);
            feat->invariant_tsc = (r.edx >> 8) & 1;
        }

        if (max_ext >= 0x80000008) {
            cpuid(0x80000008, 0, &r);
            (void)r;
        }
    }

    if (feat->fpu_xsave) {
        cpuid(0x0D, 0, &r);
        feat->xsave_size = r.ecx;
    }

    feat->smep = (read_cr4() >> 20) & 1;
    feat->smap = (read_cr4() >> 21) & 1;
}

void cpu_features_enable(cpu_features_t* feat)
{
    if (!feat) return;

    if (feat->fpu) cpu_fpu_init();
    if (feat->sse) cpu_sse_init();
    if (feat->avx) cpu_avx_init();

    uint64_t cr4 = read_cr4();
    if (feat->smep) cr4 |= (1ULL << 20);
    if (feat->smap) cr4 |= (1ULL << 21);
    write_cr4(cr4);
}

void cpu_fpu_init(void)
{
    uint64_t cr0 = read_cr0();
    cr0 &= ~((1ULL << 2) | (1ULL << 3));
    cr0 |= (1ULL << 1);
    write_cr0(cr0);
    __asm__ volatile("fninit");
}

void cpu_fpu_save(void* buf)
{
    __asm__ volatile("fxsave64 %0" : "=m"(*(uint8_t*)buf));
}

void cpu_fpu_restore(const void* buf)
{
    __asm__ volatile("fxrstor64 %0" : : "m"(*(const uint8_t*)buf));
}

void cpu_sse_init(void)
{
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 6);
    cr0 |= (1ULL << 1);
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9) | (1ULL << 10);
    write_cr4(cr4);

    __asm__ volatile("ldmxcsr %0" : : "m"(*(uint32_t[]){0x1F80}));
}

void cpu_avx_init(void)
{
    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 18);
    write_cr4(cr4);

    uint64_t xcr0 = read_xcr0();
    xcr0 |= 0x07;
    write_xcr0(xcr0);
}

void cpu_xsave(void* buf)
{
    __asm__ volatile("xsave64 %0" : "=m"(*(uint8_t*)buf) : "a"(0xFFFFFFFF), "d"(0xFFFFFFFF));
}

void cpu_xrstor(const void* buf)
{
    __asm__ volatile("xrstor64 %0" : : "m"(*(const uint8_t*)buf), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF));
}

uint64_t cpu_xsave_size(void)
{
    cpuid_result_t r;
    cpuid(0x0D, 0, &r);
    return (uint64_t)r.ecx;
}

void cpu_set_fxsr(int enable)
{
    uint64_t cr4 = read_cr4();
    if (enable) cr4 |= (1ULL << 9);
    else cr4 &= ~(1ULL << 9);
    write_cr4(cr4);
}

void cpu_set_xsave(int enable)
{
    uint64_t cr4 = read_cr4();
    if (enable) cr4 |= (1ULL << 18);
    else cr4 &= ~(1ULL << 18);
    write_cr4(cr4);
}

void cpu_set_sse(int enable)
{
    uint64_t cr0 = read_cr0();
    if (enable) cr0 &= ~(1ULL << 6);
    else cr0 |= (1ULL << 6);
    write_cr0(cr0);
}

void cpu_set_avx(int enable)
{
    if (enable) {
        uint64_t xcr0 = read_xcr0();
        xcr0 |= 0x07;
        write_xcr0(xcr0);
    } else {
        uint64_t xcr0 = read_xcr0();
        xcr0 &= ~0x04;
        write_xcr0(xcr0);
    }
}

void cpu_set_smep(int enable)
{
    uint64_t cr4 = read_cr4();
    if (enable) cr4 |= (1ULL << 20);
    else cr4 &= ~(1ULL << 20);
    write_cr4(cr4);
}

void cpu_set_smap(int enable)
{
    uint64_t cr4 = read_cr4();
    if (enable) cr4 |= (1ULL << 21);
    else cr4 &= ~(1ULL << 21);
    write_cr4(cr4);
}

void cpu_set_nx(int enable)
{
    uint64_t msr;
    __asm__ volatile("rdmsr" : "=a"(*(uint32_t*)&msr), "=d"(*((uint32_t*)&msr + 1)) : "c"(0xC0000080));
    if (enable) msr |= (1ULL << 11);
    else msr &= ~(1ULL << 11);
    __asm__ volatile("wrmsr" :: "a"(*(uint32_t*)&msr), "d"(*((uint32_t*)&msr + 1)), "c"(0xC0000080));
}