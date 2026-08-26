#ifndef ARCH_X86_64_BOOT_H
#define ARCH_X86_64_BOOT_H

#include <stdint.h>

enum PixelFormat {
    kPixelRGBR,
    kPixelBGRR
};

struct FrameBufferConfig {
    uint8_t *frame_buffer;
    uint32_t pixels_per_scan_line;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    enum PixelFormat pixel_format;
};

struct MemoryMapInfo {
    uint64_t buffer_size;
    uint64_t map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
    void *buffer;
};

#endif
