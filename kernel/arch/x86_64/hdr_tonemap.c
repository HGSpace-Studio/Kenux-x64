/*
 * HDR tone mapping and color space conversion - freestanding kernel
 * implementation.
 *
 * Provides ST.2086 metadata defaults, ACES/Hable/Reinhard tone mapping
 * operators, ST.2084 PQ and sRGB transfer helpers, per-frame tone mapping
 * of BGRA8 / RGB10A2 / RGBA16F surfaces, BT.709<->BT.2020 color matrix
 * conversion, and per-monitor HDR state tracking.
 *
 * No <math.h> is used: the small set of required float math primitives is
 * implemented locally so the file builds in a freestanding environment.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Freestanding math helpers
 * ------------------------------------------------------------------ */

static float hdtm_sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    float result = x;
    int i;
    for (i = 0; i < 20; i++) {
        result = 0.5f * (result + x / result);
    }
    return result;
}

static float hdtm_powf(float base, float exp) {
    /* Simple powf using exp/log approximation */
    if (base <= 0.0f) return 0.0f;
    /* For integer-ish exponents, use multiplication */
    if (exp == 2.0f) return base * base;
    if (exp == 0.5f) return hdtm_sqrtf(base);
    /* General case: use exp(exp * ln(base)) */
    /* ln(x) ~= 2 * ((x-1)/(x+1) + 1/3 * ((x-1)/(x+1))^3 + ...) */
    float t = (base - 1.0f) / (base + 1.0f);
    float t2 = t * t;
    float ln = 2.0f * t * (1.0f + t2 / 3.0f + t2 * t2 / 5.0f);
    /* exp(x) ~= 1 + x + x^2/2 + x^3/6 + x^4/24 + ... */
    float ex = exp * ln;
    float result = 1.0f;
    float term = 1.0f;
    int i;
    for (i = 1; i <= 15; i++) {
        term *= ex / (float)i;
        result += term;
    }
    return result;
}

static float hdtm_fminf(float a, float b) { return a < b ? a : b; }
static float hdtm_fmaxf(float a, float b) { return a > b ? a : b; }
static float hdtm_clampf(float v, float lo, float hi) { return hdtm_fminf(hdtm_fmaxf(v, lo), hi); }

/* ------------------------------------------------------------------ *
 * Half-float (IEEE 754 binary16) -> float conversion
 * ------------------------------------------------------------------ */

