#ifndef BROWSER_H
#define BROWSER_H

#include "types.h"
#include "window.h"

/* Kenux Browser Demo application - a demo web browser UI with
 * navigation bar, address bar, bookmark bar, and preset local
 * pages (kenux:// URLs) rendered directly in the kernel. */

window_t* browser_create(void);
void browser_on_show(void);
void browser_navigate(const char* url);

#endif /* BROWSER_H */
