/*
 * WDM Audio driver - freestanding kernel implementation.
 *
 * A fixed-size pool of up to 4 audio devices is maintained.  Each device
 * owns a private 16 KiB ring buffer used to stage PCM sample data for
 * playback and capture.  One built-in "HD Audio Codec" device (44.1 kHz,
 * 16-bit, stereo) is registered at init time.  Per-device volume is held
 * in a separate static table.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define AUDIO_MAX_DEVICES   4
#define AUDIO_BUFFER_SIZE   16384

/* ------------------------------------------------------------------ *
 * Device pool, per-device ring buffers, and volume table
 * ------------------------------------------------------------------ */
static AUDIO_DEVICE g_audio_pool[AUDIO_MAX_DEVICES];
static int          g_audio_count;

static uint8_t g_audio_buffer[AUDIO_MAX_DEVICES][AUDIO_BUFFER_SIZE];

/* Per-device volume (0-100). */
static uint32_t g_volume[AUDIO_MAX_DEVICES] = {50, 50, 50, 50};

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/* A slot is in use once its buffer_size has been set on registration. */
static AUDIO_DEVICE* audio_lookup(uint32_t device_id) {
    if (device_id >= (uint32_t)AUDIO_MAX_DEVICES) return NULL;
    if (g_audio_pool[device_id].buffer_size == 0) return NULL;
    return &g_audio_pool[device_id];
}

/* ------------------------------------------------------------------ *
 * Public API
 * ------------------------------------------------------------------ */

int audio_driver_init(void) {
    AUDIO_FORMAT fmt;

    memset(g_audio_pool, 0, sizeof(g_audio_pool));
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));
    g_audio_count = 0;
    g_volume[0] = 50; g_volume[1] = 50; g_volume[2] = 50; g_volume[3] = 50;

    /* Built-in HD Audio Codec: 44100 Hz, 16-bit, stereo. */
    fmt.sample_rate     = 44100;
    fmt.bits_per_sample = 16;
    fmt.channels        = 2;
    if (audio_register_device("HD Audio Codec", &fmt) < 0) return -1;

    return 0;
}

int audio_register_device(const char* name, const AUDIO_FORMAT* format) {
    int slot;
    AUDIO_DEVICE* dev;
    size_t nlen;

    if (name == NULL || format == NULL) return -1;

    for (slot = 0; slot < AUDIO_MAX_DEVICES; slot++) {
        if (g_audio_pool[slot].buffer_size == 0) break;
    }
    if (slot >= AUDIO_MAX_DEVICES) return -1;

    dev = &g_audio_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    dev->device_id        = (uint32_t)slot;
    dev->format           = *format;
    dev->buffer_size      = AUDIO_BUFFER_SIZE;
    dev->buffer_position  = 0;
    dev->playing          = 0;
    dev->recording        = 0;
    dev->total_played     = 0;
    dev->total_recorded   = 0;

    memset(g_audio_buffer[slot], 0, AUDIO_BUFFER_SIZE);

    g_audio_count++;
    return slot;
}

int audio_write_buffer(uint32_t device_id, const void* data, uint32_t length) {
    AUDIO_DEVICE* dev;
    const uint8_t* src;
    uint32_t to_write;
    uint32_t i;

    if (data == NULL || length == 0) return 0;
    dev = audio_lookup(device_id);
    if (dev == NULL) return -1;

    /* Ring buffer: cap to the buffer capacity, wrapping around. */
    to_write = (length > dev->buffer_size) ? dev->buffer_size : length;

    src = (const uint8_t*)data;
    for (i = 0; i < to_write; i++) {
        g_audio_buffer[device_id][dev->buffer_position] = src[i];
        dev->buffer_position = (dev->buffer_position + 1) % dev->buffer_size;
    }

    dev->total_played += (uint64_t)to_write;
    return (int)to_write;
}

int audio_read_buffer(uint32_t device_id, void* buf, uint32_t length) {
    AUDIO_DEVICE* dev;
    uint8_t* dst;
    uint32_t to_read;
    uint32_t i;

    if (buf == NULL || length == 0) return 0;
    dev = audio_lookup(device_id);
    if (dev == NULL) return -1;

    to_read = (length > dev->buffer_size) ? dev->buffer_size : length;

    dst = (uint8_t*)buf;
    for (i = 0; i < to_read; i++) {
        dst[i] = g_audio_buffer[device_id][dev->buffer_position];
        dev->buffer_position = (dev->buffer_position + 1) % dev->buffer_size;
    }

    dev->total_recorded += (uint64_t)to_read;
    return (int)to_read;
}

int audio_start_playback(uint32_t device_id) {
    AUDIO_DEVICE* dev;
    dev = audio_lookup(device_id);
    if (dev == NULL) return -1;
    dev->playing = 1;
    return 0;
}

int audio_stop_playback(uint32_t device_id) {
    AUDIO_DEVICE* dev;
    dev = audio_lookup(device_id);
    if (dev == NULL) return -1;
    dev->playing = 0;
    return 0;
}

int audio_set_volume(uint32_t device_id, uint32_t volume) {
    if (device_id >= (uint32_t)AUDIO_MAX_DEVICES) return -1;
    if (volume > 100) volume = 100;
    g_volume[device_id] = volume;
    return 0;
}

int audio_get_device_count(void) {
    return g_audio_count;
}

AUDIO_DEVICE* audio_get_device(uint32_t device_id) {
    if (device_id >= (uint32_t)AUDIO_MAX_DEVICES) return NULL;
    if (g_audio_pool[device_id].buffer_size == 0) return NULL;
    return &g_audio_pool[device_id];
}
