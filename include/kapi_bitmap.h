

#ifndef KAPI_BITMAP_H
#define KAPI_BITMAP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_BITS_PER_LONG (sizeof(unsigned long) * 8)
#define KAPI_BITMAP_BITS(n) ((n) * KAPI_BITS_PER_LONG)

static inline void kapi_bitmap_set(unsigned long* bitmap, unsigned int bit)
{
    bitmap[bit / KAPI_BITS_PER_LONG] |= (1UL << (bit % KAPI_BITS_PER_LONG));
}

static inline void kapi_bitmap_clear(unsigned long* bitmap, unsigned int bit)
{
    bitmap[bit / KAPI_BITS_PER_LONG] &= ~(1UL << (bit % KAPI_BITS_PER_LONG));
}

static inline int kapi_bitmap_test(const unsigned long* bitmap, unsigned int bit)
{
    return (bitmap[bit / KAPI_BITS_PER_LONG] >> (bit % KAPI_BITS_PER_LONG)) & 1UL;
}

static inline void kapi_bitmap_set_all(unsigned long* bitmap, unsigned int nbits)
{
    unsigned int longs = (nbits + KAPI_BITS_PER_LONG - 1) / KAPI_BITS_PER_LONG;
    for (unsigned int i = 0; i < longs; i++) {
        bitmap[i] = ~0UL;
    }
}

static inline void kapi_bitmap_clear_all(unsigned long* bitmap, unsigned int nbits)
{
    unsigned int longs = (nbits + KAPI_BITS_PER_LONG - 1) / KAPI_BITS_PER_LONG;
    for (unsigned int i = 0; i < longs; i++) {
        bitmap[i] = 0;
    }
}

static inline int kapi_bitmap_find_first(const unsigned long* bitmap, unsigned int nbits)
{
    unsigned int longs = (nbits + KAPI_BITS_PER_LONG - 1) / KAPI_BITS_PER_LONG;
    for (unsigned int i = 0; i < longs; i++) {
        if (bitmap[i] != 0) {
            unsigned int offset = i * KAPI_BITS_PER_LONG;
            unsigned long val = bitmap[i];
            for (unsigned int j = 0; j < KAPI_BITS_PER_LONG && (offset + j) < nbits; j++) {
                if (val & (1UL << j)) {
                    return offset + j;
                }
            }
        }
    }
    return -1;
}

static inline int kapi_bitmap_find_first_zero(const unsigned long* bitmap, unsigned int nbits)
{
    unsigned int longs = (nbits + KAPI_BITS_PER_LONG - 1) / KAPI_BITS_PER_LONG;
    for (unsigned int i = 0; i < longs; i++) {
        if (bitmap[i] != ~0UL) {
            unsigned int offset = i * KAPI_BITS_PER_LONG;
            unsigned long val = bitmap[i];
            for (unsigned int j = 0; j < KAPI_BITS_PER_LONG && (offset + j) < nbits; j++) {
                if (!(val & (1UL << j))) {
                    return offset + j;
                }
            }
        }
    }
    return -1;
}

static inline unsigned int kapi_bitmap_count(const unsigned long* bitmap, unsigned int nbits)
{
    unsigned int count = 0;
    unsigned int longs = (nbits + KAPI_BITS_PER_LONG - 1) / KAPI_BITS_PER_LONG;
    for (unsigned int i = 0; i < longs; i++) {
        unsigned long val = bitmap[i];
        while (val) {
            count += val & 1;
            val >>= 1;
        }
    }
    return count;
}

#ifdef __cplusplus
}
#endif

#endif
