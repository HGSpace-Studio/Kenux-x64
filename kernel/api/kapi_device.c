

#include "kapi_device.h"
#include "kapi.h"

#include <arch/drivers.h>
#include <arch/io.h>
#include <arch/pit.h>
#include <arch/hpet.h>
#include <arch/memory.h>
#include <arch/nvme.h>
#include <arch/ahci.h>
#include <arch/keyboard.h>
#include <timer.h>
#include <stdio.h>
#include <string.h>

struct kapi_device {
    char name[64];
    int type;
    int valid;
};

#define KAPI_MAX_IRQ 256
static struct {
    void (*handler)(int, void*);
    void* arg;
    int valid;
} kapi_irq_table[KAPI_MAX_IRQ];

#define KAPI_MAX_TIMERS 64
static struct {
    uint64_t interval;
    uint64_t last_trigger;
    void (*callback)(void*);
    void* arg;
    int periodic;
    int valid;
} kapi_timer_table[KAPI_MAX_TIMERS];

static int kapi_dev_initialized = 0;
static uint64_t kapi_boot_time = 0;
static uint64_t tsc_frequency = 0;
static int kapi_active_dev_count = 0;

/* 串口端口定义 */
#define COM1_PORT  0x3F8
#define COM1_LSR   (COM1_PORT + 5)

/* NVMe 控制器外部声明 */
extern nvme_controller_t nvme_ctrl;

/* 读取 TSC */
static inline uint64_t read_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* 校准 TSC 频率，使用 CPUID 0x15 或默认值 */
static void calibrate_tsc(void)
{
    /* 尝试 CPUID 0x15 (TSC frequency leaf) */
    uint32_t eax, ebx, ecx, edx;
    eax = 0x15;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (ebx != 0 && eax != 0) {
        /* ratio = ebx/eax, ecx = crystal freq (Hz) */
        tsc_frequency = ((uint64_t)ecx * ebx) / eax;
        if (tsc_frequency > 0) return;
    }

    /* 回退：用 CPUID 0x16 (Processor frequency leaf) */
    eax = 0x16;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (eax != 0) {
        /* eax = CPU base frequency in MHz */
        tsc_frequency = (uint64_t)eax * 1000000ULL;
        if (tsc_frequency > 0) return;
    }

    /* 最终回退：默认 2GHz */
    tsc_frequency = 2000000000ULL;
}

/* 检查串口接收缓冲区是否有数据 */
static int serial_can_read(void)
{
    return (inb(COM1_LSR) & 0x01) != 0;
}

/* 检查串口发送缓冲区是否为空 */
static int serial_can_write(void)
{
    return (inb(COM1_LSR) & 0x20) != 0;
}

/* 从串口读取一个字节 */
static uint8_t serial_read_byte(void)
{
    while (!serial_can_read()) {
        __asm__ volatile ("pause");
    }
    return inb(COM1_PORT);
}

/* 向串口写入一个字节 */
static void serial_write_byte(uint8_t c)
{
    while (!serial_can_write()) {
        __asm__ volatile ("pause");
    }
    outb(COM1_PORT, c);
}

int kapi_device_init(void)
{
    if (kapi_dev_initialized) {
        return KAPI_OK;
    }

    memset(kapi_irq_table, 0, sizeof(kapi_irq_table));
    memset(kapi_timer_table, 0, sizeof(kapi_timer_table));
    kapi_boot_time = 0;
    kapi_active_dev_count = 0;
    kapi_dev_initialized = 1;

    calibrate_tsc();

    return KAPI_OK;
}

kapi_dev_t kapi_dev_open(const char* name, int mode)
{
    if (!name) {
        return NULL;
    }

    kapi_dev_t dev = (kapi_dev_t)memory_alloc(sizeof(struct kapi_device));
    if (!dev) {
        return NULL;
    }

    memset(dev, 0, sizeof(*dev));
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->valid = 1;
    kapi_active_dev_count++;

    if (strstr(name, "/dev/sd") || strstr(name, "/dev/hd")) {
        dev->type = KAPI_DEV_BLOCK;
    } else if (strstr(name, "/dev/tty") || strstr(name, "/dev/com")) {
        dev->type = KAPI_DEV_CHAR;
    } else if (strstr(name, "/dev/eth") || strstr(name, "/dev/wlan")) {
        dev->type = KAPI_DEV_NET;
    } else if (strstr(name, "/dev/audio") || strstr(name, "/dev/dsp")) {
        dev->type = KAPI_DEV_SOUND;
    } else if (strstr(name, "/dev/fb") || strstr(name, "/dev/video")) {
        dev->type = KAPI_DEV_VIDEO;
    } else if (strstr(name, "/dev/input") || strstr(name, "/dev/mouse")) {
        dev->type = KAPI_DEV_INPUT;
    } else {
        dev->type = KAPI_DEV_UNKNOWN;
    }

    return dev;
}

