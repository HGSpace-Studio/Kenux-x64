#include <arch/hda.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <string.h>

#define HDA_MAX_CONTROLLERS 4
#define HDA_RIRB_RESPONSE_INVALID  0xFFFFFFFF

static hda_t hda_devices[HDA_MAX_CONTROLLERS];
static uint8_t hda_count = 0;

static inline uint32_t hda_read32(hda_t* dev, uint32_t reg)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    return *ptr;
}

static inline void hda_write32(hda_t* dev, uint32_t reg, uint32_t val)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    *ptr = val;
}

static inline uint16_t hda_read16(hda_t* dev, uint32_t reg)
{
    volatile uint16_t* ptr = (volatile uint16_t*)(dev->mmio_base + reg);
    return *ptr;
}

static inline void hda_write16(hda_t* dev, uint32_t reg, uint16_t val)
{
    volatile uint16_t* ptr = (volatile uint16_t*)(dev->mmio_base + reg);
    *ptr = val;
}

static int hda_match_device(uint16_t vendor, uint16_t device)
{
    if (vendor == HDA_VENDOR_ID_INTEL) {
        if (device == HDA_DEVICE_ID_HDA || device == HDA_DEVICE_ID_HDA2 ||
            device == HDA_DEVICE_ID_HDA3) return 1;
    }
    uint32_t class_code = 0x040300;
    (void)class_code;
    return 0;
}

int hda_reset(hda_t* dev)
{
    if (!dev) return -1;

    uint32_t gctl = hda_read32(dev, 0x08);
    if (gctl & 0x01) {
        hda_write32(dev, 0x08, gctl & ~0x01);
    }

    for (volatile int i = 0; i < 1000000; i++) {
        if ((hda_read32(dev, 0x08) & 0x01) == 0) break;
    }

    hda_write32(dev, 0x08, gctl | 0x01);

    for (volatile int i = 0; i < 1000000; i++) {
        if (hda_read32(dev, 0x08) & 0x01) break;
    }

    if ((hda_read32(dev, 0x08) & 0x01) == 0) return -2;

    return 0;
}

uint32_t hda_send_cmd(hda_t* dev, uint32_t codec, uint32_t nid, uint32_t verb, uint32_t param)
{
    if (!dev || !dev->corb || !dev->rirb) return HDA_RIRB_RESPONSE_INVALID;

    spinlock_acquire(&dev->lock);

    uint32_t cmd = (codec << 28) | (nid << 20) | (verb << 8) | (param & 0xFF);
    if (verb >= 0xF00) {
        cmd = (codec << 28) | (nid << 20) | ((verb >> 8) << 8) | (verb & 0xFF);
        cmd = (codec << 28) | (nid << 20) | verb;
    }

    uint32_t wp = dev->corb_wp;
    dev->corb[wp] = cmd;
    dev->corb_wp = (wp + 1) % HDA_CORB_ENTRIES;
    hda_write16(dev, HDA_CORBWP, (uint16_t)dev->corb_wp);

    for (volatile int i = 0; i < 100000; i++) {
        uint16_t rirbwp = hda_read16(dev, HDA_RIRBWP);
        if (rirbwp != dev->rirb_rp) {
            uint32_t rp = (dev->rirb_rp + 1) % HDA_RIRB_ENTRIES;
            uint32_t response = dev->rirb[rp * 2];
            dev->rirb_rp = rp;
            spinlock_release(&dev->lock);
            return response;
        }
    }

    spinlock_release(&dev->lock);
    return HDA_RIRB_RESPONSE_INVALID;
}

