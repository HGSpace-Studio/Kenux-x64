#include <arch/fpu.h>
#include <arch/memory.h>
#include <string.h>

static fpu_features_t g_fpu_features;
static int fpu_initialized = 0;

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

void fpu_init(void)
{
    if (fpu_initialized) return;

    memset(&g_fpu_features, 0, sizeof(fpu_features_t));

    fpu_detect_features(&g_fpu_features);

    if (g_fpu_features.has_fpu) {
        fpu_enable();
    }

    if (g_fpu_features.has_sse) {
        sse_enable();
    }

    if (g_fpu_features.has_avx) {
        avx_enable();
    }

    if (g_fpu_features.has_fsgsbase) {
        uint64_t fs_base = 0;
        uint64_t gs_base = 0;
        __asm__ volatile ("wrfsbase %0" :: "r"(fs_base));
        __asm__ volatile ("wrgsbase %0" :: "r"(gs_base));
    }

    fpu_initialized = 1;
}

void fpu_detect_features(fpu_features_t* features)
{
    if (!features) return;
    memset(features, 0, sizeof(fpu_features_t));

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;

    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);

        features->has_fpu = (edx & CPUID_FPU) ? 1 : 0;
        features->has_sse = (edx & 0x02000000) ? 1 : 0;
        features->has_sse2 = (edx & 0x04000000) ? 1 : 0;
        features->has_sse3 = (ecx & 0x00000001) ? 1 : 0;
        features->has_ssse3 = (ecx & 0x00000200) ? 1 : 0;
        features->has_sse41 = (ecx & 0x00080000) ? 1 : 0;
        features->has_sse42 = (ecx & 0x00100000) ? 1 : 0;
        features->has_avx = (ecx & CPUID_AVX) ? 1 : 0;
        features->has_fsgsbase = (ecx & 0x00000001) ? 1 : 0;
        features->has_xsave = (ecx & CPUID_XSAVE) ? 1 : 0;
        features->has_rdrand = (ecx & 0x40000000) ? 1 : 0;
        features->has_aesni = (ecx & 0x02000000) ? 1 : 0;

        int os_supports_avx = 0;
        if ((ecx & CPUID_XSAVE) && (ecx & CPUID_OSXSAVE)) {
            uint32_t xcr0 = fpu_xgetbv(0);
            features->xcr0 = xcr0;
            if ((xcr0 & (XCR0_X87 | XCR0_SSE)) == (XCR0_X87 | XCR0_SSE)) {
                os_supports_avx = 1;
            }
        }

        if (!os_supports_avx) {
            features->has_avx = 0;
        }
    }

    if (max_leaf >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);

        features->has_avx2 = (ebx & 0x00000020) ? 1 : 0;
        features->has_bmi1 = (ebx & 0x00000008) ? 1 : 0;
        features->has_bmi2 = (ebx & 0x00000100) ? 1 : 0;
        features->has_fma = (ebx & 0x00000001) ? 1 : 0;
        features->has_rdseed = (ebx & 0x00040000) ? 1 : 0;
        features->has_sha = (ebx & 0x20000000) ? 1 : 0;
        features->has_avx512f = (ebx & 0x00010000) ? 1 : 0;
        features->has_avx512dq = (ebx & 0x00020000) ? 1 : 0;
        features->has_avx512vl = (ebx & 0x00080000) ? 1 : 0;
        features->has_avx512bw = (ebx & 0x00400000) ? 1 : 0;
        features->has_avx512cd = (ebx & 0x00080000) ? 1 : 0;
        features->has_avx512er = (ebx & 0x00800000) ? 1 : 0;
        features->has_avx512pf = (ebx & 0x01000000) ? 1 : 0;
        features->has_clflushopt = (ebx & 0x00000100) ? 1 : 0;
        features->has_clwb = (ebx & 0x00020000) ? 1 : 0;
        features->has_umip = (ecx & 0x00000020) ? 1 : 0;
        features->has_xsaves = (edx & 0x00000001) ? 1 : 0;
    }

    if (features->has_xsave) {
        uint32_t xsave_eax, xsave_ebx, xsave_ecx, xsave_edx;
        cpuid(0x0D, 0, &xsave_eax, &xsave_ebx, &xsave_ecx, &xsave_edx);
        features->xsave_size = xsave_ebx;
    } else {
        features->xsave_size = FPU_FXSAVE_SIZE;
    }
}

