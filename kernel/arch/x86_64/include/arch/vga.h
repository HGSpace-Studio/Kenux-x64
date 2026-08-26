#ifndef ARCH_X86_64_VGA_H
#define ARCH_X86_64_VGA_H

#include <arch/types.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

extern uint16_t* vga_buffer;

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* str);
void vga_puts(const char* str);
void vga_putchar(char c);
void vga_setcolor(uint8_t fg, uint8_t bg);
void vga_scroll(void);
char vga_getchar(void);
void vga_gets(char* buffer, int size);

#endif
