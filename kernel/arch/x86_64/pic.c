#include <arch/pic.h>
#include <arch/io.h>

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

#define PIC_ICW1 0x11
#define PIC_ICW2 0x20
#define PIC_ICW3 0x04
#define PIC_ICW4 0x01

void pic_init(void)
{
    outb(PIC1_CMD, PIC_ICW1);
    outb(PIC2_CMD, PIC_ICW1);

    outb(PIC1_DATA, PIC_ICW2);
    outb(PIC2_DATA, PIC_ICW2 + 8);

    outb(PIC1_DATA, PIC_ICW3);
    outb(PIC2_DATA, PIC_ICW3);

    outb(PIC1_DATA, PIC_ICW4);
    outb(PIC2_DATA, PIC_ICW4);

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_enable(uint8_t irq)
{
    uint8_t mask;
    if (irq < 8) {
        mask = inb(PIC1_DATA) & ~(1 << irq);
        outb(PIC1_DATA, mask);
    } else {
        mask = inb(PIC1_DATA) & ~(1 << 2);
        outb(PIC1_DATA, mask);
        mask = inb(PIC2_DATA) & ~(1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

void pic_disable(uint8_t irq)
{
    uint8_t mask;
    if (irq < 8) {
        mask = inb(PIC1_DATA) | (1 << irq);
        outb(PIC1_DATA, mask);
    } else {
        mask = inb(PIC2_DATA) | (1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask_all(void)
{
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
}
