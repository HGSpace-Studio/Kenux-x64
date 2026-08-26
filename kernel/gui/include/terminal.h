#ifndef TERMINAL_H
#define TERMINAL_H

#include "types.h"
#include "window.h"

/* Terminal application - provides an interactive command-line shell
 * running inside a GUI window. Supports built-in commands that call
 * real kernel APIs for process, memory, CPU, and VFS information. */

window_t* terminal_create(void);
void terminal_on_show(void);
void terminal_handle_key(uint16_t key_code, uint16_t key_char);
void terminal_refresh(void);
void terminal_tick(void);

#endif /* TERMINAL_H */