int kapi_dev_close(kapi_dev_t dev)
{
    if (!dev) {
        return KAPI_EINVAL;
    }

    if (dev->valid) {
        dev->valid = 0;
        kapi_active_dev_count--;
    }
    memory_free(dev);
    return KAPI_OK;
}

int64_t kapi_dev_read(kapi_dev_t dev, void* buf, size_t count)
{
    if (!dev || !dev->valid || !buf) {
        return KAPI_EINVAL;
    }

    if (count == 0) {
        return 0;
    }

    switch (dev->type) {
        case KAPI_DEV_BLOCK: {
            /* 先尝试 NVMe，如不可用则 fallback 到 AHCI */
            if (nvme_probe() && nvme_ctrl.state == NVME_CTRL_STATE_READY) {
                /* 使用命名空间 1，LBA 0，读取一个扇区（512字节）作为示例 */
                int ret = nvme_read(&nvme_ctrl, 1, 0, 1, buf);
                if (ret == 0) {
                    return 512;
                }
            }
            /* Fallback 到 AHCI */
            if (ahci_get_controller_count() > 0) {
                int ret = ahci_read_sector(0, 0, buf);
                if (ret == 0) {
                    return 512;
                }
            }
            return KAPI_ERROR;
        }

        case KAPI_DEV_CHAR: {
            uint8_t* p = (uint8_t*)buf;
            size_t i;
            for (i = 0; i < count; i++) {
                p[i] = serial_read_byte();
            }
            return (int64_t)i;
        }

        case KAPI_DEV_INPUT: {
            key_event_t ev = keyboard_read();
            if (count >= sizeof(key_event_t)) {
                memcpy(buf, &ev, sizeof(key_event_t));
                return sizeof(key_event_t);
            } else {
                memcpy(buf, &ev, count);
                return (int64_t)count;
            }
        }

        default:
            return KAPI_ENOSYS;
    }
}

int64_t kapi_dev_write(kapi_dev_t dev, const void* buf, size_t count)
{
    if (!dev || !dev->valid || !buf) {
        return KAPI_EINVAL;
    }

    if (count == 0) {
        return 0;
    }

    switch (dev->type) {
        case KAPI_DEV_BLOCK: {
            /* 先尝试 NVMe，如不可用则 fallback 到 AHCI */
            if (nvme_probe() && nvme_ctrl.state == NVME_CTRL_STATE_READY) {
                int ret = nvme_write(&nvme_ctrl, 1, 0, 1, buf);
                if (ret == 0) {
                    return 512;
                }
            }
            /* Fallback 到 AHCI */
            if (ahci_get_controller_count() > 0) {
                int ret = ahci_write_sector(0, 0, buf);
                if (ret == 0) {
                    return 512;
                }
            }
            return KAPI_ERROR;
        }

        case KAPI_DEV_CHAR: {
            const uint8_t* p = (const uint8_t*)buf;
            size_t i;
            for (i = 0; i < count; i++) {
                serial_write_byte(p[i]);
            }
            return (int64_t)i;
        }

        default:
            return KAPI_ENOSYS;
    }
}

int kapi_dev_ioctl(kapi_dev_t dev, uint32_t cmd, void* arg)
{
    if (!dev || !dev->valid) {
        return KAPI_EINVAL;
    }

    return KAPI_OK;
}

