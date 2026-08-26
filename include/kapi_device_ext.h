#ifndef KAPI_DEVICE_EXT_H
#define KAPI_DEVICE_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_DEV_MAX_NAME     64
#define KAPI_DEV_MAX_PATH     256
#define KAPI_DEV_MAX_CLASS    32
#define KAPI_DEV_MAX_DRIVER   32
#define KAPI_DEV_MAX_VENDOR   64
#define KAPI_DEV_MAX_DESC     128

#define KAPI_DEV_TYPE_NONE        0
#define KAPI_DEV_TYPE_CHAR        1
#define KAPI_DEV_TYPE_BLOCK       2
#define KAPI_DEV_TYPE_NETWORK     3
#define KAPI_DEV_TYPE_USB         4
#define KAPI_DEV_TYPE_PCI         5
#define KAPI_DEV_TYPE_PLATFORM    6
#define KAPI_DEV_TYPE_I2C         7
#define KAPI_DEV_TYPE_SPI         8
#define KAPI_DEV_TYPE_GPIO        9
#define KAPI_DEV_TYPE_PWM         10
#define KAPI_DEV_TYPE_ADC         11
#define KAPI_DEV_TYPE_DAC         12
#define KAPI_DEV_TYPE_RTC         13
#define KAPI_DEV_TYPE_WATCHDOG    14
#define KAPI_DEV_TYPE_FRAMEBUFFER 15
#define KAPI_DEV_TYPE_INPUT       16
#define KAPI_DEV_TYPE_AUDIO       17
#define KAPI_DEV_TYPE_VIDEO       18
#define KAPI_DEV_TYPE_STORAGE     19
#define KAPI_DEV_TYPE_MEMORY      20
#define KAPI_DEV_TYPE_MISC        21

#define KAPI_DEV_STATE_UNKNOWN    0
#define KAPI_DEV_STATE_NOT_PRESENT 1
#define KAPI_DEV_STATE_DISABLED   2
#define KAPI_DEV_STATE_BINDING    3
#define KAPI_DEV_STATE_UNBINDING  4
#define KAPI_DEV_STATE_ACTIVE     5
#define KAPI_DEV_STATE_SUSPENDED  6

#define KAPI_PCI_VENDOR_INVALID  0xFFFF
#define KAPI_PCI_DEVICE_INVALID  0xFFFF

typedef uint32_t kapi_dev_id_t;
typedef uint16_t kapi_pci_vendor_id;
typedef uint16_t kapi_pci_device_id;

struct kapi_pci_device_id {
    kapi_pci_vendor_id vendor;
    kapi_pci_device_id device;
    uint32_t subvendor;
    uint32_t subdevice;
    uint32_t class_id;
    uint32_t class_mask;
    unsigned long driver_data;
};

typedef struct {
    kapi_dev_id_t id;
    char name[KAPI_DEV_MAX_NAME];
    char path[KAPI_DEV_MAX_PATH];
    int type;
    int state;
    int major;
    int minor;
    uint32_t class_code;
    uint32_t vendor_id;
    uint32_t device_id;
    char driver_name[KAPI_DEV_MAX_DRIVER];
    char parent_name[KAPI_DEV_MAX_NAME];
    bool can_read;
    bool can_write;
    bool can_mmap;
    bool is_removable;
    bool is_hotpluggable;
    size_t size;
    uint64_t block_size;
    void* private_data;
} kapi_device_info_t;

typedef struct {
    kapi_dev_id_t id;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t class_code[3];
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[6];
    uint32_t bar_size[6];
    uint8_t interrupt_pin;
    uint8_t interrupt_line;
    uint32_t subsystem_vendor_id;
    uint32_t subsystem_id;
    uint32_t expansion_rom_base;
    uint8_t capabilities_ptr;
    uint8_t interrupt_line;
    uint32_t irq;
    void* mmio_base[6];
    size_t mmio_size[6];
    uint16_t io_base[6];
    size_t io_size[6];
    bool is_enabled;
    bool is_bus_master;
    bool has_irq;
    bool has_mmio;
    bool has_io;
    bool is_pcie;
    uint8_t pcie_link_width;
    uint8_t pcie_link_speed;
} kapi_pci_info_t;

typedef struct {
    kapi_dev_id_t id;
    uint8_t bus_num;
    uint8_t dev_addr;
    uint16_t vid;
    uint16_t pid;
    uint16_t device_class;
    uint16_t device_subclass;
    uint16_t device_protocol;
    uint8_t config_value;
    uint8_t num_interfaces;
    uint8_t num_configurations;
    char manufacturer[KAPI_DEV_MAX_VENDOR];
    char product[KAPI_DEV_MAX_VENDOR];
    char serial_number[KAPI_DEV_MAX_VENDOR];
    uint8_t speed;
    bool is_high_speed;
    bool is_full_speed;
    bool is_low_speed;
    bool is_super_speed;
    uint8_t max_packet_size0;
} kapi_usb_info_t;

