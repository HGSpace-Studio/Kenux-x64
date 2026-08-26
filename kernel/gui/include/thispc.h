#ifndef THISPC_H
#define THISPC_H

#include "types.h"
#include "window.h"

/* This PC application - shows drives, devices, and storage usage
 * in a file-explorer-like interface. */

window_t* thispc_create(void);
void thispc_on_show(void);

#endif /* THISPC_H */