int kapi_dev_get_info(kapi_dev_t dev, kapi_dev_info_t* info)
{
    if (!dev || !dev->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    strncpy(info->name, dev->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->type = dev->type;
    info->status = dev->valid ? 1 : 0;

    return KAPI_OK;
}

int kapi_dev_count(void)
{
    return kapi_active_dev_count;
}

int kapi_dev_list(kapi_dev_info_t* infos, int count)
{
    if (!infos || count <= 0) {
        return 0;
    }

    /* 扫描中断表中注册的有效设备作为代理（实际系统应维护设备链表） */
    int filled = 0;
    for (int i = 0; i < KAPI_MAX_IRQ && filled < count; i++) {
        if (kapi_irq_table[i].valid) {
            snprintf(infos[filled].name, sizeof(infos[filled].name),
                     "irq%d", i);
            infos[filled].type = KAPI_DEV_UNKNOWN;
            infos[filled].status = 1;
            filled++;
        }
    }
    return filled;
}

int kapi_irq_register(int irq, void (*handler)(int, void*), void* arg)
{
    if (irq < 0 || irq >= KAPI_MAX_IRQ || !handler) {
        return KAPI_EINVAL;
    }

    kapi_irq_table[irq].handler = handler;
    kapi_irq_table[irq].arg = arg;
    kapi_irq_table[irq].valid = 1;

    return KAPI_OK;
}

int kapi_irq_unregister(int irq)
{
    if (irq < 0 || irq >= KAPI_MAX_IRQ) {
        return KAPI_EINVAL;
    }

    kapi_irq_table[irq].valid = 0;
    kapi_irq_table[irq].handler = NULL;

    return KAPI_OK;
}

int kapi_dma_submit(kapi_dma_desc_t* desc, int count)
{
    if (!desc || count <= 0) {
        return KAPI_EINVAL;
    }

    /* DMA 实现较为复杂，当前保留为未实现 */
    return KAPI_ENOSYS;
}

int kapi_dma_wait(uint64_t timeout_ms)
{
    /* DMA 实现较为复杂，当前保留为未实现 */
    return KAPI_ENOSYS;
}

uint64_t kapi_get_time_ms(void)
{
    if (tsc_frequency == 0) {
        return 0;
    }
    return (read_tsc() * 1000) / tsc_frequency;
}

uint64_t kapi_get_time_us(void)
{
    if (tsc_frequency == 0) {
        return 0;
    }
    return (read_tsc() * 1000000) / tsc_frequency;
}

int kapi_timer_set(uint64_t interval_ms, void (*callback)(void*), void* arg, int periodic)
{
    if (interval_ms == 0 || !callback) {
        return KAPI_EINVAL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MAX_TIMERS; i++) {
        if (!kapi_timer_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return KAPI_ERROR;
    }

    kapi_timer_table[slot].interval = interval_ms;
    kapi_timer_table[slot].last_trigger = kapi_get_time_ms();
    kapi_timer_table[slot].callback = callback;
    kapi_timer_table[slot].arg = arg;
    kapi_timer_table[slot].periodic = periodic;
    kapi_timer_table[slot].valid = 1;

    return slot;
}

int kapi_timer_cancel(int timer_id)
{
    if (timer_id < 0 || timer_id >= KAPI_MAX_TIMERS) {
        return KAPI_EINVAL;
    }

    kapi_timer_table[timer_id].valid = 0;
    return KAPI_OK;
}

void reboot_system(void)
{
    __asm__ volatile (
        "cli\n"
        "movb $0xFE, %al\n"
        "outb %al, $0x64\n"
        "1: hlt\n"
        "jmp 1b\n"
    );
}

void poweroff_system(void)
{
    __asm__ volatile (
        "cli\n"
        "movw $0x604, %dx\n"
        "movw $0x2000, %ax\n"
        "outw %ax, %dx\n"
        "1: hlt\n"
        "jmp 1b\n"
    );
}

void kapi_timer_poll(void)
{
    uint64_t now_jiffies = timer_get_jiffies();
    uint64_t now_ms = timer_jiffies_to_ms(now_jiffies);

    for (int i = 0; i < KAPI_MAX_TIMERS; i++) {
        if (!kapi_timer_table[i].valid) {
            continue;
        }

        if (now_ms - kapi_timer_table[i].last_trigger >= kapi_timer_table[i].interval) {
            kapi_timer_table[i].callback(kapi_timer_table[i].arg);
            kapi_timer_table[i].last_trigger = now_ms;

            if (!kapi_timer_table[i].periodic) {
                kapi_timer_table[i].valid = 0;
            }
        }
    }
}