typedef struct {
    void* base_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t red_offset;
    uint32_t red_size;
    uint32_t green_offset;
    uint32_t green_size;
    uint32_t blue_offset;
    uint32_t blue_size;
    uint32_t reserved_offset;
    uint32_t reserved_size;
    uint32_t pixel_format;
    bool is_linear;
    bool has_hw_cursor;
    bool has_acceleration;
    bool supports_vsync;
    uint32_t cursor_max_width;
    uint32_t cursor_max_height;
} kapi_fb_info_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint32_t bpp;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    bool is_interlaced;
    bool is_double_scan;
    bool is_sync_positive;
} kapi_display_mode_t;

typedef struct {
    kapi_dev_id_t id;
    char name[KAPI_DEV_MAX_NAME];
    uint32_t input_type;
    uint32_t capabilities;
    bool has_key;
    bool has_rel_axes;
    bool has_abs_axes;
    bool has_misc;
    uint16_t key_bitmask_len;
    uint16_t rel_bitmask_len;
    uint16_t abs_bitmask_len;
    const unsigned long* key_bitmask;
    const unsigned long* rel_bitmask;
    const unsigned long* abs_bitmask;
    int abs_min[ABS_CNT];
    int abs_max[ABS_CNT];
    int abs_fuzz[ABS_CNT];
    int abs_flat[ABS_CNT];
} kapi_input_dev_info_t;

typedef int (*kapi_dev_probe_t)(kapi_dev_id_t dev);
typedef int (*kapi_dev_remove_t)(kapi_dev_id_t dev);
typedef int (*kapi_dev_open_t)(kapi_dev_id_t dev, int flags);
typedef int (*kapi_dev_close_t)(kapi_dev_id_t dev);
typedef ssize_t (*kapi_dev_read_t)(kapi_dev_id_t dev, void* buf, size_t count, off_t offset);
typedef ssize_t (*kapi_dev_write_t)(kapi_dev_id_t dev, const void* buf, size_t count, off_t offset);
typedef int (*kapi_dev_ioctl_t)(kapi_dev_id_t dev, unsigned long request, void* arg);
typedef int (*kapi_dev_mmap_t)(kapi_dev_id_t dev, void** addr, size_t len, int prot, int flags);
typedef int (*kapi_dev_poll_t)(kapi_dev_id_t dev, short* events, int timeout);
typedef int (*kapi_dev_suspend_t)(kapi_dev_id_t dev);
typedef int (*kapi_dev_resume_t)(kapi_dev_id_t dev);
typedef int (*kapi_dev_reset_t)(kapi_dev_id_t dev);

typedef struct {
    kapi_dev_probe_t probe;
    kapi_dev_remove_t remove;
    kapi_dev_open_t open;
    kapi_dev_close_t close;
    kapi_dev_read_t read;
    kapi_dev_write_t write;
    kapi_dev_ioctl_t ioctl;
    kapi_dev_mmap_t mmap;
    kapi_dev_poll_t poll;
    kapi_dev_suspend_t suspend;
    kapi_dev_resume_t resume;
    kapi_dev_reset_t reset;
} kapi_device_ops_t;

int kapi_device_register(const char* name, int type, const kapi_device_ops_t* ops, void* priv);

int kapi_device_unregister(kapi_dev_id_t dev);

int kapi_device_open(kapi_dev_id_t dev, int flags);

int kapi_device_close(kapi_dev_id_t dev);

ssize_t kapi_device_read(kapi_dev_id_t dev, void* buf, size_t count, off_t offset);

ssize_t kapi_device_write(kapi_dev_id_t dev, const void* buf, size_t count, off_t offset);

int kapi_device_ioctl(kapi_dev_id_t dev, unsigned long request, void* arg);

int kapi_device_mmap(kapi_dev_id_t dev, void** addr, size_t len, int prot, int flags);

int kapi_device_poll(kapi_dev_id_t dev, short* events, int timeout);

int kapi_device_suspend(kapi_dev_id_t dev);

int kapi_device_resume(kapi_dev_id_t dev);

int kapi_device_reset(kapi_dev_id_t dev);

kapi_dev_id_t kapi_device_find_by_name(const char* name);

kapi_dev_id_t kapi_device_find_by_path(const char* path);

kapi_dev_id_t kapi_device_find_by_class(int class_code);

int kapi_device_get_info(kapi_dev_id_t dev, kapi_device_info_t* info);

int kapi_device_set_private_data(kapi_dev_id_t dev, void* data);

void* kapi_device_get_private_data(kapi_dev_id_t dev);

int kapi_device_get_count(void);

