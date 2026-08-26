#ifndef ARCH_X86_64_PIC_H
#define ARCH_X86_64_PIC_H

#include <arch/types.h>

void pic_init(void);
void pic_enable(uint8_t irq);
void pic_disable(uint8_t irq);
void pic_eoi(uint8_t irq);
void pic_mask_all(void);
void pic_unmask_all(void);

#endif
