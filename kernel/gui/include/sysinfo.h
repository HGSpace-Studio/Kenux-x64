#ifndef SYSINFO_H
#define SYSINFO_H

#include "types.h"
#include "window.h"

/* System Information application - shows OS version, CPU model,
 * memory info, display resolution, and kernel features. */

window_t* sysinfo_create(void);
void sysinfo_on_show(void);

#endif /* SYSINFO_H */
