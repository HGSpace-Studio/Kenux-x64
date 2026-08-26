#ifndef KEGD_SERIAL_H
#define KEGD_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write(const char *text);
void serial_write_hex64(uint64_t value);

#endif
