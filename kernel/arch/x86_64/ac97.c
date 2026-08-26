#include <arch/ac97.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>

static ac97_controller_t ac97_controllers[AC97_MAX_CONTROLLERS];
static uint8_t ac97_controller_count = 0;

/* AC97 Mixer/Codec 寄存器偏移 */
#define AC97_RESET              0x00
#define AC97_MASTER_VOLUME      0x02
#define AC97_PCM_OUT_VOLUME     0x18
#define AC97_RECORD_SELECT      0x1A
#define AC97_RECORD_GAIN        0x1C
#define AC97_POWERDOWN_CTRL     0x26
#define AC97_EXT_AUDIO_ID       0x28
#define AC97_EXT_AUDIO_CTRL     0x2A
#define AC97_PCM_FRONT_DAC_RATE 0x2C
#define AC97_PCM_LR_ADC_RATE    0x32
#define AC97_VENDOR_ID1         0x7C
#define AC97_VENDOR_ID2         0x7E

/* Bus Master (DMA) 寄存器偏移 */
#define AC97_BM_PCM_OUT_CTRL    0x00
#define AC97_BM_PCM_OUT_STATUS  0x04
#define AC97_BM_PCM_OUT_BDBA    0x08  /* Buffer Descriptor Base Address */
#define AC97_BM_PCM_OUT_CIV     0x0C  /* Current Index Value */
#define AC97_BM_PCM_OUT_LVI     0x10  /* Last Valid Index */

#define AC97_BM_PCM_IN_CTRL     0x20
#define AC97_BM_PCM_IN_STATUS   0x24
#define AC97_BM_PCM_IN_BDBA     0x28
#define AC97_BM_PCM_IN_LVI      0x30

/* 缓冲区描述符列表条目 */
typedef struct {
    uint32_t phys_addr;
    uint16_t sample_count;
    uint16_t flags;
} __attribute__((packed)) ac97_bdl_entry_t;

static ac97_bdl_entry_t ac97_bdl[2] __attribute__((aligned(16)));

void ac97_init(void)
{
    memset(ac97_controllers, 0, sizeof(ac97_controllers));
    ac97_controller_count = 0;

    uint8_t bus, device, function;
    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t class_code_reg = pci_read_config(b, d, f, 0x08);
                if (class_code_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint8_t class_code = (class_code_reg >> 24) & 0xFF;
                uint8_t subclass = (class_code_reg >> 16) & 0xFF;
                if (class_code == 0x04 && subclass == 0x01) {
                    if (ac97_controller_count < AC97_MAX_CONTROLLERS) {
                        uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                        cmd |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER;
                        pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                        ac97_controller_t* ctrl = &ac97_controllers[ac97_controller_count++];
                        ctrl->bus = b;
                        ctrl->device = d;
                        ctrl->function = f;
                        ctrl->base_address = pci_read_config(b, d, f, 0x10) & 0xFFFFFFF0;
                        ctrl->dma_base = pci_read_config(b, d, f, 0x14) & 0xFFFFFFF0;
                        ctrl->vendor_id = pci_read_config(b, d, f, 0x00) & 0xFFFF;
                        ctrl->device_id = (pci_read_config(b, d, f, 0x00) >> 16) & 0xFFFF;
                        ctrl->interrupt = pci_read_config(b, d, f, 0x3C) & 0xFF;

                        /* 检测并复位 Codec */
                        uint16_t vendor1, vendor2;
                        ac97_read_register(AC97_VENDOR_ID1, &vendor1);
                        ac97_read_register(AC97_VENDOR_ID2, &vendor2);
                        ctrl->vendor_id = vendor1;
                        ctrl->device_id = vendor2;

                        /* 复位 Codec */
                        ac97_write_register(AC97_RESET, 0x0000);
                        for (volatile int i = 0; i < 100000; i++);

                        /* 设置主音量为最大（0x0000），取消静音 */
                        ac97_write_register(AC97_MASTER_VOLUME, 0x0000);

                        /* 设置 PCM 输出音量 */
                        ac97_write_register(AC97_PCM_OUT_VOLUME, 0x0000);

                        /* 解除 Powerdown 中的模拟和数字部分 */
                        ac97_write_register(AC97_POWERDOWN_CTRL, 0x0000);
                    }
                }
            }
        }
    }
}

ac97_controller_t* ac97_get_controller(uint8_t index)
{
    if (index < ac97_controller_count) {
        return &ac97_controllers[index];
    }
    return NULL;
}

