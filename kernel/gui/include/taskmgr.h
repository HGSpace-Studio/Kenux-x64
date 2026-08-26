#ifndef TASKMGR_H
#define TASKMGR_H

#include "types.h"
#include "window.h"

/* Task Manager application - shows process list, CPU/memory usage
 * with live-updating dashboard cards and curve graphs. */

window_t* taskmgr_create(void);
void taskmgr_refresh(void);
void taskmgr_paint_content(void);
void taskmgr_update(void);

#endif /* TASKMGR_H */
