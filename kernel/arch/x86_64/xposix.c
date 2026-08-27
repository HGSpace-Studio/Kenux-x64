#include <arch/syscall.h>
#include <arch/process.h>
#include <arch/fs.h>
#include <arch/memory.h>
#include <string.h>

#define XPOSIX_OK          0
#define XPOSIX_EPERM      1
#define XPOSIX_ENOENT     2
#define XPOSIX_ESRCH      3
#define XPOSIX_EINTR      4
#define XPOSIX_EIO        5
#define XPOSIX_ENXIO      6
#define XPOSIX_E2BIG      7
#define XPOSIX_ENOEXEC    8
#define XPOSIX_EBADF      9
#define XPOSIX_ECHILD     10
#define XPOSIX_EAGAIN     11
#define XPOSIX_ENOMEM     12
#define XPOSIX_EACCES     13
#define XPOSIX_EFAULT     14
#define XPOSIX_ENOTBLK    15
#define XPOSIX_EBUSY      16
#define XPOSIX_EEXIST     17
#define XPOSIX_EXDEV      18
#define XPOSIX_ENODEV     19
#define XPOSIX_ENOTDIR    20
#define XPOSIX_EISDIR     21
#define XPOSIX_EINVAL     22
#define XPOSIX_ENFILE     23
#define XPOSIX_EMFILE     24
#define XPOSIX_ENOTTY     25
#define XPOSIX_ETXTBSY    26
#define XPOSIX_EFBIG      27
#define XPOSIX_ENOSPC     28
#define XPOSIX_ESPIPE     29
#define XPOSIX_EROFS      30
#define XPOSIX_EMLINK     31
#define XPOSIX_EPIPE      32
#define XPOSIX_EDOM       33
#define XPOSIX_ERANGE     34

#define XPOSIX_O_RDONLY   0x0000
#define XPOSIX_O_WRONLY   0x0001
#define XPOSIX_O_RDWR     0x0002
#define XPOSIX_O_CREAT    0x0100
#define XPOSIX_O_TRUNC    0x0200
#define XPOSIX_O_APPEND   0x0400
#define XPOSIX_O_NONBLOCK 0x0800
#define XPOSIX_O_EXCL     0x2000

#define XPOSIX_SEEK_SET   0
#define XPOSIX_SEEK_CUR   1
#define XPOSIX_SEEK_END   2

#define XPOSIX_STDIN_FD   0
#define XPOSIX_STDOUT_FD  1
#define XPOSIX_STDERR_FD  2
#define XPOSIX_MAX_FDS    1024

typedef struct {
    vfs_node_t* node;
    uint64_t offset;
    uint32_t flags;
    uint16_t mode;
} xposix_fd_t;

typedef struct {
    xposix_fd_t fds[XPOSIX_MAX_FDS];
    uint8_t     fd_used[XPOSIX_MAX_FDS];
    char        cwd[256];
    uint64_t    owner_pid;
    int         umask;
} xposix_process_state_t;

static xposix_process_state_t xposix_states[PROCESS_MAX];
static spinlock_t xposix_lock = SPINLOCK_INIT;

void xposix_init(void)
{
    memset(xposix_states, 0, sizeof(xposix_states));
    for (int i = 0; i < PROCESS_MAX; i++) {
        xposix_states[i].cwd[0] = '/';
        xposix_states[i].umask = 0022;
        xposix_states[i].owner_pid = i;
    }
}

static xposix_process_state_t* xposix_get_state(void)
{
    uint64_t pid = process_get_current_id();
    if (pid >= PROCESS_MAX) return NULL;
    return &xposix_states[pid];
}

static int xposix_alloc_fd(xposix_process_state_t* state)
{
    for (int i = 0; i < XPOSIX_MAX_FDS; i++) {
        if (!state->fd_used[i]) {
            state->fd_used[i] = 1;
            return i;
        }
    }
    return -1;
}

long xposix_sys_open(const char* path, int flags, int mode)
{
    if (!path) return -XPOSIX_EFAULT;

    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;

    int fd = xposix_alloc_fd(state);
    if (fd < 0) return -XPOSIX_EMFILE;

    state->fds[fd].offset = 0;
    state->fds[fd].flags = flags;
    state->fds[fd].mode = mode;
    state->fds[fd].node = NULL;

    return fd;
}

long xposix_sys_close(int fd)
{
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (fd < 0 || fd >= XPOSIX_MAX_FDS || !state->fd_used[fd]) return -XPOSIX_EBADF;

    state->fd_used[fd] = 0;
    state->fds[fd].node = NULL;
    return 0;
}

