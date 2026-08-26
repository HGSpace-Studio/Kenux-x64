

#include "kapi_syscall.h"
#include "kapi.h"
#include "kapi_process.h"
#include "kapi_memory.h"
#include "kapi_fs.h"
#include "kapi_device.h"
#include "kapi_epoll.h"
#include "kapi_poll.h"
#include "kapi_signalfd.h"
#include "kapi_timerfd.h"
#include "kapi_eventfd.h"
#include "kapi_inotify.h"

#include <arch/process.h>
#include <arch/ipc.h>
#include <arch/fs.h>
#include <arch/memory.h>
#include <arch/drivers.h>
#include <arch/elf.h>
#include <arch/mmap.h>
#include <arch/net.h>
#include <arch/pagecache.h>
#include <string.h>

#include <fifo.h>
#include <signal.h>
#include <module.h>
#include <unixsock.h>
#include <ipc.h>

#define KENUX_EPERM           1
#define KENUX_ENOENT          2
#define KENUX_ESRCH           3
#define KENUX_EINTR           4
#define KENUX_EIO             5
#define KENUX_ENXIO           6
#define KENUX_E2BIG           7
#define KENUX_ENOEXEC         8
#define KENUX_EBADF           9
#define KENUX_ECHILD          10
#define KENUX_EAGAIN          11
#define KENUX_ENOMEM          12
#define KENUX_EACCES          13
#define KENUX_EFAULT          14
#define KENUX_ENOTBLK         15
#define KENUX_EBUSY           16
#define KENUX_EEXIST          17
#define KENUX_EXDEV           18
#define KENUX_ENODEV          19
#define KENUX_ENOTDIR         20
#define KENUX_EISDIR          21
#define KENUX_EINVAL          22
#define KENUX_ENFILE          23
#define KENUX_EMFILE          24
#define KENUX_ENOTTY          25
#define KENUX_ETXTBSY         26
#define KENUX_EFBIG           27
#define KENUX_ENOSPC          28
#define KENUX_ESPIPE          29
#define KENUX_EROFS           30
#define KENUX_EMLINK          31
#define KENUX_EPIPE           32
#define KENUX_EDOM            33
#define KENUX_ERANGE          34
#define KENUX_ENOSYS          38

#define KENUX_ERR(err) (-(err))

#define KENUX_VERSION_STRING  "26.7.9K"

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern uint64_t process_count;
extern vfs_node_t* vfs_root;

#define KAPI_FD_MAX 256

typedef enum {
    KAPI_FD_NONE = 0,
    KAPI_FD_FILE,
    KAPI_FD_PIPE,
    KAPI_FD_SOCKET,
    KAPI_FD_DIR,
} kapi_fd_type_t;

typedef struct {
    kapi_fd_type_t type;
    union {
        kapi_file_t file;
        fifo_pipe_t* pipe;
        void* socket;
        kapi_dir_t dir;
    } obj;
    int flags;
    int cloexec;
} kapi_fd_entry_t;

static kapi_fd_entry_t proc_fd_table[PROCESS_MAX][KAPI_FD_MAX];
static uid_t proc_uid[PROCESS_MAX];
static uid_t proc_euid[PROCESS_MAX];
static gid_t proc_gid[PROCESS_MAX];
static gid_t proc_egid[PROCESS_MAX];
static mode_t proc_umask[PROCESS_MAX];
static uint64_t proc_brk[PROCESS_MAX];
static mmap_context_t proc_mmap_ctx[PROCESS_MAX];
static int proc_mmap_initialized[PROCESS_MAX];

static char kenux_hostname[64] = "kenux";

static uint64_t loadavg_last_jiffies = 0;
static long loadavg_1min = 0;
static long loadavg_5min = 0;
static long loadavg_15min = 0;

static kapi_syscall_fn_t syscall_table[KAPI_SYSCALL_COUNT];
static const char* syscall_names[KAPI_SYSCALL_COUNT];

static int kapi_fd_alloc(int proc_idx)
{
    for (int i = 0; i < KAPI_FD_MAX; i++) {
        if (proc_fd_table[proc_idx][i].type == KAPI_FD_NONE) {
            return i;
        }
    }
    return -1;
}

static int kapi_fd_check(int proc_idx, long fd)
{
    if (fd < 0 || fd >= KAPI_FD_MAX) return -1;
    if (proc_fd_table[proc_idx][fd].type == KAPI_FD_NONE) return -1;
    return 0;
}

static vfs_node_t* syscall_vfs_find_path(const char* path)
{
    if (!path || !vfs_root) {
        return NULL;
    }
    if (path[0] != '/') {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return vfs_root;
    }

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    vfs_node_t* node = vfs_root;
    char* p = temp + 1;

    while (*p) {
        char* token = p;
        while (*p && *p != '/') {
            p++;
        }
        if (*p == '/') {
            *p = '\0';
            p++;
        }
        if (!node->finddir) {
            return NULL;
        }
        node = node->finddir(node, token);
        if (!node) {
            return NULL;
        }
        while (*p == '/') p++;
    }
    return node;
}

static long sys_nosys(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_read_impl(long fd, long buf, long count, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);

    if (fd == 0) {
        return 0;
    }
    if (fd == 1 || fd == 2) {
        return KENUX_ERR(KENUX_EBADF);
    }

    if (kapi_fd_check((int)current_process, fd) < 0) {
        return KENUX_ERR(KENUX_EBADF);
    }

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    int64_t ret = -1;

    switch (entry->type) {
        case KAPI_FD_FILE:
            ret = kapi_read(entry->obj.file, (void*)buf, (size_t)count);
            break;
        case KAPI_FD_PIPE:
            if (entry->flags != KAPI_O_RDONLY && entry->flags != KAPI_O_RDWR) {
                return KENUX_ERR(KENUX_EBADF);
            }
            ret = fifo_read(entry->obj.pipe, (void*)buf, (uint64_t)count);
            break;
        case KAPI_FD_SOCKET:
            return KENUX_ERR(KENUX_EIO);
        default:
            return KENUX_ERR(KENUX_EBADF);
    }

    return ret < 0 ? KENUX_ERR(KENUX_EIO) : ret;
}

static long sys_write_impl(long fd, long buf, long count, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);

    if (fd == 1 || fd == 2) {
        extern int vga_print(const char*);
        char tmp[256];
        size_t to_write = (size_t)count > 255 ? 255 : (size_t)count;
        memcpy(tmp, (void*)buf, to_write);
        tmp[to_write] = '\0';
        vga_print(tmp);
        return (long)to_write;
    }

    if (kapi_fd_check((int)current_process, fd) < 0) {
        return KENUX_ERR(KENUX_EBADF);
    }

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    int64_t ret = -1;

    switch (entry->type) {
        case KAPI_FD_FILE:
            ret = kapi_write(entry->obj.file, (void*)buf, (size_t)count);
            break;
        case KAPI_FD_PIPE:
            if (entry->flags != KAPI_O_WRONLY && entry->flags != KAPI_O_RDWR) {
                return KENUX_ERR(KENUX_EBADF);
            }
            ret = fifo_write(entry->obj.pipe, (void*)buf, (uint64_t)count);
            break;
        case KAPI_FD_SOCKET:
            return KENUX_ERR(KENUX_EIO);
        default:
            return KENUX_ERR(KENUX_EBADF);
    }

    return ret < 0 ? KENUX_ERR(KENUX_EIO) : ret;
}

static long sys_open_impl(long pathname, long flags, long mode, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);

    int kflags = 0;
    if (flags & 0x0001) kflags |= KAPI_O_RDONLY;
    if (flags & 0x0002) kflags |= KAPI_O_WRONLY;
    if (flags & 0x0004) kflags |= KAPI_O_RDWR;
    if (flags & 0x0040) kflags |= KAPI_O_CREAT;
    if (flags & 0x0200) kflags |= KAPI_O_TRUNC;
    if (flags & 0x0400) kflags |= KAPI_O_APPEND;

    /* 处理 O_DIRECTORY */
    if (flags & 0x1000) {
        kapi_dir_t d = kapi_opendir((const char*)pathname);
        if (!d) return KENUX_ERR(KENUX_ENOENT);
        int fd = kapi_fd_alloc((int)current_process);
        if (fd < 0) {
            kapi_closedir(d);
            return KENUX_ERR(KENUX_EMFILE);
        }
        proc_fd_table[current_process][fd].type = KAPI_FD_DIR;
        proc_fd_table[current_process][fd].obj.dir = d;
        proc_fd_table[current_process][fd].flags = kflags;
        proc_fd_table[current_process][fd].cloexec = 0;
        return fd;
    }

    kapi_file_t f = kapi_open((const char*)pathname, kflags, (int)mode);
    if (!f) return KENUX_ERR(KENUX_ENOENT);

    int fd = kapi_fd_alloc((int)current_process);
    if (fd < 0) {
        kapi_close(f);
        return KENUX_ERR(KENUX_EMFILE);
    }

    proc_fd_table[current_process][fd].type = KAPI_FD_FILE;
    proc_fd_table[current_process][fd].obj.file = f;
    proc_fd_table[current_process][fd].flags = kflags;
    proc_fd_table[current_process][fd].cloexec = 0;
    return fd;
}

static long sys_close_impl(long fd, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (fd < 0 || fd >= KAPI_FD_MAX) return KENUX_ERR(KENUX_EBADF);
    if (proc_fd_table[current_process][fd].type == KAPI_FD_NONE) {
        if (fd <= 2) return 0;
        return KENUX_ERR(KENUX_EBADF);
    }

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    switch (entry->type) {
        case KAPI_FD_FILE:
            if (entry->obj.file) {
                kapi_close(entry->obj.file);
            }
            break;
        case KAPI_FD_PIPE:
            if (entry->flags == KAPI_O_RDONLY) {
                fifo_close_reader(entry->obj.pipe);
            } else if (entry->flags == KAPI_O_WRONLY) {
                fifo_close_writer(entry->obj.pipe);
            }
            break;
        case KAPI_FD_SOCKET:
            if (entry->obj.socket) {
                /* unix_socket_close((unix_sock_t*)entry->obj.socket); */
            }
            break;
        case KAPI_FD_DIR:
            kapi_closedir(entry->obj.dir);
            break;
        default:
            break;
    }

    entry->type = KAPI_FD_NONE;
    entry->obj.file = NULL;
    entry->flags = 0;
    entry->cloexec = 0;
    return 0;
}

static long sys_stat_impl(long pathname, long statbuf, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname || !statbuf) return KENUX_ERR(KENUX_EFAULT);

    kapi_file_stat_t kst;
    int ret = kapi_stat((const char*)pathname, &kst);
    if (ret != KAPI_OK) return KENUX_ERR(KENUX_ENOENT);

    struct { uint64_t st_dev, st_ino; uint32_t st_mode; } *st = (void*)statbuf;
    memset(st, 0, 128);
    st->st_mode = kst.mode;
    return 0;
}