int hda_enum_codec(hda_t* dev, uint32_t codec_addr)
{
    if (!dev || codec_addr >= HDA_MAX_CODECS) return -1;

    hda_codec_t* codec = &dev->codecs[dev->codec_count];
    codec->addr = codec_addr;

    uint32_t resp = hda_send_cmd(dev, codec_addr, 0, HDA_VERB_GET_PARAM, HDA_PARAM_VENDOR_ID);
    codec->vendor_id = resp >> 16;
    codec->device_id = resp & 0xFFFF;

    resp = hda_send_cmd(dev, codec_addr, 0, HDA_VERB_GET_PARAM, HDA_PARAM_REVISION_ID);
    codec->revision = resp;

    resp = hda_send_cmd(dev, codec_addr, 0, HDA_VERB_GET_PARAM, HDA_PARAM_SUB_NODE_COUNT);
    uint32_t start_nid = (resp >> 16) & 0xFF;
    uint32_t node_count = resp & 0xFF;

    resp = hda_send_cmd(dev, codec_addr, 0, HDA_VERB_GET_PARAM, HDA_PARAM_FUNCTION_GROUP);
    codec->fg_type = resp & 0xFF;

    if (codec->fg_type != 0x01) return -2;

    codec->widget_count = 0;
    for (uint32_t nid = start_nid; nid < start_nid + node_count && codec->widget_count < HDA_MAX_WIDGETS; nid++) {
        hda_widget_t* w = &codec->widgets[codec->widget_count];
        w->codec_addr = codec_addr;
        w->node_id = nid;

        resp = hda_send_cmd(dev, codec_addr, nid, HDA_VERB_GET_PARAM, HDA_PARAM_AUDIO_WIDGET_CAP);
        w->widget_cap = resp;
        w->type = (resp >> 20) & 0xF;

        resp = hda_send_cmd(dev, codec_addr, nid, HDA_VERB_GET_PARAM, HDA_PARAM_PIN_CAP);
        w->pin_cap = resp;

        resp = hda_send_cmd(dev, codec_addr, nid, HDA_VERB_GET_PARAM, HDA_PARAM_AMP_CAP);
        w->amp_cap = resp;

        resp = hda_send_cmd(dev, codec_addr, nid, HDA_VERB_GET_PARAM, HDA_PARAM_CONN_LIST_LEN);
        w->conn_list_len = resp & 0xFF;

        resp = hda_send_cmd(dev, codec_addr, nid, HDA_VERB_GET_PARAM, HDA_PARAM_SUPPORTED_PCM_SIZE);
        w->pcm_sizes = resp;

        if (w->type == 0x00 && codec->default_dac == 0) {
            codec->default_dac = nid;
        } else if (w->type == 0x01 && codec->default_adc == 0) {
            codec->default_adc = nid;
        } else if (w->type == 0x04) {
            if ((w->pin_cap & 0x04) && codec->default_pin_out == 0) {
                codec->default_pin_out = nid;
            }
            if ((w->pin_cap & 0x08) && codec->default_pin_in == 0) {
                codec->default_pin_in = nid;
            }
        }

        codec->widget_count++;
    }

    dev->codec_count++;
    return 0;
}

