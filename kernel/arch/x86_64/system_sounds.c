#include <arch/system_sounds.h>
#include <arch/pcspk.h>
#include <arch/types.h>

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
} sound_note_t;

static const sound_note_t startup_sound[] = {
    {1245, 44},
    {578, 90},
    {578, 42},
    {889, 44},
    {1045, 90},
    {1045, 86},
    {911, 44},
    {945, 44},
    {1034, 44},
    {1000, 44},
    {1056, 88},
    {1000, 44},
    {889, 90},
    {889, 90},
    {889, 84},
    {789, 44},
    {656, 90},
    {656, 86},
    {1034, 90},
    {1034, 90},
    {1034, 1},
};

static const sound_note_t shutdown_sound[] = {
    {0, 44},
    {1434, 44},
    {1567, 88},
    {1456, 44},
    {1345, 44},
    {1234, 44},
    {945, 44},
    {1034, 44},
    {967, 44},
    {800, 44},
    {922, 44},
    {789, 44},
    {856, 44},
    {745, 88},
    {767, 88},
    {700, 44},
    {667, 88},
    {611, 44},
    {567, 88},
    {533, 44},
    {600, 44},
    {622, 44},
    {667, 44},
    {645, 44},
    {622, 88},
    {678, 44},
    {567, 44},
    {611, 44},
    {556, 90},
    {556, 42},
    {0, 90},
    {0, 42},
};

static const sound_note_t error_sound[] = {
    {722, 44},
    {422, 88},
    {533, 44},
    {633, 88},
    {0, 90},
    {0, 90},
    {0, 30},
};

static void play_notes(const sound_note_t* notes, uint32_t count)
{
    if (!notes || count == 0) return;
    for (uint32_t i = 0; i < count; i++) {
        pcspk_play_tone(notes[i].frequency_hz, notes[i].duration_ms);
    }
    pcspk_stop();
}

static void short_busy_delay(uint32_t duration_ms)
{
    volatile uint32_t guard;
    while (duration_ms--) {
        for (guard = 0; guard < 90000; guard++) {
            __asm__ volatile ("pause");
        }
    }
}

static void play_notes_busy(const sound_note_t* notes, uint32_t count)
{
    if (!notes || count == 0) return;
    for (uint32_t i = 0; i < count; i++) {
        if (notes[i].frequency_hz) {
            pcspk_tone(notes[i].frequency_hz);
        } else {
            pcspk_stop();
        }
        short_busy_delay(notes[i].duration_ms);
        pcspk_stop();
    }
    pcspk_stop();
}

void system_sounds_init(void)
{
    pcspk_init();
}

void system_sound_play_startup(void)
{
    play_notes(startup_sound, 4);
}

void system_sound_play_shutdown(void)
{
    play_notes(shutdown_sound, (uint32_t)(sizeof(shutdown_sound) / sizeof(shutdown_sound[0])));
}

void system_sound_play_error(void)
{
    play_notes_busy(error_sound, (uint32_t)(sizeof(error_sound) / sizeof(error_sound[0])));
}
