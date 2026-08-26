#ifndef KAPI_INOTIFY_H
#define KAPI_INOTIFY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IN_ACCESS        0x00000001
#define KAPI_IN_MODIFY        0x00000002
#define KAPI_IN_ATTRIB        0x00000004
#define KAPI_IN_CLOSE_WRITE   0x00000008
#define KAPI_IN_CLOSE_NOWRITE 0x00000010
#define KAPI_IN_OPEN          0x00000020
#define KAPI_IN_MOVED_FROM    0x00000040
#define KAPI_IN_MOVED_TO      0x00000080
#define KAPI_IN_CREATE        0x00000100
#define KAPI_IN_DELETE        0x00000200
#define KAPI_IN_DELETE_SELF   0x00000400
#define KAPI_IN_MOVE_SELF     0x00000800

#define KAPI_IN_UNMOUNT       0x00002000
#define KAPI_IN_Q_OVERFLOW    0x00004000
#define KAPI_IN_IGNORED       0x00008000

#define KAPI_IN_ISDIR         0x40000000
#define KAPI_IN_ONESHot       0x80000000

#define KAPI_IN_ALL_EVENTS    (KAPI_IN_ACCESS | KAPI_IN_MODIFY | KAPI_IN_ATTRIB | \
                               KAPI_IN_CLOSE_WRITE | KAPI_IN_CLOSE_NOWRITE | \
                               KAPI_IN_OPEN | KAPI_IN_MOVED_FROM | KAPI_IN_MOVED_TO | \
                               KAPI_IN_CREATE | KAPI_IN_DELETE | \
                               KAPI_IN_DELETE_SELF | KAPI_IN_MOVE_SELF)

#define KAPI_IN_CLOEXEC       0x080000
#define KAPI_IN_NONBLOCK      0x0800

#define KAPI_INOTIFY_MAX_INSTANCES  8
#define KAPI_INOTIFY_MAX_WATCHES    512
#define KAPI_INOTIFY_MAX_EVENTS     256

typedef struct {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char     name[];
} kapi_inotify_event_t;

int  kapi_inotify_init1(int flags);
int  kapi_inotify_init(void);
int  kapi_inotify_add_watch(int fd, const char* pathname, uint32_t mask);
int  kapi_inotify_rm_watch(int fd, int wd);
int  kapi_inotify_init_module(void);

#ifdef __cplusplus
}
#endif

#endif