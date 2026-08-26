#include <arch/vga.h>
#include <arch/io.h>
#include <arch/keyboard.h>

/*
 * UEFI/GOP note:
 * Under UEFI firmware (QEMU OVMF), the legacy VGA text buffer at 0xB8000 is
 * NOT mapped. Writing to it triggers a page fault -> triple fault -> reset.
 * The bootloader set up GOP framebuffer mode, so we must NOT touch 0xB8000.
 *
 * Strategy: redirect all "vga" text output to COM1 serial.
 * The GUI later uses the GOP framebuffer directly via fb_info_t.
 */

#define COM1 0x3F8

static inline void serial_putc_raw(char c)
{
    /* Wait for transmit holding register empty */
    while ((inb(COM1 + 5) & 0x20) == 0) { }
    outb(COM1, (uint8_t)c);
}

static inline void serial_puts_raw(const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc_raw('\r');
        serial_putc_raw(*s++);
    }
}

/* No longer point at 0xB8000 — keep symbol for link compatibility,
 * but set to NULL so any accidental dereference faults loudly
 * instead of corrupting random memory. */
uint16_t* vga_buffer = (uint16_t*)0;

static uint8_t vga_row = 0;
static uint8_t vga_col = 0;
static uint8_t vga_color = 0x07;

void vga_init(void)
{
    serial_puts_raw("[VGA] init (serial redirect mode, GOP framebuffer)\n");
    vga_row = 0;
    vga_col = 0;
}

void vga_clear(void)
{
    serial_puts_raw("[VGA] clear\n");
    vga_row = 0;
    vga_col = 0;
}

void vga_putc(char c)
{
    if (c == '\n') {
        vga_row++;
        vga_col = 0;
        serial_putc_raw('\r');
        serial_putc_raw('\n');
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c >= 32) {
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_row++;
            vga_col = 0;
        }
        serial_putc_raw(c);
    }

    if (vga_row >= VGA_HEIGHT) {
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_print(const char* str)
{
    serial_puts_raw(str);
}

void vga_puts(const char* str)
{
    vga_print(str);
}

void vga_putchar(char c)
{
    vga_putc(c);
}

void vga_setcolor(uint8_t fg, uint8_t bg)
{
    vga_color = (bg << 4) | fg;
    (void)vga_color;
}

void vga_scroll(void)
{
    /* No-op in serial mode */
}

char vga_getchar(void)
{
    key_event_t ev = keyboard_read();
    return ev.ascii;
}

void vga_gets(char* buffer, int size)
{
    int i = 0;
    while (i < size - 1) {
        char c = vga_getchar();
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            break;
        } else if (c >= 32) {
            buffer[i++] = c;
            vga_putc(c);
        } else if (c == '\b' && i > 0) {
            i--;
            vga_putc('\b');
        }
    }
}
