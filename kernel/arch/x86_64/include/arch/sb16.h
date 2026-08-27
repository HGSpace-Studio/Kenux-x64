#ifndef ARCH_X86_64_SB16_H
#define ARCH_X86_64_SB16_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SB16_BASE_ADDR     0x220
#define SB16_MIXER_ADDR    0x224
#define SB16_MIXER_DATA    0x225
#define SB16_DSP_RESET     0x226
#define SB16_DSP_READ      0x22A
#define SB16_DSP_WRITE     0x22C
#define SB16_DSP_STATUS    0x22C
#define SB16_DSP_DATA_AVAIL 0x22E
#define SB16_DSP_ACK_8BIT  0x22E
#define SB16_DSP_ACK_16BIT 0x22F

#define SB16_CMD_RESET         0x01
#define SB16_CMD_DMA_8BIT_DAC  0x14
#define SB16_CMD_DMA_8BIT_ADC  0x24
#define SB16_CMD_DMA_16BIT_DAC 0xB4
#define SB16_CMD_DMA_16BIT_ADC 0xC4
#define SB16_CMD_SET_SAMPLE_RATE 0x41
#define SB16_CMD_SET_BLOCK_SIZE  0x48
#define SB16_CMD_PAUSE_8BIT    0x80
#define SB16_CMD_PAUSE_16BIT   0xD0
#define SB16_CMD_CONTINUE_8BIT 0xD4
#define SB16_CMD_CONTINUE_16BIT 0xD4
#define SB16_CMD_SPEAKER_ON    0xD1
#define SB16_CMD_SPEAKER_OFF   0xD3
#define SB16_CMD_GET_VERSION   0xE1
#define SB16_CMD_GET_IRQ       0xF2
#define SB16_CMD_GET_DMA8      0xF3
#define SB16_CMD_GET_DMA16     0xF8

#define SB16_MIXER_RESET       0x00
#define SB16_MIXER_MASTER_VOL  0x30
#define SB16_MIXER_MIDI_VOL    0x34
#define SB16_MIXER_CD_VOL      0x36
#define SB16_MIXER_VOICE_VOL   0x38
#define SB16_MIXER_MIC_VOL     0x3A
#define SB16_MIXER_PC_VOL      0x3C
#define SB16_MIXER_MASTER_L    0x30
#define SB16_MIXER_MASTER_R    0x31
#define SB16_MIXER_VOICE_L     0x32
#define SB16_MIXER_VOICE_R     0x33
#define SB16_MIXER_FM_L        0x34
#define SB16_MIXER_FM_R        0x35
#define SB16_MIXER_CD_L        0x36
#define SB16_MIXER_CD_R        0x37
#define SB16_MIXER_LINE_L      0x38
#define SB16_MIXER_LINE_R      0x39
#define SB16_MIXER_MIC         0x3A
#define SB16_MIXER_PC_SPK      0x3B
#define SB16_MIXER_OUTPUT_CTRL 0x3C
#define SB16_MIXER_INPUT_L     0x3D
#define SB16_MIXER_INPUT_R     0x3E
#define SB16_MIXER_INPUT_CTRL  0x3F
#define SB16_MIXER_IRQ         0x80
#define SB16_MIXER_DMA         0x81

#define SB16_IRQ_2   0x01
#define SB16_IRQ_5   0x02
#define SB16_IRQ_7   0x04
#define SB16_IRQ_10  0x08

#define SB16_DMA_0   0x01
#define SB16_DMA_1   0x02
#define SB16_DMA_3   0x08
#define SB16_DMA_5   0x20
#define SB16_DMA_6   0x40
#define SB16_DMA_7   0x80

#define SB16_SAMPLE_RATE_MIN  4000
#define SB16_SAMPLE_RATE_MAX  44100
#define SB16_BUFFER_SIZE      4096

typedef struct {
    uint16_t  base;
    uint8_t   irq;
    uint8_t   dma8;
    uint8_t   dma16;
    int       major_ver;
    int       minor_ver;
    uint32_t  sample_rate;
    uint8_t   channels;
    uint8_t   bits;
    int       playing;
    void*     dma_buffer;
    uint64_t  dma_buffer_phys;
    uint32_t  buffer_pos;
    uint32_t  buffer_size;
    spinlock_t lock;
    int       initialized;
} sb16_t;

void sb16_init(void);
sb16_t* sb16_get_device(void);
int sb16_reset(sb16_t* dev);
int sb16_set_sample_rate(sb16_t* dev, uint32_t rate);
int sb16_set_format(sb16_t* dev, uint8_t channels, uint8_t bits);
int sb16_play(sb16_t* dev, const void* data, uint32_t size);
int sb16_stop(sb16_t* dev);
int sb16_pause(sb16_t* dev);
int sb16_resume(sb16_t* dev);
void sb16_set_master_volume(sb16_t* dev, uint8_t left, uint8_t right);
void sb16_get_master_volume(sb16_t* dev, uint8_t* left, uint8_t* right);
void sb16_irq_handler(void);

#endif