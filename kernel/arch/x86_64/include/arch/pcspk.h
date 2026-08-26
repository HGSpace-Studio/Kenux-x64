#ifndef ARCH_X86_64_PCSPK_H
#define ARCH_X86_64_PCSPK_H

#include <arch/types.h>

void pcspk_init(void);
void pcspk_stop(void);
void pcspk_tone(uint32_t frequency_hz);
void pcspk_play_tone(uint32_t frequency_hz, uint32_t duration_ms);

#endif