long xposix_sys_read(int fd, void* buf, size_t count)
{
    if (!buf) return -XPOSIX_EFAULT;

    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (fd < 0 || fd >= XPOSIX_MAX_FDS || !state->fd_used[fd]) return -XPOSIX_EBADF;

    if (fd == XPOSIX_STDIN_FD) return 0;

    xposix_fd_t* f = &state->fds[fd];
    if (f->node && f->node->ops.read) {
        int ret = f->node->ops.read(f->node, f->offset, buf, count);
        if (ret > 0) f->offset += ret;
        return ret;
    }

    return 0;
}

long xposix_sys_write(int fd, const void* buf, size_t count)
{
    if (!buf) return -XPOSIX_EFAULT;

    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (fd < 0 || fd >= XPOSIX_MAX_FDS || !state->fd_used[fd]) return -XPOSIX_EBADF;

    xposix_fd_t* f = &state->fds[fd];
    if (f->node && f->node->ops.write) {
        int ret = f->node->ops.write(f->node, f->offset, buf, count);
        if (ret > 0) f->offset += ret;
        return ret;
    }

    return (long)count;
}

long xposix_sys_lseek(int fd, long offset, int whence)
{
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (fd < 0 || fd >= XPOSIX_MAX_FDS || !state->fd_used[fd]) return -XPOSIX_EBADF;

    xposix_fd_t* f = &state->fds[fd];

    switch (whence) {
    case XPOSIX_SEEK_SET:
        f->offset = offset;
        break;
    case XPOSIX_SEEK_CUR:
        f->offset += offset;
        break;
    case XPOSIX_SEEK_END:
        if (f->node) f->offset = f->node->size + offset;
        else f->offset = offset;
        break;
    default:
        return -XPOSIX_EINVAL;
    }

    return (long)f->offset;
}

long xposix_sys_ioctl(int fd, unsigned long request, void* arg)
{
    (void)request; (void)arg;
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (fd < 0 || fd >= XPOSIX_MAX_FDS || !state->fd_used[fd]) return -XPOSIX_EBADF;
    return -XPOSIX_ENOTTY;
}

long xposix_sys_mmap(void* addr, size_t length, int prot, int flags,
                     int fd, long offset)
{
    (void)addr; (void)prot; (void)flags; (void)offset;
    if (length == 0) return -XPOSIX_EINVAL;

    void* mem = memory_alloc(length);
    if (!mem) return -XPOSIX_ENOMEM;

    if (fd >= 0) {
        xposix_sys_read(fd, mem, length);
    }

    return (long)mem;
}

long xposix_sys_munmap(void* addr, size_t length)
{
    (void)length;
    if (!addr) return -XPOSIX_EINVAL;
    memory_free(addr);
    return 0;
}

long xposix_sys_getpid(void)
{
    return (long)process_get_current_id();
}

long xposix_sys_getppid(void)
{
    uint64_t pid = process_get_current_id();
    process_t* proc = process_get_by_id(pid);
    if (!proc) return 0;
    return (long)proc->parent_id;
}

long xposix_sys_getuid(void) { return 0; }
long xposix_sys_getgid(void) { return 0; }
long xposix_sys_geteuid(void) { return 0; }
long xposix_sys_getegid(void) { return 0; }

long xposix_sys_setuid(int uid) { (void)uid; return 0; }
long xposix_sys_setgid(int gid) { (void)gid; return 0; }

long xposix_sys_chdir(const char* path)
{
    if (!path) return -XPOSIX_EFAULT;
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    strncpy(state->cwd, path, 255);
    state->cwd[255] = '\0';
    return 0;
}

long xposix_sys_getcwd(char* buf, size_t size)
{
    if (!buf) return -XPOSIX_EFAULT;
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    size_t len = strlen(state->cwd);
    if (len + 1 > size) return -XPOSIX_ERANGE;
    memcpy(buf, state->cwd, len + 1);
    return (long)len;
}

long xposix_sys_dup(int oldfd)
{
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (oldfd < 0 || oldfd >= XPOSIX_MAX_FDS || !state->fd_used[oldfd]) return -XPOSIX_EBADF;

    int newfd = xposix_alloc_fd(state);
    if (newfd < 0) return -XPOSIX_EMFILE;

    state->fds[newfd] = state->fds[oldfd];
    return newfd;
}

long xposix_sys_dup2(int oldfd, int newfd)
{
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    if (oldfd < 0 || oldfd >= XPOSIX_MAX_FDS || !state->fd_used[oldfd]) return -XPOSIX_EBADF;
    if (newfd < 0 || newfd >= XPOSIX_MAX_FDS) return -XPOSIX_EBADF;

    if (state->fd_used[newfd]) xposix_sys_close(newfd);

    state->fds[newfd] = state->fds[oldfd];
    state->fd_used[newfd] = 1;
    return newfd;
}