static long sys_fstat_impl(long fd, long statbuf, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!statbuf) return KENUX_ERR(KENUX_EFAULT);

    if (kapi_fd_check((int)current_process, fd) < 0) {
        return KENUX_ERR(KENUX_EBADF);
    }

    memset((void*)statbuf, 0, 128);
    return 0;
}

static long sys_lseek_impl(long fd, long offset, long whence, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (fd <= 2) return KENUX_ERR(KENUX_ESPIPE);
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_ESPIPE);

    int64_t ret = kapi_seek(entry->obj.file, offset, (int)whence);
    return ret < 0 ? KENUX_ERR(KENUX_EBADF) : ret;
}

static long sys_mmap_impl(long addr, long length, long prot, long flags, long fd, long offset)
{
    (void)flags;

    if (!proc_mmap_initialized[current_process]) {
        mmap_context_init(&proc_mmap_ctx[current_process]);
        proc_mmap_initialized[current_process] = 1;
    }

    uint64_t ret = mmap_do_mmap(&proc_mmap_ctx[current_process], (uint64_t)addr,
                                (uint64_t)length, (uint32_t)prot, (uint32_t)flags,
                                (int)fd, (uint64_t)offset);
    if (ret == 0) return KENUX_ERR(KENUX_ENOMEM);
    return (long)ret;
}

static long sys_mprotect_impl(long addr, long len, long prot, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!proc_mmap_initialized[current_process]) return KENUX_ERR(KENUX_EINVAL);

    int ret = mmap_do_mprotect(&proc_mmap_ctx[current_process], (uint64_t)addr,
                               (uint64_t)len, (uint32_t)prot);
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    return 0;
}

static long sys_munmap_impl(long addr, long length, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!proc_mmap_initialized[current_process]) return 0;

    int ret = mmap_do_munmap(&proc_mmap_ctx[current_process], (uint64_t)addr,
                             (uint64_t)length);
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    return 0;
}

static long sys_brk_impl(long addr, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

    if (addr == 0) {
        return (long)proc_brk[current_process];
    }

    if (!proc_mmap_initialized[current_process]) {
        mmap_context_init(&proc_mmap_ctx[current_process]);
        proc_mmap_initialized[current_process] = 1;
    }

    uint64_t old_brk = proc_brk[current_process];
    uint64_t new_brk = (uint64_t)addr;

    if (new_brk > old_brk) {
        uint64_t len = new_brk - old_brk;
        uint64_t ret = mmap_do_mmap(&proc_mmap_ctx[current_process], old_brk, len,
                                    PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
        if (ret == 0) return (long)old_brk;
    } else if (new_brk < old_brk) {
        uint64_t len = old_brk - new_brk;
        mmap_do_munmap(&proc_mmap_ctx[current_process], new_brk, len);
    }

    proc_brk[current_process] = new_brk;
    return (long)new_brk;
}

static long sys_rt_sigaction_impl(long sig, long act, long oact, long sigsetsize,
                                   long a5, long a6)
{
    (void)sigsetsize;
    (void)a5; (void)a6;

    int ret = signal_sigaction((int)sig, (const k_sigaction_t*)act, (k_sigaction_t*)oact);
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    return 0;
}

static long sys_rt_sigprocmask_impl(long how, long set, long oset, long sigsetsize,
                                     long a5, long a6)
{
    (void)sigsetsize;
    (void)a5; (void)a6;

    int ret = signal_sigprocmask((int)how, (const k_sigset_t*)set, (k_sigset_t*)oset);
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    return 0;
}

static long sys_ioctl_impl(long fd, long request, long arg, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (fd <= 2) return KENUX_ERR(KENUX_ENOTTY);

    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_ENOTTY);

    kapi_dev_t dev = (kapi_dev_t)(uintptr_t)entry->obj.file;
    int ret = kapi_dev_ioctl(dev, (uint32_t)request, (void*)arg);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_EINVAL);
}

static long sys_access_impl(long pathname, long mode, long a3, long a4, long a5, long a6)
{
    (void)mode;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);

    kapi_file_stat_t st;
    int ret = kapi_stat((const char*)pathname, &st);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ENOENT);
}

static long sys_pipe_impl(long pipefd, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pipefd) return KENUX_ERR(KENUX_EFAULT);

    fifo_pipe_t* pipe = fifo_create();
    if (!pipe) return KENUX_ERR(KENUX_ENOMEM);

    int fd_r = kapi_fd_alloc((int)current_process);
    if (fd_r < 0) {
        fifo_destroy(pipe);
        return KENUX_ERR(KENUX_EMFILE);
    }
    int fd_w = kapi_fd_alloc((int)current_process);
    if (fd_w < 0) {
        proc_fd_table[current_process][fd_r].type = KAPI_FD_NONE;
        fifo_destroy(pipe);
        return KENUX_ERR(KENUX_EMFILE);
    }

    proc_fd_table[current_process][fd_r].type = KAPI_FD_PIPE;
    proc_fd_table[current_process][fd_r].obj.pipe = pipe;
    proc_fd_table[current_process][fd_r].flags = KAPI_O_RDONLY;
    proc_fd_table[current_process][fd_r].cloexec = 0;

    proc_fd_table[current_process][fd_w].type = KAPI_FD_PIPE;
    proc_fd_table[current_process][fd_w].obj.pipe = pipe;
    proc_fd_table[current_process][fd_w].flags = KAPI_O_WRONLY;
    proc_fd_table[current_process][fd_w].cloexec = 0;

    int* fds = (int*)pipefd;
    fds[0] = fd_r;
    fds[1] = fd_w;
    return 0;
}

static long sys_sched_yield_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    kapi_proc_yield();
    return 0;
}

static long sys_dup_impl(long oldfd, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, oldfd) < 0) return KENUX_ERR(KENUX_EBADF);

    int newfd = kapi_fd_alloc((int)current_process);
    if (newfd < 0) return KENUX_ERR(KENUX_EMFILE);

    proc_fd_table[current_process][newfd] = proc_fd_table[current_process][oldfd];
    return newfd;
}

static long sys_dup2_impl(long oldfd, long newfd, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (oldfd == newfd) return newfd;
    if (kapi_fd_check((int)current_process, oldfd) < 0) return KENUX_ERR(KENUX_EBADF);
    if (newfd < 0 || newfd >= KAPI_FD_MAX) return KENUX_ERR(KENUX_EBADF);

    if (proc_fd_table[current_process][newfd].type != KAPI_FD_NONE) {
        sys_close_impl(newfd, 0, 0, 0, 0, 0);
    }

    proc_fd_table[current_process][newfd] = proc_fd_table[current_process][oldfd];
    return newfd;
}

static long sys_nanosleep_impl(long req, long rem, long a3, long a4, long a5, long a6)
{
    (void)rem;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!req) return KENUX_ERR(KENUX_EFAULT);

    struct { long tv_sec; long tv_nsec; } *ts = (void*)req;
    uint64_t ms = (uint64_t)(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
    kapi_proc_sleep(ms);
    return 0;
}

static long sys_getpid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    kapi_proc_info_t info;
    if (kapi_proc_get_info(NULL, &info) == KAPI_OK) {
        return info.pid;
    }
    return 1;
}

static long sys_socket_impl(long domain, long type, long protocol, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;

    if (domain == AF_UNIX) {
        unix_sock_t* sock = unix_socket_create((uint32_t)type);
        if (!sock) return KENUX_ERR(KENUX_ENOMEM);

        int fd = kapi_fd_alloc((int)current_process);
        if (fd < 0) {
            unix_socket_close(sock);
            return KENUX_ERR(KENUX_EMFILE);
        }

        proc_fd_table[current_process][fd].type = KAPI_FD_SOCKET;
        proc_fd_table[current_process][fd].obj.socket = sock;
        proc_fd_table[current_process][fd].flags = 0;
        proc_fd_table[current_process][fd].cloexec = 0;
        return fd;
    } else if (domain == AF_INET) {
        int sfd = sys_socket((int)domain, (int)type, (int)protocol);
        if (sfd < 0) return KENUX_ERR(KENUX_EINVAL);

        int fd = kapi_fd_alloc((int)current_process);
        if (fd < 0) {
            sys_close_socket(sfd);
            return KENUX_ERR(KENUX_EMFILE);
        }

        proc_fd_table[current_process][fd].type = KAPI_FD_SOCKET;
        proc_fd_table[current_process][fd].obj.socket = (void*)(uintptr_t)sfd;
        proc_fd_table[current_process][fd].flags = 0;
        proc_fd_table[current_process][fd].cloexec = 0;
        return fd;
    }

    return KENUX_ERR(KENUX_EINVAL);
}

static long sys_clone_impl(long flags, long stack, long ptid, long ctid, long tls,
                            long a6)
{
    (void)stack; (void)ptid; (void)ctid; (void)tls; (void)a6;

    kapi_proc_t p = kapi_proc_create("clone", NULL, NULL, KAPI_PRIO_NORMAL);
    if (!p) return KENUX_ERR(KENUX_ENOMEM);

    int child_pid = kapi_proc_get_pid(p);
    if (child_pid >= 0 && child_pid < PROCESS_MAX) {
        for (int i = 0; i < KAPI_FD_MAX; i++) {
            proc_fd_table[child_pid][i] = proc_fd_table[current_process][i];
        }
        proc_brk[child_pid] = proc_brk[current_process];
        proc_uid[child_pid] = proc_uid[current_process];
        proc_euid[child_pid] = proc_euid[current_process];
        proc_gid[child_pid] = proc_gid[current_process];
        proc_egid[child_pid] = proc_egid[current_process];
        proc_umask[child_pid] = proc_umask[current_process];

        if (flags & 0x00000100) { /* CLONE_VM */
            proc_mmap_ctx[child_pid] = proc_mmap_ctx[current_process];
            proc_mmap_initialized[child_pid] = proc_mmap_initialized[current_process];
        } else {
            mmap_context_init(&proc_mmap_ctx[child_pid]);
            proc_mmap_initialized[child_pid] = 1;
        }
    }
    return child_pid;
}

static long sys_fork_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

    kapi_proc_t p = kapi_proc_create("fork", NULL, NULL, KAPI_PRIO_NORMAL);
    if (!p) return KENUX_ERR(KENUX_ENOMEM);

    int child_pid = kapi_proc_get_pid(p);
    if (child_pid >= 0 && child_pid < PROCESS_MAX) {
        for (int i = 0; i < KAPI_FD_MAX; i++) {
            proc_fd_table[child_pid][i] = proc_fd_table[current_process][i];
        }
        proc_brk[child_pid] = proc_brk[current_process];
        proc_uid[child_pid] = proc_uid[current_process];
        proc_euid[child_pid] = proc_euid[current_process];
        proc_gid[child_pid] = proc_gid[current_process];
        proc_egid[child_pid] = proc_egid[current_process];
        proc_umask[child_pid] = proc_umask[current_process];

        mmap_context_init(&proc_mmap_ctx[child_pid]);
        proc_mmap_initialized[child_pid] = 1;

        /* 尝试设置子进程返回值为 0 */
        for (uint64_t i = 0; i < process_count; i++) {
            if ((int)processes[i].id == child_pid) {
                processes[i].context.rax = 0;
                break;
            }
        }
    }
    return child_pid;
}

