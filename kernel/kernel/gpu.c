#include <gpu.h>
#include <pci.h>
#include <arch/pci.h>
#include <string.h>

static gpu_device_t gpu_devices[GPU_MAX_DEVICES];
static uint32_t gpu_device_count;

static uint64_t gpu_bar_value(uint32_t low, uint32_t high, bool is_64) {
    uint64_t value = (uint64_t)(low & ~0x0Fu);
    if (is_64) value |= (uint64_t)high << 32;
    return value;
}

static bool gpu_is_display(uint32_t class_reg) {
    uint8_t class_code = (uint8_t)(class_reg >> 24);
    uint8_t subclass = (uint8_t)(class_reg >> 16);
    return class_code == 0x03 && (subclass == 0x00 || subclass == 0x02);
}

static void gpu_probe_device(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t id = pci_read_config_dword(bus, dev, func, 0x00);
    uint32_t class_reg = pci_read_config_dword(bus, dev, func, 0x08);
    if (id == 0xFFFFFFFFu || !gpu_is_display(class_reg) || gpu_device_count >= GPU_MAX_DEVICES) return;

    gpu_device_t* out = &gpu_devices[gpu_device_count++];
    memset(out, 0, sizeof(*out));
    out->present = true;
    out->vendor_id = (uint16_t)(id & 0xFFFFu);
    out->device_id = (uint16_t)(id >> 16);
    out->bus = bus;
    out->device = dev;
    out->function = func;
    out->revision = pci_read_config_byte(bus, dev, func, 0x08);
    out->class_code = (uint8_t)(class_reg >> 24);
    out->subclass = (uint8_t)(class_reg >> 16);

    for (uint8_t i = 0; i < PCI_MAX_BARS; i++) {
        uint8_t offset = (uint8_t)(0x10 + i * 4);
        uint32_t low = pci_read_config_dword(bus, dev, func, offset);
        if (low == 0 || low == 0xFFFFFFFFu) continue;
        out->bar_io[i] = (low & 0x1u) != 0;
        bool is_64 = !out->bar_io[i] && ((low >> 1) & 0x3u) == 0x2u;
        uint32_t high = is_64 && i + 1 < PCI_MAX_BARS
            ? pci_read_config_dword(bus, dev, func, (uint8_t)(offset + 4)) : 0;
        out->bar[i] = gpu_bar_value(low, high, is_64);
        out->mmio_64bit |= is_64;
        if (is_64) i++;
    }

    if (out->vendor_id == GPU_VENDOR_INTEL) {
        out->backend = GPU_BACKEND_INTEL_PROBE;
    } else if (out->vendor_id == GPU_VENDOR_NVIDIA) {
        out->backend = GPU_BACKEND_NVIDIA_PROBE;
    } else {
        out->backend = GPU_BACKEND_UEFI_FRAMEBUFFER;
    }
}

void gpu_init(void) {
    memset(gpu_devices, 0, sizeof(gpu_devices));
    gpu_device_count = 0;
    for (uint16_t bus = 0; bus < PCI_MAX_BUSSES; bus++) {
        for (uint8_t dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            for (uint8_t func = 0; func < PCI_MAX_FUNCTIONS; func++) {
                gpu_probe_device((uint8_t)bus, dev, func);
                if (func == 0) {
                    uint8_t header = pci_read_config_byte((uint8_t)bus, dev, func, 0x0E);
                    if (!(header & PCI_HEADER_TYPE_MULTIFUNC)) break;
                }
            }
        }
    }
}

uint32_t gpu_count(void) { return gpu_device_count; }

const gpu_device_t* gpu_get(uint32_t index) {
    return index < gpu_device_count ? &gpu_devices[index] : NULL;
}

const gpu_device_t* gpu_primary(void) {
    return gpu_device_count ? &gpu_devices[0] : NULL;
}

const char* gpu_vendor_name(uint16_t vendor_id) {
    if (vendor_id == GPU_VENDOR_INTEL) return "Intel";
    if (vendor_id == GPU_VENDOR_NVIDIA) return "NVIDIA";
    if (vendor_id == GPU_VENDOR_AMD) return "AMD";
    return "Unknown";
}

const char* gpu_backend_name(gpu_backend_t backend) {
    switch (backend) {
        case GPU_BACKEND_UEFI_FRAMEBUFFER: return "UEFI framebuffer";
        case GPU_BACKEND_INTEL_PROBE: return "Intel probe";
        case GPU_BACKEND_NVIDIA_PROBE: return "NVIDIA probe";
        default: return "none";
    }
}
