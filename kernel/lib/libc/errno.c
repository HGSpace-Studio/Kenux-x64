#include "errno.h"

#ifndef errno
int errno = 0;
#endif

int* __errno_location(void)
{
    return &errno;
}
