#include <stdarg.h>
#include <stddef.h>

int vsprintf(char* str, const char* format, va_list args)
{
    int count = 0;

    while (*format) {
        if (*format == '%') {
            format++;

            if (*format == 's') {
                const char* s = va_arg(args, const char*);
                while (*s) {
                    str[count++] = *s++;
                }
            } else if (*format == 'd' || *format == 'i') {
                int n = va_arg(args, int);
                if (n < 0) {
                    str[count++] = '-';
                    n = -n;
                }

                char buffer[12];
                int i = 0;
                do {
                    buffer[i++] = '0' + (n % 10);
                    n /= 10;
                } while (n > 0);

                while (i > 0) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'x' || *format == 'X') {
                unsigned int n = va_arg(args, unsigned int);
                char buffer[16];
                int i = 0;
                do {
                    char digit = n % 16;
                    buffer[i++] = (digit < 10) ? ('0' + digit) : ((format[i] == 'X') ? ('A' + digit - 10) : ('a' + digit - 10));
                    n /= 16;
                } while (n > 0);

                while (i > 0) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'c') {
                str[count++] = (char)va_arg(args, int);
            } else if (*format == '%') {
                str[count++] = '%';
            }

            format++;
        } else {
            str[count++] = *format++;
        }
    }

    str[count] = '\0';
    return count;
}

int vsnprintf(char* str, size_t size, const char* format, va_list args)
{
    (void)size;
    return vsprintf(str, format, args);
}