uint8_t ac97_get_controller_count(void)
{
    return ac97_controller_count;
}

int ac97_get_controller_info(uint8_t index, ac97_controller_t* info)
{
    if (index >= ac97_controller_count) {
        return -1;
    }

    memcpy(info, &ac97_controllers[index], sizeof(ac97_controller_t));
    return 0;
}

uint16_t ac97_get_vendor_id(uint8_t index)
{
    if (index >= ac97_controller_count) {
        return 0;
    }

    return ac97_controllers[index].vendor_id;
}

uint16_t ac97_get_device_id(uint8_t index)
{
    if (index >= ac97_controller_count) {
        return 0;
    }

    return ac97_controllers[index].device_id;
}

int ac97_read_register(uint16_t reg, uint16_t* value)
{
    if (!value) return -1;
    if (ac97_controller_count == 0) return -1;
    if (reg > 0x7E) return -1;

    ac97_controller_t* ctrl = &ac97_controllers[0];
    uint16_t base = (uint16_t)ctrl->base_address;
    *value = inw(base + reg);
    return 0;
}

int ac97_write_register(uint16_t reg, uint16_t value)
{
    if (ac97_controller_count == 0) return -1;
    if (reg > 0x7E) return -1;

    ac97_controller_t* ctrl = &ac97_controllers[0];
    uint16_t base = (uint16_t)ctrl->base_address;
    outw(base + reg, value);
    return 0;
}

int ac97_play(const uint8_t* data, uint32_t length)
{
    if (!data || length == 0) return -1;
    if (ac97_controller_count == 0) return -1;

    ac97_controller_t* ctrl = &ac97_controllers[0];
    uint16_t dma_base = (uint16_t)ctrl->dma_base;

    /* 设置 PCM Front DAC 采样率为 48000Hz */
    ac97_write_register(AC97_PCM_FRONT_DAC_RATE, 48000);

    /* 设置 PCM Out Volume 为 0dB（不静音） */
    ac97_write_register(AC97_PCM_OUT_VOLUME, 0x0000);

    /* 配置 DMA 缓冲区描述符列表 */
    memset(ac97_bdl, 0, sizeof(ac97_bdl));
    ac97_bdl[0].phys_addr = (uint32_t)(uint64_t)data;
    ac97_bdl[0].sample_count = (uint16_t)(length / 2);  /* 16-bit 样本 */
    ac97_bdl[0].flags = 0x8000;  /* Interrupt on completion */

    /* 写入 BDL 基地址 */
    outl(dma_base + AC97_BM_PCM_OUT_BDBA, (uint32_t)(uint64_t)ac97_bdl);

    /* 设置最后一个有效索引为 0 */
    outb(dma_base + AC97_BM_PCM_OUT_LVI, 0);

    /* 启动 PCM Out DMA */
    outb(dma_base + AC97_BM_PCM_OUT_CTRL, 0x01);

    return 0;
}

int ac97_record(uint8_t* data, uint32_t* length)
{
    if (!data || !length || *length == 0) return -1;
    if (ac97_controller_count == 0) return -1;

    ac97_controller_t* ctrl = &ac97_controllers[0];
    uint16_t dma_base = (uint16_t)ctrl->dma_base;

    /* 设置 PCM LR ADC 采样率为 48000Hz */
    ac97_write_register(AC97_PCM_LR_ADC_RATE, 48000);

    /* 配置录音源为麦克风，增益设为 0dB */
    ac97_write_register(AC97_RECORD_SELECT, 0x0000);
    ac97_write_register(AC97_RECORD_GAIN, 0x0000);

    /* 配置 DMA 缓冲区描述符列表 */
    memset(ac97_bdl, 0, sizeof(ac97_bdl));
    ac97_bdl[0].phys_addr = (uint32_t)(uint64_t)data;
    ac97_bdl[0].sample_count = (uint16_t)(*length / 2);
    ac97_bdl[0].flags = 0x8000;

    /* 写入 BDL 基地址 */
    outl(dma_base + AC97_BM_PCM_IN_BDBA, (uint32_t)(uint64_t)ac97_bdl);

    /* 设置最后一个有效索引为 0 */
    outb(dma_base + AC97_BM_PCM_IN_LVI, 0);

    /* 启动 PCM In DMA */
    outb(dma_base + AC97_BM_PCM_IN_CTRL, 0x01);

    return 0;
}
