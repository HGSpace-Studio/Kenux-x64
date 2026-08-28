#ifndef VGA_IMPL_H
#define VGA_IMPL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef VGA_NATIVE

#ifndef KAL_KERNEL

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int vga_fg = 7;
static int vga_bg = 0;
static struct termios orig_termios;

void vga_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void vga_setcolor(int fg, int bg) {
    vga_fg = fg & 0x0F;
    vga_bg = bg & 0x07;

    int color_code = 30 + (fg & 0x07);
    if (fg >= 8) color_code += 60;

    printf("\033[%d;%dm", color_code, 40 + (bg & 0x07));
    fflush(stdout);
}

void vga_print(const char* str) {
    printf("%s", str);
    fflush(stdout);
}

void vga_print_char(int x, int y, char c) {
    printf("\033[%d;%dH%c", y+1, x*2+1, c);
    fflush(stdout);
}

int vga_getc(char* c) {
    return read(STDIN_FILENO, c, 1);
}

void vga_putc(char c) {
    putchar(c);
    fflush(stdout);
}

int kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void vga_init(void) {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    system("stty -echo");
    printf("\033[?25l");
    fflush(stdout);
}

void vga_cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    system("stty echo");
    printf("\033[?25h");
    printf("\033[0m");
    fflush(stdout);
}

#else

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int vga_fg = 7;
static int vga_bg = 0;

void vga_clear(void) {}
void vga_setcolor(int fg, int bg) { vga_fg = fg; vga_bg = bg; }
void vga_print(const char* str) {}
void vga_print_char(int x, int y, char c) {}
int vga_getc(char* c) { return -1; }
void vga_putc(char c) {}
int kbhit(void) { return 0; }
void vga_init(void) {}
void vga_cleanup(void) {}

#endif

#else

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int vga_fg = 7;
static int vga_bg = 0;

void vga_clear(void) {}
void vga_setcolor(int fg, int bg) { vga_fg = fg; vga_bg = bg; }
void vga_print(const char* str) {}
void vga_print_char(int x, int y, char c) {}
int vga_getc(char* c) { return -1; }
void vga_putc(char c) {}
int kbhit(void) { return 0; }
void vga_init(void) {}
void vga_cleanup(void) {}

#endif

#endif