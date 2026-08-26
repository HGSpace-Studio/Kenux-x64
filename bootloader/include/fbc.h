#ifndef FBC_HPP_
#define FBC_HPP_

#include "efi.h"

enum PixelFormat {
    kPixelRGBR,
    kPixelBGRR
};

struct FrameBufferConfig {
    UINT8 *frame_buffer;
    UINT32 pixels_per_scan_line;
    UINT32 horizontal_resolution;
    UINT32 vertical_resolution;
    enum PixelFormat pixel_format;
};

struct MemoryMapInfo {
    UINTN buffer_size;
    UINTN map_size;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR *buffer;
};

#endif