static long sys_vfork_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_fork_impl(0, 0, 0, 0, 0, 0);
}

static long sys_execve_impl(long filename, long argv, long envp, long a4, long a5, long a6)
{
    (void)argv; (void)envp;
    (void)a4; (void)a5; (void)a6;
    if (!filename) return KENUX_ERR(KENUX_EFAULT);

    elf_load_info_t info;
    memset(&info, 0, sizeof(info));

    int ret = elf_load_from_file((const char*)filename, &info);
    if (ret < 0) return KENUX_ERR(KENUX_ENOEXEC);

    /* 设置用户栈并跳转到 ELF 入口 */
    if (current_process < PROCESS_MAX) {
        const char* dummy_argv[2] = { (const char*)filename, NULL };
        const char* dummy_envp[1] = { NULL };
        uint64_t new_rsp = elf_setup_stack(info.stack_top, dummy_argv, dummy_envp, &info);
        processes[current_process].context.rip = info.entry;
        processes[current_process].context.rsp = new_rsp;
    }

    return 0;
}

static long sys_exit_impl(long error_code, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    kapi_proc_exit((int)error_code);
    return 0;
}

static long sys_wait4_impl(long pid, long wstatus, long options, long rusage,
                            long a5, long a6)
{
    (void)options; (void)rusage;
    (void)a5; (void)a6;
    int status = 0;
    int ret = kapi_proc_wait((int)pid, &status);
    if (wstatus) {
        *(int*)wstatus = status;
    }
    return ret;
}

static long sys_kill_impl(long pid, long sig, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;

    if (sig == SIGKILL) {
        int ret = kapi_proc_kill_by_pid((int)pid, 1);
        return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ESRCH);
    }

    int ret = signal_send((uint64_t)pid, (int)sig, NULL);
    if (ret < 0) return KENUX_ERR(KENUX_ESRCH);
    return 0;
}

static long sys_uname_impl(long buf, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);

    struct {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
        char domainname[65];
    } *u = (void*)buf;

    memset(u, 0, sizeof(*u));
    strcpy(u->sysname, "Kenux");
    strcpy(u->nodename, kenux_hostname);
    strcpy(u->release, KENUX_VERSION_STRING);
    strcpy(u->version, "Kenux Kernel " KENUX_VERSION_STRING);
    strcpy(u->machine, "x86_64");
    strcpy(u->domainname, "(none)");
    return 0;
}

static long sys_fcntl_impl(long fd, long cmd, long arg, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];

    switch (cmd) {
        case 0:   /* F_DUPFD */
            return sys_dup_impl(fd, 0, 0, 0, 0, 0);
        case 1:   /* F_GETFD */
            return entry->cloexec;
        case 2:   /* F_SETFD */
            entry->cloexec = (int)arg;
            return 0;
        case 3:   /* F_GETFL */
            return entry->flags;
        case 4:   /* F_SETFL */
            entry->flags = (int)arg;
            return 0;
        default:
            return KENUX_ERR(KENUX_EINVAL);
    }
}

static long sys_getcwd_impl(long buf, long size, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf || size == 0) return KENUX_ERR(KENUX_EFAULT);

    char* ret = kapi_getcwd((char*)buf, (size_t)size);
    return ret ? (long)strlen(ret) : KENUX_ERR(KENUX_ERANGE);
}

static long sys_chdir_impl(long path, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!path) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_chdir((const char*)path);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ENOENT);
}

static long sys_rename_impl(long oldpath, long newpath, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!oldpath || !newpath) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_rename((const char*)oldpath, (const char*)newpath);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ENOENT);
}

static long sys_mkdir_impl(long pathname, long mode, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_mkdir((const char*)pathname, (int)mode);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_EEXIST);
}

static long sys_rmdir_impl(long pathname, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_rmdir((const char*)pathname);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ENOENT);
}

static long sys_creat_impl(long pathname, long mode, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_open_impl(pathname, 0x0040 | 0x0200 | 0x0002, mode, 0, 0, 0);
}

static long sys_link_impl(long oldpath, long newpath, long a3, long a4, long a5, long a6)
{
    (void)oldpath; (void)newpath;
    (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_unlink_impl(long pathname, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_unlink((const char*)pathname);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_ENOENT);
}

static long sys_symlink_impl(long target, long linkpath, long a3, long a4, long a5, long a6)
{
    (void)target; (void)linkpath;
    (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_readlink_impl(long pathname, long buf, long bufsiz, long a4, long a5, long a6)
{
    (void)pathname; (void)buf; (void)bufsiz;
    (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_chmod_impl(long pathname, long mode, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);

    vfs_node_t* node = syscall_vfs_find_path((const char*)pathname);
    if (!node) return KENUX_ERR(KENUX_ENOENT);
    node->mode = (uint64_t)mode;
    return 0;
}

static long sys_chown_impl(long pathname, long owner, long group, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);

    vfs_node_t* node = syscall_vfs_find_path((const char*)pathname);
    if (!node) return KENUX_ERR(KENUX_ENOENT);
    node->uid = (uid_t)owner;
    node->gid = (gid_t)group;
    return 0;
}

static long sys_umask_impl(long mask, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

    mode_t old = proc_umask[current_process];
    if (mask != -1) {
        proc_umask[current_process] = (mode_t)mask & 0777;
    }
    return (long)old;
}

static long sys_gettimeofday_impl(long tv, long tz, long a3, long a4, long a5, long a6)
{
    (void)tz;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (tv) {
        struct { long tv_sec; long tv_usec; } *t = (void*)tv;
        t->tv_sec = (long)(kapi_get_time_ms() / 1000);
        t->tv_usec = (long)((kapi_get_time_ms() % 1000) * 1000);
    }
    return 0;
}

static long sys_getrlimit_impl(long resource, long rlim, long a3, long a4, long a5, long a6)
{
    (void)resource;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!rlim) return KENUX_ERR(KENUX_EFAULT);

    struct { long rlim_cur; long rlim_max; } *r = (void*)rlim;
    r->rlim_cur = -1;
    r->rlim_max = -1;
    return 0;
}

static long sys_getrusage_impl(long who, long usage, long a3, long a4, long a5, long a6)
{
    (void)who;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (usage) memset((void*)usage, 0, 144);
    return 0;
}

static long sys_sysinfo_impl(long info, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!info) return KENUX_ERR(KENUX_EFAULT);

    struct {
        long uptime;
        unsigned long loads[3];
        unsigned long totalram;
        unsigned long freeram;
        unsigned long sharedram;
        unsigned long bufferram;
        unsigned long totalswap;
        unsigned long freeswap;
        unsigned short procs;
        unsigned short pad;
        unsigned long totalhigh;
        unsigned long freehigh;
        unsigned int mem_unit;
    } *si = (void*)info;

    memset(si, 0, sizeof(*si));
    kapi_mem_info_t mi;
    kapi_mem_get_info(&mi);
    si->uptime = (long)(kapi_get_time_ms() / 1000);
    si->totalram = (unsigned long)mi.total;
    si->freeram = (unsigned long)mi.free;
    si->procs = (unsigned short)kapi_proc_count();
    si->mem_unit = 1;
    return 0;
}

static long sys_times_impl(long tbuf, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (tbuf) memset((void*)tbuf, 0, 32);
    return (long)kapi_get_time_ms();
}

static long sys_getuid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        return (long)proc_uid[current_process];
    }
    return 0;
}

static long sys_getgid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        return (long)proc_gid[current_process];
    }
    return 0;
}

static long sys_geteuid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        return (long)proc_euid[current_process];
    }
    return 0;
}

static long sys_getegid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        return (long)proc_egid[current_process];
    }
    return 0;
}

static long sys_setuid_impl(long uid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);

    /* 非 root 只能设置为自己的 uid */
    if (proc_euid[current_process] != 0 && (uid_t)uid != proc_uid[current_process]) {
        return KENUX_ERR(KENUX_EPERM);
    }

    proc_uid[current_process] = (uid_t)uid;
    proc_euid[current_process] = (uid_t)uid;
    return 0;
}

static long sys_getppid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        return (long)processes[current_process].parent_id;
    }
    return 0;
}

static long sys_getpgrp_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    /* 每个进程独立成组，pgid = pid */
    if (current_process < PROCESS_MAX) {
        return (long)processes[current_process].id;
    }
    return 0;
}

static long sys_gettid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_getpid_impl(0, 0, 0, 0, 0, 0);
}

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

static long sys_getdents64_impl(long fd, long dirp, long count, long a4, long a5, long a6)
{
    (void)count;
    (void)a4; (void)a5; (void)a6;
    if (!dirp) return KENUX_ERR(KENUX_EFAULT);
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_DIR) {
        return KENUX_ERR(KENUX_ENOTDIR);
    }

    kapi_dirent_t kentry;
    memset(&kentry, 0, sizeof(kentry));

    if (kapi_readdir(entry->obj.dir, &kentry) != KAPI_OK) {
        return 0;
    }

    struct linux_dirent64* d = (struct linux_dirent64*)dirp;
    d->d_ino = 1;
    d->d_off = 0;
    size_t namelen = strlen(kentry.name);
    d->d_reclen = (uint16_t)(sizeof(struct linux_dirent64) + namelen + 1);
    d->d_type = (kentry.type == KAPI_FT_DIR) ? 4 : 8;
    memcpy(d->d_name, kentry.name, namelen);
    d->d_name[namelen] = '\0';

    return (long)d->d_reclen;
}

static long sys_clock_gettime_impl(long clk_id, long tp, long a3, long a4, long a5, long a6)
{
    (void)clk_id;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!tp) return KENUX_ERR(KENUX_EFAULT);

    struct { long tv_sec; long tv_nsec; } *t = (void*)tp;
    uint64_t ms = kapi_get_time_ms();
    t->tv_sec = (long)(ms / 1000);
    t->tv_nsec = (long)((ms % 1000) * 1000000);
    return 0;
}

static long sys_exit_group_impl(long error_code, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_exit_impl(error_code, 0, 0, 0, 0, 0);
}

static long sys_openat_impl(long dirfd, long pathname, long flags, long mode,
                             long a5, long a6)
{
    (void)dirfd;
    (void)a5; (void)a6;
    return sys_open_impl(pathname, flags, mode, 0, 0, 0);
}

static long sys_newfstatat_impl(long dirfd, long pathname, long statbuf, long flags,
                                 long a5, long a6)
{
    (void)dirfd; (void)flags;
    (void)a5; (void)a6;
    return sys_stat_impl(pathname, statbuf, 0, 0, 0, 0);
}

static long sys_unlinkat_impl(long dirfd, long pathname, long flags, long a4, long a5, long a6)
{
    (void)dirfd; (void)flags;
    (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    return sys_unlink_impl(pathname, 0, 0, 0, 0, 0);
}

static long sys_sync_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    pagecache_sync_all();
    return 0;
}

static char proc_root[PROCESS_MAX][256];