int kapi_device_list(kapi_device_info_t* devs, int count);

int kapi_device_list_by_type(int type, kapi_device_info_t* devs, int count);

char* kapi_device_create_node(kapi_dev_id_t dev, const char* name, int mode);

int kapi_device_remove_node(const char* path);

int kapi_chardev_register(int major, int minor, const char* name, const kapi_device_ops_t* ops);

int kapi_chardev_unregister(int major, int minor);

int kapi_blockdev_register(int major, int minor, const char* name, const kapi_device_ops_t* ops,
                           size_t block_size, uint64_t num_blocks);

int kapi_blockdev_unregister(int major, int minor);

int kapi_blockdev_read_blocks(int major, int minor, uint64_t block, uint32_t count, void* buffer);

int kapi_blockdev_write_blocks(int major, int minor, uint64_t block, uint32_t count, const void* buffer);

int kapi_blockdev_flush(int major, int minor);

int kapi_blockdev_ioctl(int major, int minor, unsigned long request, void* arg);

int kapi_pci_scan_bus(uint8_t bus);

int kapi_pci_find_device(uint16_t vendor, uint16_t device, kapi_pci_info_t* info);

int kapi_pci_find_class(uint32_t class_code, kapi_pci_info_t* info);

int kapi_pci_enable_device(kapi_pci_info_t* pci);

int kapi_pci_disable_device(kapi_pci_info_t* pci);

int kapi_pci_set_master(kapi_pci_info_t* pci);

int kapi_pci_clear_master(kapi_pci_info_t* pci);

uint32_t kapi_pci_read_config_byte(kapi_pci_info_t* pci, int offset);

uint32_t kapi_pci_read_config_word(kapi_pci_info_t* pci, int offset);

uint32_t kapi_pci_read_config_dword(kapi_pci_info_t* pci, int offset);

void kapi_pci_write_config_byte(kapi_pci_info_t* pci, int offset, uint8_t value);

void kapi_pci_write_config_word(kapi_pci_info_t* pci, int offset, uint16_t value);

void kapi_pci_write_config_dword(kapi_pci_info_t* pci, int offset, uint32_t value);

void* kapi_pci_map_bar(kapi_pci_info_t* pci, int bar);

void kapi_pci_unmap_bar(kapi_pci_info_t* pci, int bar, void* addr);

int kapi_pci_request_irq(kapi_pci_info_t* pci, void (*handler)(int, void*, void*), void* data);

void kapi_pci_free_irq(kapi_pci_info_t* pci);

int kapi_pci_set_dma_mask(kapi_pci_info_t* pci, uint64_t mask);

int kapi_pci_alloc_consistent(kapi_pci_info_t* pci, size_t size, dma_addr_t* dma_handle);

void kapi_pci_free_consistent(kapi_pci_info_t* pci, size_t size, void* cpu_addr, dma_addr_t dma_handle);

int kapi_fb_open(kapi_dev_id_t dev, int flags);

int kapi_fb_close(kapi_dev_id_t dev);

int kapi_fb_get_info(kapi_dev_id_t dev, kapi_fb_info_t* info);

int kapi_fb_set_mode(kapi_dev_id_t dev, const kapi_display_mode_t* mode);

int kapi_fb_get_modes(kapi_dev_id_t dev, kapi_display_mode_t* modes, int count);

int kapi_fb_pan_display(kapi_dev_id_t dev, uint32_t x, uint32_t y);

int kapi_fb_blank(kapi_dev_id_t dev, int mode);

int kapi_fb_fillrect(kapi_dev_id_t dev, int dx, int dy, int width, int height, uint32_t color);

int kapi_fb_copyarea(kapi_dev_id_t dev, int sx, int sy, int dx, int dy, int width, int height);

int kapi_fb_imageblit(kapi_dev_id_t dev, int dx, int dy, int width, int height, const void* image);

int kapi_fb_cursor(kapi_dev_id_t dev, const kapi_fb_cursor_t* cursor);

int kapi_fb_setcolreg(kapi_dev_id_t dev, unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp);

int kapi_input_open(kapi_dev_id_t dev, int flags);

int kapi_input_close(kapi_dev_id_t dev);

int kapi_input_get_info(kapi_dev_id_t dev, kapi_input_dev_info_t* info);

ssize_t kapi_input_event_read(kapi_dev_id_t dev, struct input_event* events, size_t count);

int kapi_input_grab(kapi_dev_id_t dev);

int kapi_input_ungrab(kapi_dev_id_t dev);

int kapi_input_set_leds(kapi_dev_id_t dev, uint16_t leds);

int kapi_input_get_key_state(kapi_dev_id_t dev, int keycode);

int kapi_input_get_abs_state(kapi_dev_id_t dev, int axis, int* value);

#ifdef __cplusplus
}
#endif

#endif