void hda_init(void)
{
    memset(hda_devices, 0, sizeof(hda_devices));
    hda_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t id = pci_read_config(b, d, f, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (!hda_match_device(vendor, device)) continue;
                if (hda_count >= HDA_MAX_CONTROLLERS) return;

                hda_t* dev = &hda_devices[hda_count];
                dev->bus = (uint8_t)b;
                dev->device = (uint8_t)d;
                dev->function = (uint8_t)f;
                dev->vendor_id = vendor;
                dev->device_id = device;
                dev->irq = pci_read_config(b, d, f, 0x3C) & 0xFF;
                spin_init(&dev->lock);

                uint32_t bar = pci_read_config(b, d, f, 0x10);
                dev->mmio_base = (uint64_t)(bar & 0xFFFFFFF0);
                uint32_t bar_hi = pci_read_config(b, d, f, 0x14);
                dev->mmio_base |= ((uint64_t)bar_hi << 32);

                uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
                pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                if (hda_reset(dev) != 0) continue;

                dev->gcaps = hda_read32(dev, HDA_GCAP);
                dev->output_streams = (dev->gcaps >> 12) & 0xF;
                dev->input_streams = (dev->gcaps >> 8) & 0xF;
                dev->bdl_streams = (dev->gcaps >> 4) & 0xF;

                dev->corb = (uint32_t*)memory_alloc(HDA_CORB_ENTRIES * sizeof(uint32_t) + 128);
                dev->corb_phys = (uint64_t)(uintptr_t)dev->corb;
                dev->rirb = (uint32_t*)memory_alloc(HDA_RIRB_ENTRIES * 2 * sizeof(uint32_t) + 128);
                dev->rirb_phys = (uint64_t)(uintptr_t)dev->rirb;
                if (!dev->corb || !dev->rirb) continue;

                memset(dev->corb, 0, HDA_CORB_ENTRIES * sizeof(uint32_t));
                memset(dev->rirb, 0, HDA_RIRB_ENTRIES * 2 * sizeof(uint32_t));
                dev->corb_wp = 0;
                dev->rirb_rp = 0;

                hda_write32(dev, HDA_CORBLBASE, (uint32_t)dev->corb_phys);
                hda_write32(dev, HDA_CORBUBASE, (uint32_t)(dev->corb_phys >> 32));
                hda_write8(dev, HDA_CORBSIZE, HDA_CORBSIZE_256);
                hda_write16(dev, HDA_CORBWP, 0);
                hda_write16(dev, HDA_CORBRP, 0x8000);
                hda_write8(dev, HDA_CORBCTL, HDA_CORBCTL_RUN);

                hda_write32(dev, HDA_RIRBLBASE, (uint32_t)dev->rirb_phys);
                hda_write32(dev, HDA_RIRBUBASE, (uint32_t)(dev->rirb_phys >> 32));
                hda_write8(dev, HDA_RIRBSIZE, HDA_RIRBSIZE_256);
                hda_write16(dev, HDA_RIRBWP, 0x8000);
                hda_write8(dev, HDA_RIRBCTL, HDA_RIRBCTL_RUN | HDA_RIRBCTL_RINTCTL);

                for (int i = 0; i < HDA_STREAM_COUNT; i++) {
                    dev->stream_bufs[i] = memory_alloc(HDA_STREAM_BUF_SIZE);
                    dev->stream_phys[i] = (uint64_t)(uintptr_t)dev->stream_bufs[i];
                    dev->bdl[i] = (uint32_t*)memory_alloc(HDA_BDL_ENTRIES * 16);
                    dev->bdl_phys[i] = (uint64_t)(uintptr_t)dev->bdl[i];
                }

                hda_write32(dev, HDA_INTCTL, HDA_INTCTL_GIE | HDA_INTCTL_CIE);

                uint16_t statests = (uint16_t)hda_read32(dev, 0x0C);
                for (int i = 0; i < 15; i++) {
                    if (statests & (1 << i)) {
                        hda_enum_codec(dev, (uint32_t)i);
                    }
                }

                dev->sample_rate = 48000;
                dev->channels = 2;
                dev->bits = 16;
                dev->playing = 0;
                dev->initialized = 1;
                hda_count++;
            }
        }
    }
}

hda_t* hda_get_controller(uint8_t index)
{
    if (index >= hda_count) return NULL;
    return &hda_devices[index];
}

int hda_set_sample_rate(hda_t* dev, uint32_t rate)
{
    if (!dev || !dev->initialized) return -1;
    spinlock_acquire(&dev->lock);
    dev->sample_rate = rate;
    spinlock_release(&dev->lock);
    return 0;
}

int hda_set_format(hda_t* dev, uint8_t channels, uint8_t bits)
{
    if (!dev || !dev->initialized) return -1;
    if (channels < 1 || channels > 8) return -2;
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32) return -3;
    spinlock_acquire(&dev->lock);
    dev->channels = channels;
    dev->bits = bits;
    spinlock_release(&dev->lock);
    return 0;
}

