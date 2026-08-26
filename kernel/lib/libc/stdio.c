#include <arch/vga.h>
#include <stdarg.h>
#include <string.h>

static char print_buffer[1024];

int vsprintf(char* str, const char* format, va_list args);
int vsnprintf(char* str, size_t size, const char* format, va_list args);

int sprintf(char* str, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int result = vsprintf(str, format, args);

    va_end(args);

    return result;
}

int snprintf(char* str, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int result = vsnprintf(str, size, format, args);

    va_end(args);

    return result;
}

int printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    vsprintf(print_buffer, format, args);

    va_end(args);

    vga_print(print_buffer);

    return 0;
}

int puts(const char* str)
{
    vga_print(str);
    vga_print("\n");
    return 0;
}

int putchar(int c)
{
    vga_putc((char)c);
    return c;
}
