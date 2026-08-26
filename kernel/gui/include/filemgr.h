#ifndef FILEMGR_H
#define FILEMGR_H

#include "types.h"
#include "window.h"

/* File Explorer application - provides a file manager with sidebar
 * navigation, address bar, and file/folder listing using the kernel
 * VFS API (with simulated fallback). */

window_t* filemgr_create(void);
void filemgr_refresh(void);
void filemgr_on_show(void);

#endif /* FILEMGR_H */