int hda_play(hda_t* dev, const void* data, uint32_t size)
{
    if (!dev || !dev->initialized || !data || size == 0) return -1;

    spinlock_acquire(&dev->lock);

    if (dev->codec_count == 0) {
        spinlock_release(&dev->lock);
        return -2;
    }

    hda_codec_t* codec = &dev->codecs[0];
    if (codec->default_dac == 0) {
        spinlock_release(&dev->lock);
        return -3;
    }

    uint32_t copy_size = size;
    if (copy_size > HDA_STREAM_BUF_SIZE) copy_size = HDA_STREAM_BUF_SIZE;

    memcpy(dev->stream_bufs[0], data, copy_size);

    uint32_t fmt = 0;
    switch (dev->bits) {
    case 8:  fmt |= (0x00 << 4); break;
    case 16: fmt |= (0x01 << 4); break;
    case 24: fmt |= (0x02 << 4); break;
    case 32: fmt |= (0x03 << 4); break;
    }
    fmt |= (dev->channels - 1);
    fmt |= (dev->sample_rate << 8) & 0x3FFF00;

    hda_send_cmd(dev, codec->addr, codec->default_dac, HDA_VERB_SET_CONV_FMT, fmt);

    if (codec->default_pin_out) {
        hda_send_cmd(dev, codec->addr, codec->default_pin_out, HDA_VERB_SET_EAPD_BTLENABLE, 0x02);
        hda_send_cmd(dev, codec->addr, codec->default_pin_out, HDA_VERB_SET_PIN_WIDGET_CTRL, 0xC0);
    }

    uint32_t stream_offset = 0x80;
    hda_write32(dev, stream_offset + 0x00, 1);
    hda_write32(dev, stream_offset + 0x04, fmt);
    hda_write32(dev, stream_offset + 0x08, (uint32_t)dev->bdl_phys[0]);
    hda_write32(dev, stream_offset + 0x0C, (uint32_t)(dev->bdl_phys[0] >> 32));

    dev->bdl[0][0] = (uint32_t)dev->stream_phys[0];
    dev->bdl[0][1] = (uint32_t)(dev->stream_phys[0] >> 32);
    dev->bdl[0][2] = copy_size;
    dev->bdl[0][3] = 0;

    hda_write32(dev, stream_offset + 0x00, 0x80000001);

    dev->playing = 1;
    spinlock_release(&dev->lock);
    return (int)copy_size;
}

int hda_stop(hda_t* dev)
{
    if (!dev || !dev->initialized) return -1;
    spinlock_acquire(&dev->lock);
    uint32_t stream_offset = 0x80;
    hda_write32(dev, stream_offset + 0x00, 0);
    dev->playing = 0;
    spinlock_release(&dev->lock);
    return 0;
}

void hda_set_volume(hda_t* dev, uint32_t nid, int left, int right)
{
    if (!dev || !dev->initialized || dev->codec_count == 0) return;
    hda_codec_t* codec = &dev->codecs[0];
    uint32_t amp_cmd = HDA_VERB_SET_AMP_GAIN_MUTE;
    if (left >= 0) {
        hda_send_cmd(dev, codec->addr, nid, amp_cmd, (0x70 << 8) | ((uint32_t)left & 0x7F));
    }
    if (right >= 0) {
        hda_send_cmd(dev, codec->addr, nid, amp_cmd, (0x30 << 8) | ((uint32_t)right & 0x7F));
    }
}

void hda_irq_handler(void)
{
    for (uint8_t i = 0; i < hda_count; i++) {
        hda_t* dev = &hda_devices[i];
        uint32_t intsts = hda_read32(dev, HDA_INTSTS);
        if (intsts == 0) continue;
        hda_write32(dev, HDA_INTSTS, intsts);
    }
}

uint8_t hda_get_count(void)
{
    return hda_count;
}