int fpu_save(void* buffer)
{
    if (!buffer) return -1;
    if (g_fpu_features.has_xsave) {
        if (g_fpu_features.has_xsaves) {
            __asm__ volatile ("xsaves (%0)" :: "r"(buffer) : "memory");
        } else {
            __asm__ volatile ("xsave (%0)" :: "r"(buffer), "a"((uint32_t)-1), "d"((uint32_t)-1) : "memory");
        }
    } else if (g_fpu_features.has_sse) {
        __asm__ volatile ("fxsave (%0)" :: "r"(buffer) : "memory");
    } else {
        __asm__ volatile ("fnsave (%0)" :: "r"(buffer) : "memory");
    }
    return 0;
}

void fpu_restore(const void* buffer)
{
    if (!buffer) return;
    if (g_fpu_features.has_xsave) {
        if (g_fpu_features.has_xsaves) {
            __asm__ volatile ("xrstors (%0)" :: "r"(buffer), "a"((uint32_t)-1), "d"((uint32_t)-1) : "memory");
        } else {
            __asm__ volatile ("xrstor (%0)" :: "r"(buffer), "a"((uint32_t)-1), "d"((uint32_t)-1) : "memory");
        }
    } else if (g_fpu_features.has_sse) {
        __asm__ volatile ("fxrstor (%0)" :: "r"(buffer) : "memory");
    } else {
        __asm__ volatile ("frstor (%0)" :: "r"(buffer) : "memory");
    }
}

void fpu_enable(void)
{
    __asm__ volatile (
        "clts"
    );
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((uint64_t)1 << 2);
    cr0 |= ((uint64_t)1 << 1);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void fpu_disable(void)
{
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= ((uint64_t)1 << 2);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void sse_enable(void)
{
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((uint64_t)1 << 6);
    cr0 |= ((uint64_t)1 << 1);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= ((uint64_t)1 << 9);
    cr4 |= ((uint64_t)1 << 10);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
}

void avx_enable(void)
{
    if (!g_fpu_features.has_xsave) return;

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= ((uint64_t)1 << 18);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));

    uint32_t xcr0 = fpu_xgetbv(0);
    xcr0 |= XCR0_X87 | XCR0_SSE | XCR0_AVX;
    fpu_xsetbv(0, xcr0);
    g_fpu_features.xcr0 = xcr0;
}

uint32_t fpu_get_xcr0(void)
{
    if (g_fpu_features.has_xsave) {
        return fpu_xgetbv(0);
    }
    return XCR0_X87 | XCR0_SSE;
}

void fpu_set_xcr0(uint32_t mask)
{
    if (!g_fpu_features.has_xsave) return;
    fpu_xsetbv(0, mask);
    g_fpu_features.xcr0 = mask;
}

uint32_t fpu_get_xsave_size(void)
{
    return g_fpu_features.xsave_size;
}

void fpu_init_task(void* xsave_area)
{
    if (!xsave_area) return;
    memset(xsave_area, 0, g_fpu_features.xsave_size);
    if (g_fpu_features.has_xsave) {
        uint8_t* area = (uint8_t*)xsave_area;
        area[0] = 0x00;
        area[1] = 0x00;
        area[2] = 0x00;
        area[3] = 0x00;
        area[4] = 0x00;
        area[5] = 0x00;
        area[6] = 0x80;
        area[7] = 0x1F;
        if (g_fpu_features.has_avx) {
            uint8_t* header = area + FPU_XSAVE_HEADER_OFFSET;
            header[24] = (uint8_t)(XCR0_X87 | XCR0_SSE | XCR0_AVX);
            header[25] = (uint8_t)((XCR0_X87 | XCR0_SSE | XCR0_AVX) >> 8);
        }
    }
}

void fpu_switch_task(void* old_area, void* new_area)
{
    if (old_area) {
        fpu_save(old_area);
    }
    if (new_area) {
        fpu_restore(new_area);
    }
}

void fpu_handle_exception(void)
{
    fpu_enable();
}