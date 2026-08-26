#ifndef KERNEL_GPU_H
#define KERNEL_GPU_H

#include <arch/types.h>

#define GPU_MAX_DEVICES 8
#define GPU_VENDOR_UNKNOWN 0
#define GPU_VENDOR_INTEL 0x8086
#define GPU_VENDOR_NVIDIA 0x10DE
#define GPU_VENDOR_AMD 0x1002

typedef enum {
    GPU_BACKEND_NONE = 0,
    GPU_BACKEND_UEFI_FRAMEBUFFER,
    GPU_BACKEND_INTEL_PROBE,
    GPU_BACKEND_NVIDIA_PROBE
} gpu_backend_t;

typedef struct {
    bool present;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t revision;
    uint8_t class_code;
    uint8_t subclass;
    uint64_t bar[6];
    uint64_t bar_size[6];
    bool bar_io[6];
    bool mmio_64bit;
    gpu_backend_t backend;
} gpu_device_t;

void gpu_init(void);
uint32_t gpu_count(void);
const gpu_device_t* gpu_get(uint32_t index);
const gpu_device_t* gpu_primary(void);
const char* gpu_vendor_name(uint16_t vendor_id);
const char* gpu_backend_name(gpu_backend_t backend);

#endif
