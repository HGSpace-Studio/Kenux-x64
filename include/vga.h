#ifndef VGA_H
#define VGA_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#ifdef VGA_NATIVE
#include "vga_impl.h"
#else

void vga_clear(void);
void vga_setcolor(int fg, int bg);
void vga_print(const char* str);
void vga_print_char(int x, int y, char c);
int  vga_getc(char* c);
void vga_putc(char c);
int  kbhit(void);

#endif

#endif