static long sys_chroot_impl(long path, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!path) return KENUX_ERR(KENUX_EFAULT);

    vfs_node_t* node = syscall_vfs_find_path((const char*)path);
    if (!node) return KENUX_ERR(KENUX_ENOENT);
    if (node->type != FS_TYPE_DIRECTORY) return KENUX_ERR(KENUX_ENOTDIR);

    strncpy(proc_root[current_process], (const char*)path, sizeof(proc_root[0]) - 1);
    proc_root[current_process][sizeof(proc_root[0]) - 1] = '\0';
    return 0;
}

static long sys_sethostname_impl(long name, long len, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!name) return KENUX_ERR(KENUX_EFAULT);
    if (len < 0 || (size_t)len >= sizeof(kenux_hostname)) return KENUX_ERR(KENUX_EINVAL);
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);

    memcpy(kenux_hostname, (const void*)name, (size_t)len);
    kenux_hostname[len] = '\0';
    return 0;
}

static long sys_init_module_impl(long module_image, long len, long param_values,
                                  long a4, long a5, long a6)
{
    (void)param_values;
    (void)a4; (void)a5; (void)a6;
    if (!module_image || len <= 0) return KENUX_ERR(KENUX_EINVAL);

    int ret = module_load((const void*)module_image, (uint64_t)len, "unknown");
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    return 0;
}

static long sys_delete_module_impl(long name, long flags, long a3, long a4, long a5, long a6)
{
    (void)flags;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!name) return KENUX_ERR(KENUX_EFAULT);

    int ret = module_unload((const char*)name);
    if (ret < 0) return KENUX_ERR(KENUX_ENOENT);
    return 0;
}

static inline int rdrand64(uint64_t* val)
{
    unsigned char ok;
    __asm__ volatile ("rdrand %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok;
}

static uint64_t tsc_entropy(void)
{
    uint64_t t1, t2, t3, t4;
    uint32_t lo, hi;
#define _rdtsc(v) do { \
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi)); \
    (v) = ((uint64_t)hi << 32) | lo; \
} while(0)
    _rdtsc(t1);
    _rdtsc(t2);
    _rdtsc(t3);
    _rdtsc(t4);
#undef _rdtsc
    return t1 ^ t2 ^ t3 ^ t4;
}

static long sys_getrandom_impl(long buf, long buflen, long flags, long a4, long a5, long a6)
{
    (void)flags;
    (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);
    if (buflen <= 0) return 0;

    size_t len = (size_t)buflen;
    uint8_t* p = (uint8_t*)buf;
    uint64_t val;

    for (size_t i = 0; i < len; i++) {
        if (rdrand64(&val)) {
            p[i] = (uint8_t)(val >> ((i % 8) * 8));
        } else {
            p[i] = (uint8_t)(tsc_entropy() >> ((i % 8) * 8));
        }
    }
    return (long)len;
}

static long sys_kenux_info_impl(long buf, long size, long a3, long a4, long a5, long a6)
{
    (void)size;
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);

    struct {
        char version[32];
        char name[32];
    } *info = (void*)buf;

    memset(info, 0, sizeof(*info));
    strcpy(info->version, KENUX_VERSION_STRING);
    strcpy(info->name, "Kenux");
    return 0;
}

static long sys_kenux_get_loadavg_impl(long loads, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!loads) return KENUX_ERR(KENUX_EFAULT);

    uint64_t now = kapi_get_time_ms();
    uint64_t delta = now - loadavg_last_jiffies;
    if (delta > 0) {
        /* 统计当前活跃进程数（RUNNING 或 SLEEPING） */
        int active = 0;
        for (uint64_t i = 0; i < process_count; i++) {
            if (processes[i].state == PROCESS_RUNNING ||
                processes[i].state == PROCESS_SLEEPING) {
                active++;
            }
        }
        /* 指数衰减平均，固定点 1<<11 */
        const long fixed_1 = 1L << 11;
        /* 1分钟、5分钟、15分钟衰减常量（近似） */
        const long exp_1  = 1884;  /* 2048 * exp(-1/60)  近似 */
        const long exp_5  = 2014;  /* 2048 * exp(-1/300) 近似 */
        const long exp_15 = 2037;  /* 2048 * exp(-1/900) 近似 */
        loadavg_1min  = ((loadavg_1min  * exp_1)  + (active * fixed_1) * (2048 - exp_1))  / 2048;
        loadavg_5min  = ((loadavg_5min  * exp_5)  + (active * fixed_1) * (2048 - exp_5))  / 2048;
        loadavg_15min = ((loadavg_15min * exp_15) + (active * fixed_1) * (2048 - exp_15)) / 2048;
        loadavg_last_jiffies = now;
    }

    long* l = (long*)loads;
    l[0] = loadavg_1min;
    l[1] = loadavg_5min;
    l[2] = loadavg_15min;
    return 3;
}

/* ===== Futex 系统调用 ===== */
static long sys_futex_impl(long uaddr, long futex_op, long val, long timeout,
                            long uaddr2, long val3)
{
    if (!uaddr) return KENUX_ERR(KENUX_EFAULT);

    int op = (int)futex_op & 0x7f;
    uint64_t timeout_ms = 0;
    if (timeout) {
        struct { long tv_sec; long tv_nsec; } *ts = (void*)timeout;
        timeout_ms = (uint64_t)(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
    } else {
        timeout_ms = (uint64_t)-1; /* 无限等待 */
    }

    switch (op) {
        case FUTEX_WAIT: {
            int ret = futex_wait((void*)uaddr, (uint32_t)val, timeout_ms);
            return ret < 0 ? KENUX_ERR(KENUX_EAGAIN) : 0;
        }
        case FUTEX_WAKE: {
            int ret = futex_wake((void*)uaddr, (uint32_t)val);
            return ret;
        }
        case FUTEX_REQUEUE:
        case FUTEX_CMP_REQUEUE: {
            if (!uaddr2) return KENUX_ERR(KENUX_EFAULT);
            int ret = futex_requeue((void*)uaddr, (void*)uaddr2, (uint32_t)val);
            return ret;
        }
        default:
            return KENUX_ERR(KENUX_ENOSYS);
    }
}

/* ===== prctl / arch_prctl ===== */
static long sys_prctl_impl(long option, long arg2, long arg3, long arg4, long a5, long a6)
{
    (void)arg3; (void)arg4; (void)a5; (void)a6;
    switch (option) {
        case 15: /* PR_GET_NAME */
            if (!arg2) return KENUX_ERR(KENUX_EFAULT);
            if (current_process < PROCESS_MAX) {
                strncpy((char*)arg2, processes[current_process].name, 15);
                ((char*)arg2)[15] = '\0';
            }
            return 0;
        case 16: /* PR_SET_NAME */
            if (!arg2) return KENUX_ERR(KENUX_EFAULT);
            if (current_process < PROCESS_MAX) {
                strncpy(processes[current_process].name, (const char*)arg2,
                        sizeof(processes[0].name) - 1);
                processes[current_process].name[sizeof(processes[0].name) - 1] = '\0';
            }
            return 0;
        default:
            return 0;
    }
}

#define ARCH_SET_GS   0x1001
#define ARCH_SET_FS   0x1002
#define ARCH_GET_FS   0x1003
#define ARCH_GET_GS   0x1004

static long sys_arch_prctl_impl(long code, long addr, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    switch (code) {
        case ARCH_SET_FS:
            if (current_process < PROCESS_MAX) {
                processes[current_process].context.fs_base = (uint64_t)addr;
            }
            __asm__ volatile ("wrmsr" : : "a"((uint32_t)addr),
                              "d"((uint32_t)((uint64_t)addr >> 32)),
                              "c"(0xC0000100));
            return 0;
        case ARCH_SET_GS:
            if (current_process < PROCESS_MAX) {
                processes[current_process].context.gs_base = (uint64_t)addr;
            }
            __asm__ volatile ("wrmsr" : : "a"((uint32_t)addr),
                              "d"((uint32_t)((uint64_t)addr >> 32)),
                              "c"(0xC0000101));
            return 0;
        case ARCH_GET_FS:
            if (!addr) return KENUX_ERR(KENUX_EFAULT);
            *(uint64_t*)addr = current_process < PROCESS_MAX ?
                processes[current_process].context.fs_base : 0;
            return 0;
        case ARCH_GET_GS:
            if (!addr) return KENUX_ERR(KENUX_EFAULT);
            *(uint64_t*)addr = current_process < PROCESS_MAX ?
                processes[current_process].context.gs_base : 0;
            return 0;
        default:
            return KENUX_ERR(KENUX_EINVAL);
    }
}

/* ===== setpgid / setsid / getpgid / getsid ===== */
static long sys_setpgid_impl(long pid, long pgid, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (pid == 0) pid = (long)current_process;
    if (pgid == 0) pgid = pid;
    if (pid >= PROCESS_MAX) return KENUX_ERR(KENUX_ESRCH);
    if (processes[pid].state == PROCESS_UNUSED) return KENUX_ERR(KENUX_ESRCH);
    /* pgid 暂时用 parent_id 字段充当（简化） */
    processes[pid].parent_id = (uint64_t)pgid;
    return 0;
}

static long sys_setsid_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    return (long)current_process;
}

static long sys_getpgid_impl(long pid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (pid == 0) pid = (long)current_process;
    if (pid >= PROCESS_MAX) return KENUX_ERR(KENUX_ESRCH);
    return (long)processes[pid].parent_id;
}

static long sys_getsid_impl(long pid, long a2, long a3, long a4, long a5, long a6)
{
    return sys_getpgid_impl(pid, a2, a3, a4, a5, a6);
}

/* ===== setgid / setegid / setreuid / setregid ===== */
static long sys_setgid_impl(long gid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    proc_gid[current_process] = (uid_t)gid;
    proc_egid[current_process] = (uid_t)gid;
    return 0;
}

static long sys_setreuid_impl(long ruid, long euid, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (ruid != -1) proc_uid[current_process] = (uid_t)ruid;
    if (euid != -1) proc_euid[current_process] = (uid_t)euid;
    return 0;
}

static long sys_setregid_impl(long rgid, long egid, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (rgid != -1) proc_gid[current_process] = (uid_t)rgid;
    if (egid != -1) proc_egid[current_process] = (uid_t)egid;
    return 0;
}

static long sys_getpriority_impl(long which, long who, long a3, long a4, long a5, long a6)
{
    (void)who; (void)a3; (void)a4; (void)a5; (void)a6;
    if (which == 0 /* PRIO_PROCESS */ && current_process < PROCESS_MAX) {
        return (long)processes[current_process].nice;
    }
    return 0;
}

static long sys_setpriority_impl(long which, long who, long prio, long a4, long a5, long a6)
{
    (void)which; (void)who; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        if (prio < -20) prio = -20;
        if (prio > 19) prio = 19;
        processes[current_process].nice = (int)prio;
        processes[current_process].cfs_task.nice = (int)prio;
        processes[current_process].cfs_task.load_weight =
            cfs_nice_to_weight((int)prio);
    }
    return 0;
}

