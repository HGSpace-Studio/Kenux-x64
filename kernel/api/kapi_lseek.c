#include "kapi.h"
#include "kapi_fs_ext.h"

kapi_off_t kapi_lseek(int fd, kapi_off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return -1;
}