long xposix_sys_pipe(int pipefd[2])
{
    if (!pipefd) return -XPOSIX_EFAULT;
    pipefd[0] = 3;
    pipefd[1] = 4;
    return 0;
}

long xposix_sys_fork(void)
{
    return (long)process_fork();
}

long xposix_sys_execve(const char* path, char* const argv[], char* const envp[])
{
    if (!path) return -XPOSIX_EFAULT;
    (void)argv; (void)envp;
    return -XPOSIX_ENOEXEC;
}

long xposix_sys_waitpid(int pid, int* status, int options)
{
    (void)pid; (void)status; (void)options;
    return -XPOSIX_ECHILD;
}

long xposix_sys_exit(int status)
{
    process_exit((uint64_t)status);
    return 0;
}

long xposix_sys_kill(int pid, int sig)
{
    (void)pid; (void)sig;
    return 0;
}

long xposix_sys_nanosleep(const void* req, void* rem)
{
    (void)req; (void)rem;
    return 0;
}

long xposix_sys_gettimeofday(void* tv, void* tz)
{
    (void)tv; (void)tz;
    return 0;
}

long xposix_sys_mkdir(const char* path, int mode)
{
    (void)path; (void)mode;
    return 0;
}

long xposix_sys_rmdir(const char* path)
{
    (void)path;
    return 0;
}

long xposix_sys_unlink(const char* path)
{
    (void)path;
    return 0;
}

long xposix_sys_rename(const char* oldpath, const char* newpath)
{
    (void)oldpath; (void)newpath;
    return 0;
}

long xposix_sys_stat(const char* path, void* statbuf)
{
    (void)path; (void)statbuf;
    return 0;
}

long xposix_sys_fstat(int fd, void* statbuf)
{
    (void)fd; (void)statbuf;
    return 0;
}

long xposix_sys_chmod(const char* path, int mode)
{
    (void)path; (void)mode;
    return 0;
}

long xposix_sys_fcntl(int fd, int cmd, long arg)
{
    (void)fd; (void)cmd; (void)arg;
    return 0;
}

long xposix_sys_socket(int domain, int type, int protocol)
{
    (void)domain; (void)type; (void)protocol;
    return 3;
}

long xposix_sys_bind(int sockfd, const void* addr, size_t addrlen)
{
    (void)sockfd; (void)addr; (void)addrlen;
    return 0;
}

long xposix_sys_listen(int sockfd, int backlog)
{
    (void)sockfd; (void)backlog;
    return 0;
}

long xposix_sys_accept(int sockfd, void* addr, size_t* addrlen)
{
    (void)sockfd; (void)addr; (void)addrlen;
    return 4;
}

long xposix_sys_connect(int sockfd, const void* addr, size_t addrlen)
{
    (void)sockfd; (void)addr; (void)addrlen;
    return 0;
}

long xposix_sys_send(int sockfd, const void* buf, size_t len, int flags)
{
    (void)sockfd; (void)buf; (void)flags;
    return (long)len;
}

long xposix_sys_recv(int sockfd, void* buf, size_t len, int flags)
{
    (void)sockfd; (void)buf; (void)len; (void)flags;
    return 0;
}

long xposix_sys_select(int nfds, void* readfds, void* writefds, void* exceptfds, void* timeout)
{
    (void)nfds; (void)readfds; (void)writefds; (void)exceptfds; (void)timeout;
    return 0;
}

long xposix_sys_poll(void* fds, int nfds, int timeout)
{
    (void)fds; (void)nfds; (void)timeout;
    return 0;
}

long xposix_sys_mprotect(void* addr, size_t len, int prot)
{
    (void)addr; (void)len; (void)prot;
    return 0;
}

long xposix_sys_sigaction(int signum, const void* act, void* oldact)
{
    (void)signum; (void)act; (void)oldact;
    return 0;
}

long xposix_sys_sigprocmask(int how, const void* set, void* oldset)
{
    (void)how; (void)set; (void)oldset;
    return 0;
}

long xposix_sys_sched_yield(void)
{
    process_yield();
    return 0;
}

long xposix_sys_sysconf(int name)
{
    switch (name) {
    case 1: return 256;
    case 2: return 4096;
    case 3: return (long)PROCESS_MAX;
    case 4: return 1024;
    case 5: return 8;
    case 6: return 1;
    default: return -XPOSIX_EINVAL;
    }
}