static float hdtm_half_to_float(uint16_t h) {
    uint32_t sign = (uint32_t)((h >> 15) & 0x1u);
    uint32_t exp  = (uint32_t)((h >> 10) & 0x1fu);
    uint32_t mant = (uint32_t)(h & 0x3ffu);
    uint32_t f_bits;
    float f;

    if (exp == 0u) {
        if (mant == 0u) {
            /* signed zero */
            f_bits = sign << 31;
        } else {
            /* subnormal half: normalize into a normal float */
            exp = 1u;
            while (!(mant & 0x400u)) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x3ffu;
            exp += (127u - 15u);
            f_bits = (sign << 31) | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        /* infinity or NaN */
        f_bits = (sign << 31) | 0x7f800000u | (mant << 13);
    } else {
        /* normal */
        exp += (127u - 15u);
        f_bits = (sign << 31) | (exp << 23) | (mant << 13);
    }
    memcpy(&f, &f_bits, sizeof(f));
    return f;
}

/* ------------------------------------------------------------------ *
 * ST.2084 PQ transfer helpers
 * ------------------------------------------------------------------ */

/* ST.2084 PQ EOTF: converts PQ value [0,1] to linear luminance [0, 10000] nit */
float pq_to_linear(float e) {
    /* e is the PQ encoded value [0,1] */
    float m1 = 0.1593017578125f;  /* 2610/16384 */
    float m2 = 78.84375f;          /* 2523/32 */
    float c1 = 0.8359375f;         /* 3424/4096 */
    float c2 = 18.8515625f;        /* 2413/128 */
    float c3 = 18.6875f;           /* 2392/128 */
    float n = hdtm_powf(e, 1.0f / m2);
    float num = hdtm_fmaxf(n - c1, 0.0f);
    float den = c2 - c3 * n;
    float l = hdtm_powf(num / den, 1.0f / m1);
    return l * 10000.0f;  /* in nit */
}

/* ST.2084 PQ OETF: converts linear luminance [0, 10000] to PQ [0,1] */
float linear_to_pq(float l) {
    float L = l / 10000.0f;
    float m1 = 0.1593017578125f;
    float m2 = 78.84375f;
    float c1 = 0.8359375f;
    float c2 = 18.8515625f;
    float c3 = 18.6875f;
    float Lp = hdtm_powf(L, m1);
    float n = (c1 + c2 * Lp) / (1.0f + c3 * Lp);
    return hdtm_powf(n, m2);
}

/* ------------------------------------------------------------------ *
 * sRGB gamma transfer helpers
 * ------------------------------------------------------------------ */

static float srgb_to_linear(float s) {
    if (s <= 0.04045f) return s / 12.92f;
    return hdtm_powf((s + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb(float l) {
    if (l <= 0.0031308f) return 12.92f * l;
    return 1.055f * hdtm_powf(l, 1.0f / 2.4f) - 0.055f;
}

/* ------------------------------------------------------------------ *
 * HDR metadata defaults
 * ------------------------------------------------------------------ */

void hdr_get_default_metadata(HDR_METADATA* meta) {
    /* DCI-P3 primaries in CIExy * 50000, D65 white point */
    /* Max luminance 1000 nit, min 0.001 nit */
    /* MaxCLL 1000, MaxFALL 400 */
    if (meta == NULL) return;

    /* BT.2020 primaries (CIExy * 50000) */
    meta->display_primary_x[0] = 35400;  /* Red   x */
    meta->display_primary_y[0] = 14600;  /* Red   y */
    meta->display_primary_x[1] = 8500;   /* Green x */
    meta->display_primary_y[1] = 39850;  /* Green y */
    meta->display_primary_x[2] = 6550;   /* Blue  x */
    meta->display_primary_y[2] = 14300;  /* Blue  y */

    /* D65 white point (CIExy * 50000) */
    meta->white_point_x = 31270;
    meta->white_point_y = 32900;

    meta->max_luminance = 1000;  /* 1000 nit */
    meta->min_luminance = 10;    /* 0.001 nit (* 10000 granularity) */

    meta->max_content_light_level = 1000;       /* MaxCLL  (nit) */
    meta->max_frame_average_light_level = 400;  /* MaxFALL (nit) */

    meta->valid = 1;
}

/* ------------------------------------------------------------------ *
 * Tone mapping parameter defaults
 * ------------------------------------------------------------------ */

void tonemap_get_defaults(TONEMAP_PARAMS* params) {
    /* ACES filmic, exposure 1.0, white_point 11.2 */
    /* Hable defaults: A=0.22, B=0.30, C=0.10, D=0.20, E=0.01, F=0.30 */
    /* linear_white = 11.2 */
    /* target_luminance = 100 (SDR) */
    /* source_cs = BT2020_PQ, target_cs = SRGB */
    if (params == NULL) return;

    params->operator         = TM_ACES_FILMIC;
    params->exposure         = 1.0f;
    params->white_point      = 11.2f;

    params->shoulder_strength = 0.22f;  /* Hable A */
    params->linear_strength   = 0.30f;  /* Hable B */
    params->linear_angle      = 0.10f;  /* Hable C */
    params->toe_strength      = 0.20f;  /* Hable D */
    params->toe_numerator     = 0.01f;  /* Hable E */
    params->toe_denominator   = 0.30f;  /* Hable F */
    params->linear_white      = 11.2f;

    params->target_luminance  = 100.0f;   /* SDR */
    params->source_peak       = 0.0f;     /* auto */

    params->source_cs = COLOR_SPACE_BT2020_PQ;
    params->target_cs = COLOR_SPACE_SRGB;
}

/* ------------------------------------------------------------------ *
 * Tone mapping operators
 * ------------------------------------------------------------------ */

float tonemap_reinhard(float hdr_value, float white_point) {
    float l = hdr_value;
    float w = white_point * white_point;
    return (l * (1.0f + l / w)) / (1.0f + l);
}

float tonemap_aces_filmic(float hdr_value) {
    /* ACES filmic approximation by Krzysztof Narkowicz */
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    float l = hdr_value;
    return hdtm_clampf((l * (a * l + b)) / (l * (c * l + d) + e), 0.0f, 1.0f);
}

static float hable_curve(float x, const TONEMAP_PARAMS* p) {
    float A = p->shoulder_strength;
    float B = p->linear_strength;
    float C = p->linear_angle;
    float D = p->toe_strength;
    float E = p->toe_numerator;
    float F = p->toe_denominator;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float tonemap_hable(float hdr_value, const TONEMAP_PARAMS* p) {
    float exposure_bias = 2.0f;
    float curr = hable_curve(exposure_bias * hdr_value, p);
    float white_scale = 1.0f / hable_curve(p->linear_white, p);
    return curr * white_scale;
}

/* Select operator and apply to a single linear channel value. */
static float hdtm_tonemap_channel(float v, const TONEMAP_PARAMS* p) {
    switch (p->operator) {
        case TM_NONE:        return hdtm_clampf(v, 0.0f, 1.0f);
        case TM_REINHARD:    return tonemap_reinhard(v, p->white_point);
        case TM_ACES_FILMIC: return tonemap_aces_filmic(v);
        case TM_HABLE:       return tonemap_hable(v, p);
        case TM_GTMO:        return tonemap_aces_filmic(v);  /* fallback */
        default:             return hdtm_clampf(v, 0.0f, 1.0f);
    }
}

/* ------------------------------------------------------------------ *
 * Pixel decode helpers
 * ------------------------------------------------------------------ */

/* Decode one pixel to linear RGB + alpha.
 * BGRA8 / RGB10A2 channels are sRGB-decoded; RGBA16F is already linear. */
static void hdtm_decode_pixel(const unsigned char* p, uint32_t fmt,
                              float* r, float* g, float* b, float* a) {
    if (fmt == 0u) {            /* BGRA8: B,G,R,A bytes */
        float bf = p[0] / 255.0f;
        float gf = p[1] / 255.0f;
        float rf = p[2] / 255.0f;
        *r = srgb_to_linear(rf);
        *g = srgb_to_linear(gf);
        *b = srgb_to_linear(bf);
        *a = p[3] / 255.0f;
    } else if (fmt == 1u) {     /* RGB10A2: packed 10/10/10/2 little-endian */
        uint32_t v;
        float rf, gf, bf;
        memcpy(&v, p, 4);
        rf = (float)(v & 0x3ffu) / 1023.0f;
        gf = (float)((v >> 10) & 0x3ffu) / 1023.0f;
        bf = (float)((v >> 20) & 0x3ffu) / 1023.0f;
        *r = srgb_to_linear(rf);
        *g = srgb_to_linear(gf);
        *b = srgb_to_linear(bf);
        *a = (float)((v >> 30) & 0x3u) / 3.0f;
    } else {                    /* RGBA16F: four half-floats, already linear */
        uint16_t hr, hg, hb, ha;
        memcpy(&hr, p + 0, 2);
        memcpy(&hg, p + 2, 2);
        memcpy(&hb, p + 4, 2);
        memcpy(&ha, p + 6, 2);
        *r = hdtm_half_to_float(hr);
        *g = hdtm_half_to_float(hg);
        *b = hdtm_half_to_float(hb);
        *a = hdtm_half_to_float(ha);
    }
}

/* ------------------------------------------------------------------ *
 * Apply tone mapping to a full frame
 * ------------------------------------------------------------------ */

void tonemap_apply(const TONEMAP_PARAMS* params,
                   const void* src, void* dst,
                   uint32_t width, uint32_t height,
                   uint32_t src_pitch, uint32_t dst_pitch,
                   uint32_t src_format, uint32_t dst_format) {
    /*
     * For each pixel:
     *   1. Decode from source format to linear RGB float
     *   2. Apply exposure
     *   3. Apply tone mapping operator
     *   4. Encode to destination format
     *
     * Full processing is performed when the destination is BGRA8 and the
     * source is BGRA8, RGB10A2 or RGBA16F.  Other combinations fall back to
     * a raw row copy so the call never touches uninitialized memory.
     */
    const unsigned char* s = (const unsigned char*)src;
    unsigned char* d = (unsigned char*)dst;
    uint32_t src_bpp = (src_format == 2u) ? 8u : 4u;

    if (dst_format == 0u &&
        (src_format == 0u || src_format == 1u || src_format == 2u)) {
        uint32_t y, x;
        for (y = 0u; y < height; y++) {
            for (x = 0u; x < width; x++) {
                float r, g, b, a;
                unsigned char ob, og, orr, oa;

                hdtm_decode_pixel(s + (size_t)x * src_bpp, src_format,
                                  &r, &g, &b, &a);

                /* 2. exposure */
                r *= params->exposure;
                g *= params->exposure;
                b *= params->exposure;

                /* 3. tone map per channel (alpha passes through) */
                r = hdtm_tonemap_channel(r, params);
                g = hdtm_tonemap_channel(g, params);
                b = hdtm_tonemap_channel(b, params);

                /* 4. encode BGRA8 via sRGB gamma */
                ob  = (unsigned char)(hdtm_clampf(linear_to_srgb(b), 0.0f, 1.0f) * 255.0f + 0.5f);
                og  = (unsigned char)(hdtm_clampf(linear_to_srgb(g), 0.0f, 1.0f) * 255.0f + 0.5f);
                orr = (unsigned char)(hdtm_clampf(linear_to_srgb(r), 0.0f, 1.0f) * 255.0f + 0.5f);
                oa  = (unsigned char)(hdtm_clampf(a, 0.0f, 1.0f) * 255.0f + 0.5f);

                d[(size_t)x * 4u + 0u] = ob;
                d[(size_t)x * 4u + 1u] = og;
                d[(size_t)x * 4u + 2u] = orr;
                d[(size_t)x * 4u + 3u] = oa;
            }
            s += src_pitch;
            d += dst_pitch;
        }
        return;
    }

    /* Fallback: raw row copy for unsupported format combinations. */
    {
        uint32_t row = (src_pitch < dst_pitch) ? src_pitch : dst_pitch;
        uint32_t y;
        for (y = 0u; y < height; y++) {
            memcpy(d, s, row);
            s += src_pitch;
            d += dst_pitch;
        }
    }
}

/* ------------------------------------------------------------------ *
 * Color space conversion (BT.709 <-> BT.2020)
 * ------------------------------------------------------------------ */

/* BT.709 -> BT.2020 (linear RGB) */
static const float g_m_709_to_2020[3][3] = {
    { 0.6274f,  0.3293f, 0.0433f },
    { 0.0691f,  0.9195f, 0.0114f },
    { 0.0164f,  0.0880f, 0.8956f }
};

/* BT.2020 -> BT.709 (linear RGB) */
static const float g_m_2020_to_709[3][3] = {
    { 1.6605f, -0.5876f, -0.0728f },
    { -0.1266f, 1.1329f, -0.0063f },
    { 0.0129f, -0.1948f,  1.1819f }
};

static void hdtm_apply_matrix(float* r, float* g, float* b, const float m[3][3]) {
    float nr = m[0][0] * (*r) + m[0][1] * (*g) + m[0][2] * (*b);
    float ng = m[1][0] * (*r) + m[1][1] * (*g) + m[1][2] * (*b);
    float nb = m[2][0] * (*r) + m[2][1] * (*g) + m[2][2] * (*b);
    *r = nr;
    *g = ng;
    *b = nb;
}

void color_space_convert(COLOR_SPACE from, COLOR_SPACE to,
                         const void* src, void* dst,
                         uint32_t width, uint32_t height,
                         uint32_t pitch) {
    /* If from == to, just memcpy */
    if (from == to) {
        memcpy(dst, src, (size_t)pitch * height);
        return;
    }

    /* Determine BT.709 <-> BT.2020 direction. */
    {
        int from_709  = (from == COLOR_SPACE_SRGB || from == COLOR_SPACE_SRGB_LINEAR || from == COLOR_SPACE_SCRGB);
        int to_709    = (to   == COLOR_SPACE_SRGB || to   == COLOR_SPACE_SRGB_LINEAR || to   == COLOR_SPACE_SCRGB);
        int from_2020 = (from == COLOR_SPACE_BT2020_PQ || from == COLOR_SPACE_BT2020_LINEAR);
        int to_2020   = (to   == COLOR_SPACE_BT2020_PQ || to   == COLOR_SPACE_BT2020_LINEAR);
        const float (*m)[3] = NULL;

        if (from_709 && to_2020)      m = g_m_709_to_2020;
        else if (from_2020 && to_709) m = g_m_2020_to_709;

        if (m == NULL) {
            /* Simplified: no matrix for this pair, just copy. */
            memcpy(dst, src, (size_t)pitch * height);
            return;
        }

        /* Apply 3x3 color matrix per pixel for BGRA8. */
        {
            const unsigned char* s = (const unsigned char*)src;
            unsigned char* d = (unsigned char*)dst;
            uint32_t y, x;
            for (y = 0u; y < height; y++) {
                for (x = 0u; x < width; x++) {
                    unsigned char B = s[(size_t)x * 4u + 0u];
                    unsigned char G = s[(size_t)x * 4u + 1u];
                    unsigned char R = s[(size_t)x * 4u + 2u];
                    unsigned char A = s[(size_t)x * 4u + 3u];
                    float rf = R / 255.0f;
                    float gf = G / 255.0f;
                    float bf = B / 255.0f;

                    hdtm_apply_matrix(&rf, &gf, &bf, m);

                    rf = hdtm_clampf(rf, 0.0f, 1.0f);
                    gf = hdtm_clampf(gf, 0.0f, 1.0f);
                    bf = hdtm_clampf(bf, 0.0f, 1.0f);

                    d[(size_t)x * 4u + 0u] = (unsigned char)(bf * 255.0f + 0.5f);
                    d[(size_t)x * 4u + 1u] = (unsigned char)(gf * 255.0f + 0.5f);
                    d[(size_t)x * 4u + 2u] = (unsigned char)(rf * 255.0f + 0.5f);
                    d[(size_t)x * 4u + 3u] = A;
                }
                s += pitch;
                d += pitch;
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 * HDR monitor state (per-monitor, up to 8)
 * ------------------------------------------------------------------ */

static struct { int hdr_enabled; HDR_METADATA meta; } g_hdr_state[8];

int hdr_set_metadata(uint32_t monitor_id, const HDR_METADATA* meta) {
    if (monitor_id >= 8u || meta == NULL) return 0;
    g_hdr_state[monitor_id].meta = *meta;
    g_hdr_state[monitor_id].meta.valid = 1;
    return 1;
}

int hdr_enable(uint32_t monitor_id, int enable) {
    if (monitor_id >= 8u) return 0;
    g_hdr_state[monitor_id].hdr_enabled = enable ? 1 : 0;
    return 1;
}

int hdr_is_capable(uint32_t monitor_id) {
    return (monitor_id < 8u) ? 1 : 0;
}
