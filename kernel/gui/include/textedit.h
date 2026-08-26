#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include "types.h"
#include "window.h"

/* Text Editor application - a simple multi-line text editor with
 * line numbers, cursor positioning, auto-scroll, and a status bar
 * showing line/column/character counts. */

window_t* textedit_create(void);
void textedit_on_show(void);
void textedit_handle_key(uint16_t key_code, uint16_t key_char);

#endif /* TEXTEDIT_H */
