#ifndef ARCH_X86_64_HDA_H
#define ARCH_X86_64_HDA_H

#include <arch/types.h>
#include <arch/pci.h>
#include <arch/spinlock.h>

#define HDA_VENDOR_ID_INTEL  0x8086
#define HDA_DEVICE_ID_HDA    0x2668
#define HDA_DEVICE_ID_HDA2   0x27D8
#define HDA_DEVICE_ID_HDA3   0x3A3E

#define HDA_GCAP             0x00
#define HDA_VMIN             0x02
#define HDA_VMAJ             0x03
#define HDA_OUTPAY           0x04
#define HDA_INPAY            0x05
#define HDA_OUTSTR           0x06
#define HDA_INSTR            0x07
#define HDA_INTCTL           0x20
#define HDA_INTSTS           0x24
#define HDA_WALLCLK          0x30
#define HDA_SSYNC            0x38
#define HDA_CORBLBASE        0x40
#define HDA_CORBUBASE        0x44
#define HDA_CORBWP           0x48
#define HDA_CORBRP           0x4A
#define HDA_CORBCTL          0x4C
#define HDA_CORBSTS          0x4D
#define HDA_CORBSIZE         0x4E
#define HDA_RIRBLBASE        0x50
#define HDA_RIRBUBASE        0x54
#define HDA_RIRBWP           0x58
#define HDA_RIRBCTL          0x5C
#define HDA_RIRBSTS          0x5D
#define HDA_RIRBSIZE         0x5E
#define HDA_ICOI             0x60
#define HDA_ICIS             0x64
#define HDA_DPLBASE          0x70
#define HDA_DPUBASE          0x74

#define HDA_INTCTL_GIE       0x80000000
#define HDA_INTCTL_CIE       0x40000000

#define HDA_CORBCTL_RUN      0x02
#define HDA_CORBCTL_RESET    0x01

#define HDA_RIRBCTL_RUN      0x02
#define HDA_RIRBCTL_RESET    0x01
#define HDA_RIRBCTL_RINTCTL  0x04

#define HDA_CORBSIZE_8       0x00
#define HDA_CORBSIZE_16      0x10
#define HDA_CORBSIZE_64      0x20
#define HDA_CORBSIZE_256     0x30
#define HDA_CORBSIZE_1024    0x40
#define HDA_CORBSIZE_4096    0x50

#define HDA_RIRBSIZE_8       0x00
#define HDA_RIRBSIZE_16      0x10
#define HDA_RIRBSIZE_64      0x20
#define HDA_RIRBSIZE_256     0x30
#define HDA_RIRBSIZE_1024    0x40
#define HDA_RIRBSIZE_4096    0x50

#define HDA_VERB_GET_PARAM           0xF00
#define HDA_VERB_GET_CONN_LIST       0xF01
#define HDA_VERB_GET_PROC_STATE      0xF02
#define HDA_VERB_GET_CONV_FMT        0xA00
#define HDA_VERB_SET_CONV_FMT        0x200
#define HDA_VERB_GET_AMP_GAIN_MUTE   0xB00
#define HDA_VERB_SET_AMP_GAIN_MUTE   0x300
#define HDA_VERB_SET_EAPD_BTLENABLE  0x3C0
#define HDA_VERB_GET_PIN_SENSE       0xF09
#define HDA_VERB_SET_PIN_WIDGET_CTRL 0x307
#define HDA_VERB_SET_CHANNEL_COUNT   0x30D
#define HDA_VERB_GET_SUBSYSTEM_ID    0xF20
#define HDA_VERB_SET_POWER_STATE     0x305
#define HDA_VERB_GET_POWER_STATE     0xF05

#define HDA_PARAM_VENDOR_ID          0x00
#define HDA_PARAM_REVISION_ID        0x02
#define HDA_PARAM_SUB_NODE_COUNT     0x04
#define HDA_PARAM_FUNCTION_GROUP     0x05
#define HDA_PARAM_AUDIO_WIDGET_CAP   0x09
#define HDA_PARAM_SUPPORTED_PCM_SIZE 0x0A
#define HDA_PARAM_SUPPORTED_FMT      0x0B
#define HDA_PARAM_PIN_CAP            0x0C
#define HDA_PARAM_AMP_CAP            0x12
#define HDA_PARAM_CONN_LIST_LEN      0x0E
#define HDA_PARAM_PROC_WIDGET_CAP    0x10

#define HDA_MAX_CODECS      16
#define HDA_MAX_WIDGETS     64
#define HDA_CORB_ENTRIES    256
#define HDA_RIRB_ENTRIES    256
#define HDA_BDL_ENTRIES     256

#define HDA_STREAM_COUNT    8
#define HDA_STREAM_BUF_SIZE 4096

typedef struct {
    uint32_t codec_addr;
    uint32_t node_id;
    uint32_t widget_cap;
    uint32_t pin_cap;
    uint32_t amp_cap;
    uint32_t conn_list_len;
    uint32_t pcm_sizes;
    uint32_t fmt;
    uint32_t subsystem_id;
    int type;
} hda_widget_t;

typedef struct {
    uint32_t addr;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t revision;
    uint32_t fg_type;
    uint32_t widget_count;
    hda_widget_t widgets[HDA_MAX_WIDGETS];
    uint32_t default_dac;
    uint32_t default_adc;
    uint32_t default_pin_out;
    uint32_t default_pin_in;
} hda_codec_t;

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint64_t  mmio_base;
    uint8_t   irq;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint32_t  gcaps;
    uint8_t   output_streams;
    uint8_t   input_streams;
    uint8_t   bdl_streams;
    uint16_t  codec_count;
    hda_codec_t codecs[HDA_MAX_CODECS];
    uint32_t* corb;
    uint64_t  corb_phys;
    uint32_t  corb_wp;
    uint32_t* rirb;
    uint64_t  rirb_phys;
    uint32_t  rirb_rp;
    void*     stream_bufs[HDA_STREAM_COUNT];
    uint64_t  stream_phys[HDA_STREAM_COUNT];
    uint32_t* bdl[HDA_STREAM_COUNT];
    uint64_t  bdl_phys[HDA_STREAM_COUNT];
    uint32_t  sample_rate;
    uint8_t   channels;
    uint8_t   bits;
    int       playing;
    spinlock_t lock;
    int       initialized;
} hda_t;

void hda_init(void);
hda_t* hda_get_controller(uint8_t index);
int hda_reset(hda_t* dev);
uint32_t hda_send_cmd(hda_t* dev, uint32_t codec, uint32_t nid, uint32_t verb, uint32_t param);
int hda_enum_codec(hda_t* dev, uint32_t codec_addr);
int hda_set_sample_rate(hda_t* dev, uint32_t rate);
int hda_set_format(hda_t* dev, uint8_t channels, uint8_t bits);
int hda_play(hda_t* dev, const void* data, uint32_t size);
int hda_record(hda_t* dev, void* buffer, uint32_t* length);
int hda_stop(hda_t* dev);
void hda_set_volume(hda_t* dev, uint32_t nid, int left, int right);
void hda_irq_handler(void);
uint8_t hda_get_count(void);

#endif