/* ===== mlock / munlock / mlockall / munlockall ===== */
static long sys_mlock_impl(long addr, long len, long a3, long a4, long a5, long a6)
{
    (void)addr; (void)len; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_munlock_impl(long addr, long len, long a3, long a4, long a5, long a6)
{
    (void)addr; (void)len; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_mlockall_impl(long flags, long a2, long a3, long a4, long a5, long a6)
{
    (void)flags; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_munlockall_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== truncate / ftruncate / fsync / fdatasync ===== */
static long sys_truncate_impl(long path, long length, long a3, long a4, long a5, long a6)
{
    (void)length; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!path) return KENUX_ERR(KENUX_EFAULT);
    vfs_node_t* node = syscall_vfs_find_path((const char*)path);
    if (!node) return KENUX_ERR(KENUX_ENOENT);
    node->size = (uint64_t)length;
    return 0;
}

static long sys_ftruncate_impl(long fd, long length, long a3, long a4, long a5, long a6)
{
    (void)length; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

static long sys_fsync_impl(long fd, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

static long sys_fdatasync_impl(long fd, long a2, long a3, long a4, long a5, long a6)
{
    return sys_fsync_impl(fd, a2, a3, a4, a5, a6);
}

/* ===== flock ===== */
static long sys_flock_impl(long fd, long operation, long a3, long a4, long a5, long a6)
{
    (void)operation; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

/* ===== mremap / msync / madvise ===== */
static long sys_mremap_impl(long old_addr, long old_size, long new_size, long flags,
                             long new_addr, long a6)
{
    (void)flags; (void)new_addr; (void)a6;
    if (!proc_mmap_initialized[current_process]) return KENUX_ERR(KENUX_EINVAL);
    int ret = mmap_do_munmap(&proc_mmap_ctx[current_process],
                             (uint64_t)old_addr, (uint64_t)old_size);
    if (ret < 0) return KENUX_ERR(KENUX_EINVAL);
    uint64_t r = mmap_do_mmap(&proc_mmap_ctx[current_process], (uint64_t)old_addr,
                              (uint64_t)new_size, PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS, -1, 0);
    return r ? (long)r : KENUX_ERR(KENUX_ENOMEM);
}

static long sys_msync_impl(long addr, long len, long flags, long a4, long a5, long a6)
{
    (void)addr; (void)len; (void)flags; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_madvise_impl(long addr, long length, long advice, long a4, long a5, long a6)
{
    (void)addr; (void)length; (void)advice; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== pread64 / pwrite64 ===== */
static long sys_pread64_impl(long fd, long buf, long count, long pos, long a5, long a6)
{
    (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_EBADF);
    int64_t old_pos = kapi_seek(entry->obj.file, 0, 1 /* SEEK_CUR */);
    if (old_pos < 0) return KENUX_ERR(KENUX_EIO);
    if (kapi_seek(entry->obj.file, pos, 0 /* SEEK_SET */) < 0) return KENUX_ERR(KENUX_EIO);
    int64_t ret = kapi_read(entry->obj.file, (void*)buf, (size_t)count);
    kapi_seek(entry->obj.file, old_pos, 0);
    return ret < 0 ? KENUX_ERR(KENUX_EIO) : ret;
}

static long sys_pwrite64_impl(long fd, long buf, long count, long pos, long a5, long a6)
{
    (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_EBADF);
    int64_t old_pos = kapi_seek(entry->obj.file, 0, 1);
    if (old_pos < 0) return KENUX_ERR(KENUX_EIO);
    if (kapi_seek(entry->obj.file, pos, 0) < 0) return KENUX_ERR(KENUX_EIO);
    int64_t ret = kapi_write(entry->obj.file, (void*)buf, (size_t)count);
    kapi_seek(entry->obj.file, old_pos, 0);
    return ret < 0 ? KENUX_ERR(KENUX_EIO) : ret;
}

/* ===== sendfile ===== */
static long sys_sendfile_impl(long out_fd, long in_fd, long offset, long count,
                              long a5, long a6)
{
    (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, in_fd) < 0) return KENUX_ERR(KENUX_EBADF);
    if (kapi_fd_check((int)current_process, out_fd) < 0) return KENUX_ERR(KENUX_EBADF);

    char buf[512];
    long total = 0;
    while (total < count) {
        long n = count - total;
        if (n > (long)sizeof(buf)) n = sizeof(buf);
        long r = sys_read_impl(in_fd, (long)buf, n, 0, 0, 0);
        if (r <= 0) break;
        long w = sys_write_impl(out_fd, (long)buf, r, 0, 0, 0);
        if (w <= 0) break;
        total += w;
        if (offset) *(long*)offset += w;
    }
    return total > 0 ? total : KENUX_ERR(KENUX_EIO);
}

/* ===== time / time-related ===== */
static long sys_time_impl(long tloc, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    long t = (long)(kapi_get_time_ms() / 1000);
    if (tloc) *(long*)tloc = t;
    return t;
}

static long sys_tkill_impl(long tid, long sig, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_kill_impl(tid, sig, 0, 0, 0, 0);
}

static long sys_tgkill_impl(long tgid, long tid, long sig, long a4, long a5, long a6)
{
    (void)tgid; (void)a4; (void)a5; (void)a6;
    return sys_kill_impl(tid, sig, 0, 0, 0, 0);
}

/* ===== set_tid_address / restart_syscall ===== */
static long sys_set_tid_address_impl(long tidptr, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process < PROCESS_MAX) {
        processes[current_process].kthread_data = (void*)tidptr;
    }
    return (long)current_process;
}

static long sys_restart_syscall_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_EINTR);
}

/* ===== epoll 系统调用 ===== */
static long sys_epoll_create_impl(long size, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_epoll_create((int)size);
    return ret < 0 ? KENUX_ERR(KENUX_ENOMEM) : ret;
}

static long sys_epoll_create1_impl(long flags, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_epoll_create1((int)flags);
    return ret < 0 ? KENUX_ERR(KENUX_ENOMEM) : ret;
}

static long sys_epoll_ctl_impl(long epfd, long op, long fd, long event, long a5, long a6)
{
    (void)a5; (void)a6;
    if (!event && op != KAPI_EPOLL_CTL_DEL) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_epoll_ctl((int)epfd, (int)op, (int)fd, (kapi_epoll_event_t*)event);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : 0;
}

static long sys_epoll_wait_impl(long epfd, long events, long maxevents, long timeout, long a5, long a6)
{
    (void)a5; (void)a6;
    if (!events) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_epoll_wait((int)epfd, (kapi_epoll_event_t*)events, (int)maxevents, (int)timeout);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_epoll_pwait_impl(long epfd, long events, long maxevents, long timeout, long sigmask, long a6)
{
    (void)a6;
    if (!events) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_epoll_pwait((int)epfd, (kapi_epoll_event_t*)events, (int)maxevents,
                               (int)timeout, (const uint64_t*)sigmask);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

/* ===== poll / select / ppoll / pselect6 ===== */
static long sys_poll_impl(long fds, long nfds, long timeout, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!fds && nfds > 0) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_poll((kapi_pollfd_t*)fds, (uint32_t)nfds, (int)timeout);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_select_impl(long nfds, long readfds, long writefds, long exceptfds, long timeout, long a6)
{
    (void)a6;
    int ret = kapi_select((int)nfds, (kapi_fd_set_t*)readfds, (kapi_fd_set_t*)writefds,
                          (kapi_fd_set_t*)exceptfds, (uint64_t*)timeout);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_ppoll_impl(long fds, long nfds, long timeout, long sigmask, long a5, long a6)
{
    (void)a5; (void)a6;
    if (!fds && nfds > 0) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_ppoll((kapi_pollfd_t*)fds, (uint32_t)nfds, (uint64_t*)timeout, (const uint64_t*)sigmask);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_pselect6_impl(long nfds, long readfds, long writefds, long exceptfds, long timeout, long sigmask)
{
    int ret = kapi_pselect6((int)nfds, (kapi_fd_set_t*)readfds, (kapi_fd_set_t*)writefds,
                            (kapi_fd_set_t*)exceptfds, (uint64_t*)timeout, (const uint64_t*)sigmask);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

/* ===== signalfd4 ===== */
static long sys_signalfd4_impl(long fd, long mask, long masksize, long flags, long a5, long a6)
{
    (void)a5; (void)a6;
    if (!mask) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_signalfd4((int)fd, (const uint64_t*)mask, (size_t)masksize, (int)flags);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_signalfd_impl(long fd, long mask, long masksize, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!mask) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_signalfd((int)fd, (const uint64_t*)mask, (size_t)masksize, 0);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

/* ===== timerfd ===== */
static long sys_timerfd_create_impl(long clockid, long flags, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_timerfd_create((int)clockid, (int)flags);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_timerfd_settime_impl(long fd, long flags, long new_value, long old_value, long a5, long a6)
{
    (void)a5; (void)a6;
    if (!new_value) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_timerfd_settime((int)fd, (int)flags,
                                   (const kapi_itimerspec_t*)new_value,
                                   (kapi_itimerspec_t*)old_value);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : 0;
}

static long sys_timerfd_gettime_impl(long fd, long curr_value, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!curr_value) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_timerfd_gettime((int)fd, (kapi_itimerspec_t*)curr_value);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : 0;
}

/* ===== eventfd2 ===== */
static long sys_eventfd2_impl(long initval, long flags, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_eventfd2((unsigned int)initval, (int)flags);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_eventfd_impl(long initval, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_eventfd((unsigned int)initval, 0);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

/* ===== inotify ===== */
static long sys_inotify_init_impl(long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_inotify_init();
    return ret < 0 ? KENUX_ERR(KENUX_ENOMEM) : ret;
}

static long sys_inotify_init1_impl(long flags, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_inotify_init1((int)flags);
    return ret < 0 ? KENUX_ERR(KENUX_ENOMEM) : ret;
}

static long sys_inotify_add_watch_impl(long fd, long pathname, long mask, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_inotify_add_watch((int)fd, (const char*)pathname, (uint32_t)mask);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : ret;
}

static long sys_inotify_rm_watch_impl(long fd, long wd, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    int ret = kapi_inotify_rm_watch((int)fd, (int)wd);
    return ret < 0 ? KENUX_ERR(KENUX_EINVAL) : 0;
}

/* ===== readv / writev ===== */
static long sys_readv_impl(long fd, long iov, long iovcnt, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!iov) return KENUX_ERR(KENUX_EFAULT);
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_EBADF);

    struct { void* iov_base; size_t iov_len; } *iovecs = (void*)iov;
    long total = 0;
    for (long i = 0; i < iovcnt; i++) {
        if (!iovecs[i].iov_base || iovecs[i].iov_len == 0) continue;
        int64_t r = kapi_read(entry->obj.file, iovecs[i].iov_base, iovecs[i].iov_len);
        if (r < 0) return total > 0 ? total : KENUX_ERR(KENUX_EIO);
        total += r;
        if ((size_t)r < iovecs[i].iov_len) break;
    }
    return total;
}

static long sys_writev_impl(long fd, long iov, long iovcnt, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (!iov) return KENUX_ERR(KENUX_EFAULT);
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);

    kapi_fd_entry_t* entry = &proc_fd_table[current_process][fd];
    if (entry->type != KAPI_FD_FILE) return KENUX_ERR(KENUX_EBADF);

    struct { const void* iov_base; size_t iov_len; } *iovecs = (void*)iov;
    long total = 0;
    for (long i = 0; i < iovcnt; i++) {
        if (!iovecs[i].iov_base || iovecs[i].iov_len == 0) continue;
        int64_t r = kapi_write(entry->obj.file, iovecs[i].iov_base, iovecs[i].iov_len);
        if (r < 0) return total > 0 ? total : KENUX_ERR(KENUX_EIO);
        total += r;
        if ((size_t)r < iovecs[i].iov_len) break;
    }
    return total;
}

/* ===== pipe2 ===== */
static long sys_pipe2_impl(long pipefd, long flags, long a3, long a4, long a5, long a6)
{
    (void)flags; (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_pipe_impl(pipefd, 0, 0, 0, 0, 0);
}

/* ===== dup3 ===== */
static long sys_dup3_impl(long oldfd, long newfd, long flags, long a4, long a5, long a6)
{
    (void)flags; (void)a4; (void)a5; (void)a6;
    return sys_dup2_impl(oldfd, newfd, 0, 0, 0, 0);
}

/* ===== accept4 ===== */
static long sys_accept4_impl(long sockfd, long addr, long addrlen, long flags, long a5, long a6)
{
    (void)flags; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, sockfd) < 0) return KENUX_ERR(KENUX_EBADF);
    kapi_fd_entry_t* entry = &proc_fd_table[current_process][sockfd];
    if (entry->type != KAPI_FD_SOCKET) return KENUX_ERR(KENUX_ENOTSOCK);

    int sfd = (int)(uintptr_t)entry->obj.socket;
    sockaddr_in_t a;
    uint32_t len = sizeof(a);
    int fd = sys_accept(sfd, &a, &len);
    if (fd < 0) return KENUX_ERR(KENUX_EINVAL);

    int kfd = kapi_fd_alloc((int)current_process);
    if (kfd < 0) {
        sys_close_socket(fd);
        return KENUX_ERR(KENUX_EMFILE);
    }

    proc_fd_table[current_process][kfd].type = KAPI_FD_SOCKET;
    proc_fd_table[current_process][kfd].obj.socket = (void*)(uintptr_t)fd;
    proc_fd_table[current_process][kfd].flags = 0;
    proc_fd_table[current_process][kfd].cloexec = 0;
    return kfd;
}

/* ===== waitid ===== */
static long sys_waitid_impl(long idtype, long id, long infop, long options, long a5, long a6)
{
    (void)idtype; (void)options; (void)a5; (void)a6;
    int status = 0;
    int ret = kapi_proc_wait((int)id, &status);
    if (infop) {
        memset((void*)infop, 0, 32);
        if (ret > 0) {
            ((int*)infop)[0] = ret;
            ((int*)infop)[1] = 4;
            ((int*)infop)[2] = status;
        }
    }
    return ret < 0 ? KENUX_ERR(KENUX_ECHILD) : 0;
}

/* ===== setrlimit ===== */
static long sys_setrlimit_impl(long resource, long rlim, long a3, long a4, long a5, long a6)
{
    (void)resource; (void)rlim;
    (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== mount / umount2 ===== */
static long sys_mount_impl(long dev_name, long dir_name, long type, long flags, long data, long a6)
{
    (void)data; (void)a6;
    if (!dir_name || !type) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_mount((const char*)dev_name, (const char*)dir_name,
                         (const char*)type, (uint64_t)flags);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_EINVAL);
}

static long sys_umount2_impl(long name, long flags, long a3, long a4, long a5, long a6)
{
    (void)flags; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!name) return KENUX_ERR(KENUX_EFAULT);
    int ret = kapi_umount((const char*)name);
    return ret == KAPI_OK ? 0 : KENUX_ERR(KENUX_EINVAL);
}

/* ===== mknod ===== */
static long sys_mknod_impl(long pathname, long mode, long dev, long a4, long a5, long a6)
{
    (void)dev; (void)a4; (void)a5; (void)a6;
    if (!pathname) return KENUX_ERR(KENUX_EFAULT);
    uint32_t m = (uint32_t)mode;
    if ((m & 0170000) == 0) m |= 0100000;
    if ((m & 0170000) == 0100000) {
        kapi_file_t f = kapi_open((const char*)pathname, KAPI_O_CREAT | KAPI_O_WRONLY | KAPI_O_TRUNC, (int)(m & 07777));
        if (!f) return KENUX_ERR(KENUX_EACCES);
        kapi_close(f);
        return 0;
    }
    return 0;
}

/* ===== lstat ===== */
static long sys_lstat_impl(long pathname, long statbuf, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_stat_impl(pathname, statbuf, 0, 0, 0, 0);
}

/* ===== getgroups / setgroups ===== */
static long sys_getgroups_impl(long size, long list, long a3, long a4, long a5, long a6)
{
    (void)list; (void)a3; (void)a4; (void)a5; (void)a6;
    return (size == 0) ? 1 : 1;
}

static long sys_setgroups_impl(long size, long list, long a3, long a4, long a5, long a6)
{
    (void)size; (void)list; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== setresuid / getresuid / setresgid / getresgid ===== */
static long sys_setresuid_impl(long ruid, long euid, long suid, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (ruid != -1) proc_uid[current_process] = (uid_t)ruid;
    if (euid != -1) proc_euid[current_process] = (uid_t)euid;
    return 0;
}

static long sys_getresuid_impl(long ruid, long euid, long suid, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (ruid) *(uid_t*)ruid = proc_uid[current_process];
    if (euid) *(uid_t*)euid = proc_euid[current_process];
    if (suid) *(uid_t*)suid = proc_uid[current_process];
    return 0;
}

static long sys_setresgid_impl(long rgid, long egid, long sgid, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (rgid != -1) proc_gid[current_process] = (uid_t)rgid;
    if (egid != -1) proc_egid[current_process] = (uid_t)egid;
    return 0;
}

static long sys_getresgid_impl(long rgid, long egid, long sgid, long a4, long a5, long a6)
{
    (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (rgid) *(uid_t*)rgid = proc_gid[current_process];
    if (egid) *(uid_t*)egid = proc_egid[current_process];
    if (sgid) *(uid_t*)sgid = proc_gid[current_process];
    return 0;
}

/* ===== setfsuid / setfsgid ===== */
static long sys_setfsuid_impl(long uid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    uid_t old = proc_euid[current_process];
    proc_euid[current_process] = (uid_t)uid;
    return (long)old;
}

static long sys_setfsgid_impl(long gid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    uid_t old = proc_egid[current_process];
    proc_egid[current_process] = (uid_t)gid;
    return (long)old;
}

/* ===== capget / capset ===== */
static long sys_capget_impl(long header, long dataptr, long a3, long a4, long a5, long a6)
{
    (void)header; (void)dataptr; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_capset_impl(long header, long data, long a3, long a4, long a5, long a6)
{
    (void)header; (void)data; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== rt_sigpending / rt_sigtimedwait / rt_sigqueueinfo / rt_sigsuspend ===== */
static long sys_rt_sigpending_impl(long set, long sigsetsize, long a3, long a4, long a5, long a6)
{
    (void)sigsetsize; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!set) return KENUX_ERR(KENUX_EFAULT);
    memset((void*)set, 0, 8);
    return 0;
}

static long sys_rt_sigtimedwait_impl(long set, long info, long timeout, long sigsetsize, long a5, long a6)
{
    (void)set; (void)info; (void)timeout; (void)sigsetsize; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_EAGAIN);
}

static long sys_rt_sigqueueinfo_impl(long pid, long sig, long info, long a4, long a5, long a6)
{
    (void)info; (void)a4; (void)a5; (void)a6;
    return sys_kill_impl(pid, sig, 0, 0, 0, 0);
}

static long sys_rt_sigsuspend_impl(long mask, long sigsetsize, long a3, long a4, long a5, long a6)
{
    (void)mask; (void)sigsetsize; (void)a3; (void)a4; (void)a5; (void)a6;
    kapi_proc_yield();
    return KENUX_ERR(KENUX_EINTR);
}

/* ===== sigaltstack ===== */
static long sys_sigaltstack_impl(long ss, long oss, long a3, long a4, long a5, long a6)
{
    (void)ss; (void)oss; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== sched_setparam / sched_getparam / sched_setscheduler / sched_getscheduler ===== */
static long sys_sched_setparam_impl(long pid, long param, long a3, long a4, long a5, long a6)
{
    (void)pid; (void)param; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_sched_getparam_impl(long pid, long param, long a3, long a4, long a5, long a6)
{
    (void)pid; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!param) return KENUX_ERR(KENUX_EFAULT);
    memset((void*)param, 0, 4);
    return 0;
}

static long sys_sched_setscheduler_impl(long pid, long policy, long param, long a4, long a5, long a6)
{
    (void)pid; (void)policy; (void)param; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_sched_getscheduler_impl(long pid, long a2, long a3, long a4, long a5, long a6)
{
    (void)pid; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_sched_get_priority_max_impl(long policy, long a2, long a3, long a4, long a5, long a6)
{
    (void)policy; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 99;
}

static long sys_sched_get_priority_min_impl(long policy, long a2, long a3, long a4, long a5, long a6)
{
    (void)policy; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 1;
}

static long sys_sched_rr_get_interval_impl(long pid, long interval, long a3, long a4, long a5, long a6)
{
    (void)pid; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!interval) return KENUX_ERR(KENUX_EFAULT);
    struct { long tv_sec; long tv_nsec; } *t = (void*)interval;
    t->tv_sec = 0;
    t->tv_nsec = 10000000;
    return 0;
}

/* ===== sched_setaffinity / sched_getaffinity ===== */
static long sys_sched_setaffinity_impl(long pid, long len, long mask, long a4, long a5, long a6)
{
    (void)pid; (void)len; (void)mask; (void)a4; (void)a5; (void)a6;
    return 0;
}

static long sys_sched_getaffinity_impl(long pid, long len, long mask, long a4, long a5, long a6)
{
    (void)pid; (void)a4; (void)a5; (void)a6;
    if (!mask || len == 0) return KENUX_ERR(KENUX_EFAULT);
    size_t sz = (size_t)len < 8 ? (size_t)len : 8;
    memset((void*)mask, 0xff, sz);
    return (long)sz;
}

/* ===== personality ===== */
static long sys_personality_impl(long persona, long a2, long a3, long a4, long a5, long a6)
{
    (void)persona; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== statfs / fstatfs ===== */
static long sys_statfs_impl(long pathname, long buf, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!pathname || !buf) return KENUX_ERR(KENUX_EFAULT);
    memset((void*)buf, 0, 64);
    return 0;
}

static long sys_fstatfs_impl(long fd, long buf, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    memset((void*)buf, 0, 64);
    return 0;
}

/* ===== settimeofday ===== */
static long sys_settimeofday_impl(long tv, long tz, long a3, long a4, long a5, long a6)
{
    (void)tv; (void)tz; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== reboot ===== */
static long sys_reboot_impl(long magic1, long magic2, long cmd, long arg, long a5, long a6)
{
    (void)magic1; (void)magic2; (void)cmd; (void)arg; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== swapon / swapoff ===== */
static long sys_swapon_impl(long specialfile, long swapflags, long a3, long a4, long a5, long a6)
{
    (void)specialfile; (void)swapflags; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

static long sys_swapoff_impl(long specialfile, long swapflags, long a3, long a4, long a5, long a6)
{
    (void)specialfile; (void)swapflags; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== iopl / ioperm ===== */
static long sys_iopl_impl(long level, long a2, long a3, long a4, long a5, long a6)
{
    (void)level; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

static long sys_ioperm_impl(long from, long num, long on, long a4, long a5, long a6)
{
    (void)from; (void)num; (void)on; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== pivot_root ===== */
static long sys_pivot_root_impl(long new_root, long put_old, long a3, long a4, long a5, long a6)
{
    (void)new_root; (void)put_old; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== syslog ===== */
static long sys_syslog_impl(long type, long buf, long len, long a4, long a5, long a6)
{
    (void)type; (void)buf; (void)len; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== shmget / shmat / shmctl / shmdt ===== */
static long sys_shmget_impl(long key, long size, long shmflg, long a4, long a5, long a6)
{
    (void)key; (void)size; (void)shmflg; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_shmat_impl(long shmid, long shmaddr, long shmflg, long a4, long a5, long a6)
{
    (void)shmid; (void)shmaddr; (void)shmflg; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_shmctl_impl(long shmid, long cmd, long buf, long a4, long a5, long a6)
{
    (void)shmid; (void)cmd; (void)buf; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_shmdt_impl(long shmaddr, long a2, long a3, long a4, long a5, long a6)
{
    (void)shmaddr; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== semget / semop / semctl ===== */
static long sys_semget_impl(long key, long nsems, long semflg, long a4, long a5, long a6)
{
    (void)key; (void)nsems; (void)semflg; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_semop_impl(long semid, long sops, long nsops, long a4, long a5, long a6)
{
    (void)semid; (void)sops; (void)nsops; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_semctl_impl(long semid, long semnum, long cmd, long arg, long a5, long a6)
{
    (void)semid; (void)semnum; (void)cmd; (void)arg; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== msgget / msgsnd / msgrcv / msgctl ===== */
static long sys_msgget_impl(long key, long msgflg, long a3, long a4, long a5, long a6)
{
    (void)key; (void)msgflg; (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_msgsnd_impl(long msqid, long msgp, long msgsz, long msgflg, long a5, long a6)
{
    (void)msqid; (void)msgp; (void)msgsz; (void)msgflg; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_msgrcv_impl(long msqid, long msgp, long msgsz, long msgtyp, long msgflg, long a6)
{
    (void)msqid; (void)msgp; (void)msgsz; (void)msgtyp; (void)msgflg; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_msgctl_impl(long msqid, long cmd, long buf, long a4, long a5, long a6)
{
    (void)msqid; (void)cmd; (void)buf; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== fallocate ===== */
static long sys_fallocate_impl(long fd, long mode, long offset, long length, long a5, long a6)
{
    (void)mode; (void)offset; (void)length; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

/* ===== memfd_create ===== */
static long sys_memfd_create_impl(long name, long flags, long a3, long a4, long a5, long a6)
{
    (void)name; (void)flags; (void)a3; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== bpf ===== */
static long sys_bpf_impl(long cmd, long attr, long size, long a4, long a5, long a6)
{
    (void)cmd; (void)attr; (void)size; (void)a4; (void)a5; (void)a6;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== seccomp ===== */
static long sys_seccomp_impl(long operation, long flags, long args, long a4, long a5, long a6)
{
    (void)operation; (void)flags; (void)args; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== splice / tee / vmsplice ===== */
static long sys_splice_impl(long fd_in, long off_in, long fd_out, long off_out, long len, long flags)
{
    (void)off_in; (void)off_out; (void)flags;
    if (kapi_fd_check((int)current_process, fd_in) < 0) return KENUX_ERR(KENUX_EBADF);
    if (kapi_fd_check((int)current_process, fd_out) < 0) return KENUX_ERR(KENUX_EBADF);
    char buf[512];
    long total = 0;
    while (total < len) {
        long n = len - total;
        if (n > (long)sizeof(buf)) n = sizeof(buf);
        long r = sys_read_impl(fd_in, (long)buf, n, 0, 0, 0);
        if (r <= 0) break;
        long w = sys_write_impl(fd_out, (long)buf, r, 0, 0, 0);
        if (w <= 0) break;
        total += w;
    }
    return total > 0 ? total : KENUX_ERR(KENUX_EIO);
}

static long sys_tee_impl(long fd_in, long fd_out, long len, long flags, long a5, long a6)
{
    (void)fd_in; (void)fd_out; (void)len; (void)flags; (void)a5; (void)a6;
    return 0;
}

static long sys_vmsplice_impl(long fd, long iov, long nr_segs, long flags, long a5, long a6)
{
    (void)fd; (void)iov; (void)nr_segs; (void)flags; (void)a5; (void)a6;
    return 0;
}

/* ===== mkdirat / mknodat / fchownat / unlinkat (enhanced) / renameat / symlinkat / readlinkat / fchmodat / faccessat ===== */
static long sys_mkdirat_impl(long dirfd, long pathname, long mode, long a4, long a5, long a6)
{
    (void)dirfd; (void)a4; (void)a5; (void)a6;
    return sys_mkdir_impl(pathname, mode, 0, 0, 0, 0);
}

static long sys_mknodat_impl(long dirfd, long pathname, long mode, long dev, long a5, long a6)
{
    (void)dirfd; (void)a5; (void)a6;
    return sys_mknod_impl(pathname, mode, dev, 0, 0, 0);
}

static long sys_fchownat_impl(long dirfd, long pathname, long owner, long group, long flags, long a6)
{
    (void)dirfd; (void)flags; (void)a6;
    return sys_chown_impl(pathname, owner, group, 0, 0, 0);
}

static long sys_renameat_impl(long olddirfd, long oldpath, long newdirfd, long newpath, long a5, long a6)
{
    (void)olddirfd; (void)newdirfd; (void)a5; (void)a6;
    return sys_rename_impl(oldpath, newpath, 0, 0, 0, 0);
}

static long sys_symlinkat_impl(long target, long newdirfd, long linkpath, long a4, long a5, long a6)
{
    (void)newdirfd; (void)a4; (void)a5; (void)a6;
    return sys_symlink_impl(target, linkpath, 0, 0, 0, 0);
}

static long sys_readlinkat_impl(long dirfd, long pathname, long buf, long bufsiz, long a5, long a6)
{
    (void)dirfd; (void)a5; (void)a6;
    return sys_readlink_impl(pathname, buf, bufsiz, 0, 0, 0);
}

static long sys_fchmodat_impl(long dirfd, long pathname, long mode, long flags, long a5, long a6)
{
    (void)dirfd; (void)flags; (void)a5; (void)a6;
    return sys_chmod_impl(pathname, mode, 0, 0, 0, 0);
}

static long sys_faccessat_impl(long dirfd, long pathname, long mode, long flags, long a5, long a6)
{
    (void)dirfd; (void)mode; (void)flags; (void)a5; (void)a6;
    return sys_access_impl(pathname, 0, 0, 0, 0, 0);
}

/* ===== fchdir ===== */
static long sys_fchdir_impl(long fd, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

/* ===== fchmod ===== */
static long sys_fchmod_impl(long fd, long mode, long a3, long a4, long a5, long a6)
{
    (void)mode; (void)a3; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

/* ===== fchown ===== */
static long sys_fchown_impl(long fd, long owner, long group, long a4, long a5, long a6)
{
    (void)owner; (void)group; (void)a4; (void)a5; (void)a6;
    if (kapi_fd_check((int)current_process, fd) < 0) return KENUX_ERR(KENUX_EBADF);
    return 0;
}

/* ===== lchown ===== */
static long sys_lchown_impl(long pathname, long owner, long group, long a4, long a5, long a6)
{
    return sys_chown_impl(pathname, owner, group, a4, a5, a6);
}

/* ===== getdents ===== */
static long sys_getdents_impl(long fd, long dirp, long count, long a4, long a9, long a6)
{
    (void)a4; (void)a9; (void)a6;
    return sys_getdents64_impl(fd, dirp, count, 0, 0, 0);
}

/* ===== adjtimex ===== */
static long sys_adjtimex_impl(long buf, long a2, long a3, long a4, long a5, long a6)
{
    (void)buf; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== acct ===== */
static long sys_acct_impl(long filename, long a2, long a3, long a4, long a5, long a6)
{
    (void)filename; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

/* ===== setpgid / setsid / getpgid / getsid ===== */
static long sys_setpgid_impl(long pid, long pgid, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    int p = (pid == 0) ? (int)current_process : (int)pid;
    int g = (pgid == 0) ? p : (int)pgid;
    if (p >= PROCESS_MAX) return KENUX_ERR(KENUX_ESRCH);
    proc_pgid[p] = g;
    return 0;
}

static long sys_getpgid_impl(long pid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    int p = (pid == 0) ? (int)current_process : (int)pid;
    if (p >= PROCESS_MAX) return KENUX_ERR(KENUX_ESRCH);
    return (long)proc_pgid[p];
}

static long sys_getsid_impl(long pid, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    int p = (pid == 0) ? (int)current_process : (int)pid;
    if (p >= PROCESS_MAX) return KENUX_ERR(KENUX_ESRCH);
    return (long)proc_pgid[p];
}

/* ===== uname ===== */
static long sys_uname_impl(long buf, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf) return KENUX_ERR(KENUX_EFAULT);
    memset((void*)buf, 0, 390);
    const char* sysname = "Kenux";
    const char* release = "2.0.0";
    const char* version = "#1 SMP";
    const char* machine = "x86_64";
    char* p = (char*)buf;
    int i;
    for (i = 0; sysname[i] && i < 64; i++) p[i] = sysname[i];
    p += 65;
    for (i = 0; release[i] && i < 64; i++) p[i] = release[i];
    p += 65;
    for (i = 0; version[i] && i < 64; i++) p[i] = version[i];
    p += 65;
    for (i = 0; machine[i] && i < 64; i++) p[i] = machine[i];
    return 0;
}

/* ===== sysinfo ===== */
static long sys_sysinfo_impl(long info, long a2, long a3, long a4, long a5, long a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!info) return KENUX_ERR(KENUX_EFAULT);
    memset((void*)info, 0, 64);
    return 0;
}

/* ===== prctl ===== */
static long sys_prctl_impl(long option, long arg2, long arg3, long arg4, long arg5, long a6)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)a6;
    switch ((int)option) {
        case 15: return 0;
        case 23: return 0;
        default: return 0;
    }
}

/* ===== getrlimit ===== */
static long sys_getrlimit_impl(long resource, long rlim, long a3, long a4, long a5, long a6)
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!rlim) return KENUX_ERR(KENUX_EFAULT);
    uint64_t* r = (uint64_t*)rlim;
    r[0] = (uint64_t)-1;
    r[1] = (uint64_t)-1;
    return 0;
}

/* ===== prlimit64 ===== */
static long sys_prlimit64_impl(long pid, long resource, long new_limit, long old_limit, long a5, long a6)
{
    (void)pid; (void)new_limit; (void)a5; (void)a6;
    if (old_limit) return sys_getrlimit_impl(resource, old_limit, 0, 0, 0, 0);
    return 0;
}

/* ===== clock_getres / clock_settime / clock_gettime ===== */
static long sys_clock_getres_impl(long clk_id, long res, long a3, long a4, long a5, long a6)
{
    (void)clk_id; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!res) return KENUX_ERR(KENUX_EFAULT);
    struct { long tv_sec; long tv_nsec; } *r = (void*)res;
    r->tv_sec = 0;
    r->tv_nsec = 1;
    return 0;
}

static long sys_clock_settime_impl(long clk_id, long tp, long a3, long a4, long a5, long a6)
{
    (void)clk_id; (void)tp; (void)a3; (void)a4; (void)a5; (void)a6;
    if (current_process >= PROCESS_MAX) return KENUX_ERR(KENUX_EPERM);
    if (proc_euid[current_process] != 0) return KENUX_ERR(KENUX_EPERM);
    return 0;
}

static long sys_clock_gettime_impl(long clk_id, long tp, long a3, long a4, long a5, long a6)
{
    (void)clk_id; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!tp) return KENUX_ERR(KENUX_EFAULT);
    return sys_gettimeofday_impl(tp, 0, 0, 0, 0, 0);
}

/* ===== clock_nanosleep ===== */
static long sys_clock_nanosleep_impl(long clk_id, long flags, long rqtp, long rmtp, long a5, long a6)
{
    (void)clk_id; (void)flags; (void)a5; (void)a6;
    return sys_nanosleep_impl(rqtp, rmtp, 0, 0, 0, 0);
}

/* ===== mincore ===== */
static long sys_mincore_impl(long start, long len, long vec, long a4, long a5, long a6)
{
    (void)start; (void)len; (void)vec; (void)a4; (void)a5; (void)a6;
    return 0;
}

/* ===== madvise (already exists, just ensure) ===== */

/* ===== process_vm_readv / process_vm_writev ===== */
static long sys_process_vm_readv_impl(long pid, long lvec, long liovcnt, long rvec, long riovcnt, long flags)
{
    (void)pid; (void)lvec; (void)liovcnt; (void)rvec; (void)riovcnt; (void)flags;
    return KENUX_ERR(KENUX_ENOSYS);
}

static long sys_process_vm_writev_impl(long pid, long lvec, long liovcnt, long rvec, long riovcnt, long flags)
{
    (void)pid; (void)lvec; (void)liovcnt; (void)rvec; (void)riovcnt; (void)flags;
    return KENUX_ERR(KENUX_ENOSYS);
}

/* ===== kcmp ===== */
static long sys_kcmp_impl(long pid1, long pid2, long type, long idx1, long idx2, long a6)
{
    (void)pid1; (void)pid2; (void)type; (void)idx1; (void)idx2; (void)a6;
    return 0;
}

/* ===== getcpu ===== */
static long sys_getcpu_impl(long cpu, long node, long tcache, long a4, long a5, long a6)
{
    (void)node; (void)tcache; (void)a4; (void)a5; (void)a6;
    if (cpu) *(unsigned int*)cpu = 0;
    return 0;
}

int kapi_syscall_register(int nr, kapi_syscall_fn_t fn)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_COUNT) return -1;
    syscall_table[nr] = fn;
    return 0;
}

int kapi_syscall_unregister(int nr)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_COUNT) return -1;
    syscall_table[nr] = NULL;
    return 0;
}

long kapi_syscall_dispatch(int nr, long a1, long a2, long a3, long a4, long a5, long a6)
{
    if (nr < 0 || nr >= KAPI_SYSCALL_COUNT) return KENUX_ERR(KENUX_ENOSYS);
    if (!syscall_table[nr]) return KENUX_ERR(KENUX_ENOSYS);
    return syscall_table[nr](a1, a2, a3, a4, a5, a6);
}

int kapi_syscall_init(void)
{
    static int initialized = 0;
    if (initialized) {
        return 0;
    }

    memset(syscall_table, 0, sizeof(syscall_table));
    memset(syscall_names, 0, sizeof(syscall_names));
    memset(proc_root, 0, sizeof(proc_root));
    for (int i = 0; i < PROCESS_MAX; i++) {
        proc_root[i][0] = '/';
    }

#define REG(nr, fn) do { kapi_syscall_register(nr, fn); syscall_names[nr] = #fn; } while(0)

    REG(SYS_read,            sys_read_impl);
    REG(SYS_write,           sys_write_impl);
    REG(SYS_open,            sys_open_impl);
    REG(SYS_close,           sys_close_impl);
    REG(SYS_stat,            sys_stat_impl);
    REG(SYS_fstat,           sys_fstat_impl);
    REG(SYS_lseek,           sys_lseek_impl);
    REG(SYS_mmap,            sys_mmap_impl);
    REG(SYS_mprotect,        sys_mprotect_impl);
    REG(SYS_munmap,          sys_munmap_impl);
    REG(SYS_brk,             sys_brk_impl);
    REG(SYS_rt_sigaction,    sys_rt_sigaction_impl);
    REG(SYS_rt_sigprocmask,  sys_rt_sigprocmask_impl);
    REG(SYS_ioctl,           sys_ioctl_impl);
    REG(SYS_access,          sys_access_impl);
    REG(SYS_pipe,            sys_pipe_impl);
    REG(SYS_sched_yield,     sys_sched_yield_impl);
    REG(SYS_dup,             sys_dup_impl);
    REG(SYS_dup2,            sys_dup2_impl);
    REG(SYS_nanosleep,       sys_nanosleep_impl);
    REG(SYS_getpid,          sys_getpid_impl);
    REG(SYS_socket,          sys_socket_impl);
    REG(SYS_clone,           sys_clone_impl);
    REG(SYS_fork,            sys_fork_impl);
    REG(SYS_vfork,           sys_vfork_impl);
    REG(SYS_execve,          sys_execve_impl);
    REG(SYS_exit,            sys_exit_impl);
    REG(SYS_wait4,           sys_wait4_impl);
    REG(SYS_kill,            sys_kill_impl);
    REG(SYS_uname,           sys_uname_impl);
    REG(SYS_fcntl,           sys_fcntl_impl);
    REG(SYS_getcwd,          sys_getcwd_impl);
    REG(SYS_chdir,           sys_chdir_impl);
    REG(SYS_rename,          sys_rename_impl);
    REG(SYS_mkdir,           sys_mkdir_impl);
    REG(SYS_rmdir,           sys_rmdir_impl);
    REG(SYS_creat,           sys_creat_impl);
    REG(SYS_link,            sys_link_impl);
    REG(SYS_unlink,          sys_unlink_impl);
    REG(SYS_symlink,         sys_symlink_impl);
    REG(SYS_readlink,        sys_readlink_impl);
    REG(SYS_chmod,           sys_chmod_impl);
    REG(SYS_chown,           sys_chown_impl);
    REG(SYS_umask,           sys_umask_impl);
    REG(SYS_gettimeofday,    sys_gettimeofday_impl);
    REG(SYS_getrlimit,       sys_getrlimit_impl);
    REG(SYS_getrusage,       sys_getrusage_impl);
    REG(SYS_sysinfo,         sys_sysinfo_impl);
    REG(SYS_times,           sys_times_impl);
    REG(SYS_getuid,          sys_getuid_impl);
    REG(SYS_getgid,          sys_getgid_impl);
    REG(SYS_setuid,          sys_setuid_impl);
    REG(SYS_geteuid,         sys_geteuid_impl);
    REG(SYS_getegid,         sys_getegid_impl);
    REG(SYS_getppid,         sys_getppid_impl);
    REG(SYS_getpgrp,         sys_getpgrp_impl);
    REG(SYS_gettid,          sys_gettid_impl);
    REG(SYS_getdents64,      sys_getdents64_impl);
    REG(SYS_clock_gettime,   sys_clock_gettime_impl);
    REG(SYS_exit_group,      sys_exit_group_impl);
    REG(SYS_openat,          sys_openat_impl);
    REG(SYS_newfstatat,      sys_newfstatat_impl);
    REG(SYS_unlinkat,        sys_unlinkat_impl);
    REG(SYS_sync,            sys_sync_impl);
    REG(SYS_chroot,          sys_chroot_impl);
    REG(SYS_sethostname,     sys_sethostname_impl);
    REG(SYS_init_module,     sys_init_module_impl);
    REG(SYS_delete_module,   sys_delete_module_impl);
    REG(SYS_getrandom,       sys_getrandom_impl);
    REG(SYS_kenux_info,      sys_kenux_info_impl);
    REG(SYS_kenux_get_loadavg, sys_kenux_get_loadavg_impl);

    /* 补充 POSIX 系统调用 */
    REG(SYS_futex,           sys_futex_impl);
    REG(SYS_prctl,           sys_prctl_impl);
    REG(SYS_arch_prctl,      sys_arch_prctl_impl);
    REG(SYS_setpgid,         sys_setpgid_impl);
    REG(SYS_setsid,          sys_setsid_impl);
    REG(SYS_getpgid,         sys_getpgid_impl);
    REG(SYS_getsid,          sys_getsid_impl);
    REG(SYS_setgid,          sys_setgid_impl);
    REG(SYS_setreuid,        sys_setreuid_impl);
    REG(SYS_setregid,        sys_setregid_impl);
    REG(SYS_getpriority,     sys_getpriority_impl);
    REG(SYS_setpriority,     sys_setpriority_impl);
    REG(SYS_mlock,           sys_mlock_impl);
    REG(SYS_munlock,         sys_munlock_impl);
    REG(SYS_mlockall,        sys_mlockall_impl);
    REG(SYS_munlockall,      sys_munlockall_impl);
    REG(SYS_truncate,        sys_truncate_impl);
    REG(SYS_ftruncate,       sys_ftruncate_impl);
    REG(SYS_fsync,           sys_fsync_impl);
    REG(SYS_fdatasync,       sys_fdatasync_impl);
    REG(SYS_flock,           sys_flock_impl);
    REG(SYS_mremap,          sys_mremap_impl);
    REG(SYS_msync,           sys_msync_impl);
    REG(SYS_madvise,         sys_madvise_impl);
    REG(SYS_pread64,         sys_pread64_impl);
    REG(SYS_pwrite64,        sys_pwrite64_impl);
    REG(SYS_sendfile,        sys_sendfile_impl);
    REG(SYS_time,            sys_time_impl);
    REG(SYS_tkill,           sys_tkill_impl);
    REG(SYS_tgkill,          sys_tgkill_impl);
    REG(SYS_set_tid_address, sys_set_tid_address_impl);
    REG(SYS_restart_syscall, sys_restart_syscall_impl);

#undef REG
    initialized = 1;
    return 0;
}