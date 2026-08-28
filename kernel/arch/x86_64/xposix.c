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

long xposix_sys_epoll_create1(int flags)
{
    (void)flags;
    int fd = xposix_alloc_fd(xposix_get_state());
    if (fd < 0) return -XPOSIX_EMFILE;
    return fd;
}

long xposix_sys_epoll_ctl(int epfd, int op, int fd, void* event)
{
    (void)epfd; (void)op; (void)fd; (void)event;
    return 0;
}

long xposix_sys_epoll_wait(int epfd, void* events, int maxevents, int timeout)
{
    (void)epfd; (void)events; (void)maxevents; (void)timeout;
    return 0;
}

long xposix_sys_futex(void* uaddr, int op, int val, const void* timeout,
                       void* uaddr2, int val3)
{
    (void)uaddr; (void)op; (void)val; (void)timeout; (void)uaddr2; (void)val3;
    return 0;
}

long xposix_sys_mq_open(const char* name, int oflag, int mode, void* attr)
{
    (void)name; (void)oflag; (void)mode; (void)attr;
    return 3;
}

long xposix_sys_mq_close(int mqdes)
{
    (void)mqdes;
    return 0;
}

long xposix_sys_mq_send(int mqdes, const char* msg, size_t len, unsigned prio)
{
    (void)mqdes; (void)msg; (void)len; (void)prio;
    return 0;
}

long xposix_sys_mq_receive(int mqdes, char* msg, size_t len, unsigned* prio)
{
    (void)mqdes; (void)msg; (void)len; (void)prio;
    return 0;
}

long xposix_sys_sem_init(void* sem, int pshared, unsigned value)
{
    (void)sem; (void)pshared; (void)value;
    return 0;
}

long xposix_sys_sem_wait(void* sem)
{
    (void)sem;
    return 0;
}

long xposix_sys_sem_post(void* sem)
{
    (void)sem;
    return 0;
}

long xposix_sys_shm_open(const char* name, int oflag, int mode)
{
    (void)name; (void)oflag; (void)mode;
    return 3;
}

long xposix_sys_shm_unlink(const char* name)
{
    (void)name;
    return 0;
}

long xposix_sys_shmget(int key, size_t size, int shmflg)
{
    (void)key; (void)size; (void)shmflg;
    return 1;
}

long xposix_sys_shmat(int shmid, const void* addr, int shmflg)
{
    (void)shmid; (void)addr; (void)shmflg;
    void* p = memory_alloc(4096);
    return p ? (long)p : -XPOSIX_ENOMEM;
}

long xposix_sys_shmdt(const void* addr)
{
    (void)addr;
    return 0;
}

long xposix_sys_shmctl(int shmid, int cmd, void* buf)
{
    (void)shmid; (void)cmd; (void)buf;
    return 0;
}

long xposix_sys_msgget(int key, int msgflg)
{
    (void)key; (void)msgflg;
    return 1;
}

long xposix_sys_msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg)
{
    (void)msqid; (void)msgp; (void)msgsz; (void)msgflg;
    return 0;
}

long xposix_sys_msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg)
{
    (void)msqid; (void)msgp; (void)msgsz; (void)msgtyp; (void)msgflg;
    return 0;
}

long xposix_sys_msgctl(int msqid, int cmd, void* buf)
{
    (void)msqid; (void)cmd; (void)buf;
    return 0;
}

long xposix_sys_semget(int key, int nsems, int semflg)
{
    (void)key; (void)nsems; (void)semflg;
    return 1;
}

long xposix_sys_semop(int semid, void* sops, size_t nsops)
{
    (void)semid; (void)sops; (void)nsops;
    return 0;
}

long xposix_sys_semctl(int semid, int semnum, int cmd, void* arg)
{
    (void)semid; (void)semnum; (void)cmd; (void)arg;
    return 0;
}

long xposix_sys_clock_gettime(int clk, void* tp)
{
    (void)clk; (void)tp;
    return 0;
}

long xposix_sys_clock_settime(int clk, const void* tp)
{
    (void)clk; (void)tp;
    return 0;
}

long xposix_sys_clock_nanosleep(int clk, int flags, const void* req, void* rem)
{
    (void)clk; (void)flags; (void)req; (void)rem;
    return 0;
}

long xposix_sys_timer_create(int clk, void* sevp, void* timerid)
{
    (void)clk; (void)sevp; (void)timerid;
    return 0;
}

long xposix_sys_timer_delete(void* timerid)
{
    (void)timerid;
    return 0;
}

long xposix_sys_timer_settime(void* timerid, int flags, const void* val, void* oval)
{
    (void)timerid; (void)flags; (void)val; (void)oval;
    return 0;
}

long xposix_sys_signalfd(int fd, const void* mask, size_t sizemask)
{
    (void)fd; (void)mask; (void)sizemask;
    return 3;
}

long xposix_sys_timerfd_create(int clk, int flags)
{
    (void)clk; (void)flags;
    return 3;
}

long xposix_sys_timerfd_settime(int fd, int flags, const void* val, void* oval)
{
    (void)fd; (void)flags; (void)val; (void)oval;
    return 0;
}

long xposix_sys_eventfd(unsigned int initval, int flags)
{
    (void)initval; (void)flags;
    return 3;
}

long xposix_sys_inotify_init1(int flags)
{
    (void)flags;
    return 3;
}

long xposix_sys_inotify_add_watch(int fd, const char* path, uint32_t mask)
{
    (void)fd; (void)path; (void)mask;
    return 1;
}

long xposix_sys_inotify_rm_watch(int fd, int wd)
{
    (void)fd; (void)wd;
    return 0;
}

long xposix_sys_getdents64(int fd, void* dirp, size_t count)
{
    (void)fd; (void)dirp; (void)count;
    return 0;
}

long xposix_sys_prlimit64(int pid, int resource, const void* newlim, void* oldlim)
{
    (void)pid; (void)resource; (void)newlim; (void)oldlim;
    return 0;
}

long xposix_sys_setrlimit(int resource, const void* rlim)
{
    (void)resource; (void)rlim;
    return 0;
}

long xposix_sys_getrlimit(int resource, void* rlim)
{
    (void)resource; (void)rlim;
    return 0;
}

long xposix_sys_getrusage(int who, void* usage)
{
    (void)who; (void)usage;
    return 0;
}

long xposix_sys_umask(int mask)
{
    xposix_process_state_t* state = xposix_get_state();
    if (!state) return -XPOSIX_EPERM;
    int old = state->umask;
    state->umask = mask;
    return old;
}

long xposix_sys_chown(const char* path, int uid, int gid)
{
    (void)path; (void)uid; (void)gid;
    return 0;
}

long xposix_sys_fchown(int fd, int uid, int gid)
{
    (void)fd; (void)uid; (void)gid;
    return 0;
}

long xposix_sys_lchown(const char* path, int uid, int gid)
{
    (void)path; (void)uid; (void)gid;
    return 0;
}

long xposix_sys_link(const char* oldpath, const char* newpath)
{
    (void)oldpath; (void)newpath;
    return 0;
}

long xposix_sys_unlinkat(int dirfd, const char* path, int flags)
{
    (void)dirfd; (void)path; (void)flags;
    return 0;
}

long xposix_sys_symlink(const char* target, const char* linkpath)
{
    (void)target; (void)linkpath;
    return 0;
}

long xposix_sys_readlink(const char* path, char* buf, size_t bufsiz)
{
    (void)path; (void)buf; (void)bufsiz;
    return -XPOSIX_EINVAL;
}

long xposix_sys_truncate(const char* path, long length)
{
    (void)path; (void)length;
    return 0;
}

long xposix_sys_ftruncate(int fd, long length)
{
    (void)fd; (void)length;
    return 0;
}

long xposix_sys_sync(void)
{
    return 0;
}

long xposix_sys_fsync(int fd)
{
    (void)fd;
    return 0;
}

long xposix_sys_fdatasync(int fd)
{
    (void)fd;
    return 0;
}

long xposix_sys_madvise(void* addr, size_t length, int advice)
{
    (void)addr; (void)length; (void)advice;
    return 0;
}

long xposix_sys_mincore(void* addr, size_t length, void* vec)
{
    (void)addr; (void)length; (void)vec;
    return 0;
}

long xposix_sys_readdir(int fd, void* dirp, unsigned int count)
{
    (void)fd; (void)dirp; (void)count;
    return 0;
}

long xposix_sys_access(const char* pathname, int mode)
{
    (void)pathname; (void)mode;
    return 0;
}

long xposix_sys_faccessat(int dirfd, const char* pathname, int mode, int flags)
{
    (void)dirfd; (void)pathname; (void)mode; (void)flags;
    return 0;
}

long xposix_sys_readv(int fd, const void* iov, int iovcnt)
{
    (void)fd; (void)iov; (void)iovcnt;
    return 0;
}

long xposix_sys_writev(int fd, const void* iov, int iovcnt)
{
    (void)fd; (void)iov; (void)iovcnt;
    return 0;
}

long xposix_sys_pread64(int fd, void* buf, size_t count, long offset)
{
    (void)fd; (void)buf; (void)count; (void)offset;
    return 0;
}

long xposix_sys_pwrite64(int fd, const void* buf, size_t count, long offset)
{
    (void)fd; (void)buf; (void)count; (void)offset;
    return (long)count;
}

long xposix_sys_sendfile(int out_fd, int in_fd, long* offset, size_t count)
{
    (void)out_fd; (void)in_fd; (void)offset; (void)count;
    return 0;
}

long xposix_sys_splice(int fd_in, long* off_in, int fd_out, long* off_out,
                       size_t len, unsigned int flags)
{
    (void)fd_in; (void)off_in; (void)fd_out; (void)off_out; (void)len; (void)flags;
    return 0;
}

long xposix_sys_tee(int fd_in, int fd_out, size_t len, unsigned int flags)
{
    (void)fd_in; (void)fd_out; (void)len; (void)flags;
    return 0;
}

long xposix_sys_getgroups(int size, void* list)
{
    (void)size; (void)list;
    return 0;
}

long xposix_sys_setgroups(int size, const void* list)
{
    (void)size; (void)list;
    return 0;
}

long xposix_sys_getresuid(void* ruid, void* euid, void* suid)
{
    (void)ruid; (void)euid; (void)suid;
    return 0;
}

long xposix_sys_setresuid(int ruid, int euid, int suid)
{
    (void)ruid; (void)euid; (void)suid;
    return 0;
}

long xposix_sys_getresgid(void* rgid, void* egid, void* sgid)
{
    (void)rgid; (void)egid; (void)sgid;
    return 0;
}

long xposix_sys_setresgid(int rgid, int egid, int sgid)
{
    (void)rgid; (void)egid; (void)sgid;
    return 0;
}

long xposix_sys_setpgid(int pid, int pgid)
{
    (void)pid; (void)pgid;
    return 0;
}

long xposix_sys_getpgid(int pid)
{
    (void)pid;
    return 0;
}

long xposix_sys_getsid(int pid)
{
    (void)pid;
    return 0;
}

long xposix_sys_setsid(void)
{
    return (long)process_get_current_id();
}

long xposix_sys_wait4(int pid, int* status, int options, void* rusage)
{
    (void)pid; (void)status; (void)options; (void)rusage;
    return -XPOSIX_ECHILD;
}

long xposix_sys_clone(unsigned long flags, void* child_stack,
                       int* ptid, int* ctid, unsigned long newtls)
{
    (void)flags; (void)child_stack; (void)ptid; (void)ctid; (void)newtls;
    return (long)process_fork();
}

long xposix_sys_uname(void* buf)
{
    if (!buf) return -XPOSIX_EFAULT;
    const char* sysname = "KenuxOS";
    const char* release = "1.0.0";
    const char* version = "KenuxOS 1.0.0 x86_64";
    const char* machine = "x86_64";
    char* out = (char*)buf;
    memset(out, 0, 5 * 65);
    for (int i = 0; i < 65 && sysname[i]; i++) out[i] = sysname[i];
    out += 65;
    for (int i = 0; i < 65 && release[i]; i++) out[i] = release[i];
    out += 65;
    for (int i = 0; i < 65 && version[i]; i++) out[i] = version[i];
    out += 65;
    out += 65;
    for (int i = 0; i < 65 && machine[i]; i++) out[i] = machine[i];
    return 0;
}

long xposix_sys_sethostname(const char* name, size_t len)
{
    (void)name; (void)len;
    return 0;
}

long xposix_sys_gethostname(char* name, size_t len)
{
    if (!name || len < 8) return -XPOSIX_EFAULT;
    const char* host = "kenux";
    size_t hlen = 5;
    if (hlen + 1 > len) hlen = len - 1;
    memcpy(name, host, hlen);
    name[hlen] = '\0';
    return 0;
}

long xposix_sys_brk(void* addr)
{
    (void)addr;
    return 0;
}

long xposix_sys_set_tid_address(int* tidptr)
{
    (void)tidptr;
    return (long)process_get_current_id();
}

long xposix_sys_arch_prctl(int code, unsigned long addr)
{
    (void)code; (void)addr;
    return 0;
}

long xposix_sys_rt_sigaction(int signum, const void* act, void* oldact, size_t sigsetsize)
{
    (void)signum; (void)act; (void)oldact; (void)sigsetsize;
    return 0;
}

long xposix_sys_rt_sigprocmask(int how, const void* set, void* oldset, size_t sigsetsize)
{
    (void)how; (void)set; (void)oldset; (void)sigsetsize;
    return 0;
}

long xposix_sys_rt_sigreturn(void)
{
    return 0;
}

long xposix_sys_tgkill(int tgid, int tid, int sig)
{
    (void)tgid; (void)tid; (void)sig;
    return 0;
}

long xposix_sys_tkill(int tid, int sig)
{
    (void)tid; (void)sig;
    return 0;
}

long xposix_sys_ptrace(int request, int pid, void* addr, void* data)
{
    (void)request; (void)pid; (void)addr; (void)data;
    return -XPOSIX_EPERM;
}

long xposix_sys_getrandom(void* buf, size_t count, unsigned int flags)
{
    (void)buf; (void)count; (void)flags;
    return 0;
}

long xposix_sys_memfd_create(const char* name, unsigned int flags)
{
    (void)name; (void)flags;
    int fd = xposix_alloc_fd(xposix_get_state());
    if (fd < 0) return -XPOSIX_EMFILE;
    return fd;
}

long xposix_sys_pipe2(int pipefd[2], int flags)
{
    if (!pipefd) return -XPOSIX_EFAULT;
    (void)flags;
    pipefd[0] = xposix_alloc_fd(xposix_get_state());
    pipefd[1] = xposix_alloc_fd(xposix_get_state());
    if (pipefd[0] < 0 || pipefd[1] < 0) return -XPOSIX_EMFILE;
    return 0;
}

long xposix_sys_dup3(int oldfd, int newfd, int flags)
{
    (void)flags;
    return xposix_sys_dup2(oldfd, newfd);
}

long xposix_sys_openat(int dirfd, const char* pathname, int flags, int mode)
{
    (void)dirfd;
    return xposix_sys_open(pathname, flags, mode);
}

long xposix_sys_fstatat(int dirfd, const char* pathname, void* statbuf, int flags)
{
    (void)dirfd; (void)pathname; (void)statbuf; (void)flags;
    return 0;
}

long xposix_sys_mkdirat(int dirfd, const char* pathname, int mode)
{
    (void)dirfd;
    return xposix_sys_mkdir(pathname, mode);
}

long xposix_sys_readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz)
{
    (void)dirfd; (void)pathname; (void)buf; (void)bufsiz;
    return -XPOSIX_EINVAL;
}

long xposix_sys_newfstatat(int dirfd, const char* pathname, void* statbuf, int flags)
{
    (void)dirfd; (void)pathname; (void)statbuf; (void)flags;
    return 0;
}

long xposix_sys_renameat(int olddirfd, const char* oldpath,
                         int newdirfd, const char* newpath)
{
    (void)olddirfd; (void)newdirfd;
    return xposix_sys_rename(oldpath, newpath);
}

long xposix_sys_fchmod(int fd, int mode)
{
    (void)fd; (void)mode;
    return 0;
}

long xposix_sys_fchdir(int fd)
{
    (void)fd;
    return 0;
}

long xposix_sys_getdents(int fd, void* dirp, unsigned int count)
{
    (void)fd; (void)dirp; (void)count;
    return 0;
}

long xposix_sys_socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)domain; (void)type; (void)protocol;
    if (!sv) return -XPOSIX_EFAULT;
    sv[0] = 3;
    sv[1] = 4;
    return 0;
}

long xposix_sys_sendto(int sockfd, const void* buf, size_t len, int flags,
                       const void* dest_addr, size_t addrlen)
{
    (void)sockfd; (void)buf; (void)flags; (void)dest_addr; (void)addrlen;
    return (long)len;
}

long xposix_sys_recvfrom(int sockfd, void* buf, size_t len, int flags,
                         void* src_addr, size_t* addrlen)
{
    (void)sockfd; (void)buf; (void)len; (void)flags; (void)src_addr; (void)addrlen;
    return 0;
}

long xposix_sys_setsockopt(int sockfd, int level, int optname,
                           const void* optval, size_t optlen)
{
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

long xposix_sys_getsockopt(int sockfd, int level, int optname,
                           void* optval, size_t* optlen)
{
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

long xposix_sys_shutdown(int sockfd, int how)
{
    (void)sockfd; (void)how;
    return 0;
}

long xposix_sys_sched_setaffinity(int pid, size_t cpusetsize, const void* mask)
{
    (void)pid; (void)cpusetsize; (void)mask;
    return 0;
}

long xposix_sys_sched_getaffinity(int pid, size_t cpusetsize, void* mask)
{
    (void)pid; (void)cpusetsize; (void)mask;
    return 0;
}

long xposix_sys_sched_setparam(int pid, const void* param)
{
    (void)pid; (void)param;
    return 0;
}

long xposix_sys_sched_getparam(int pid, void* param)
{
    (void)pid; (void)param;
    return 0;
}

long xposix_sys_sched_setscheduler(int pid, int policy, const void* param)
{
    (void)pid; (void)policy; (void)param;
    return 0;
}

long xposix_sys_sched_getscheduler(int pid)
{
    (void)pid;
    return 0;
}

long xposix_sys_sched_get_priority_max(int policy)
{
    (void)policy;
    return 99;
}

long xposix_sys_sched_get_priority_min(int policy)
{
    (void)policy;
    return 1;
}

long xposix_sys_ioprio_set(int which, int who, int ioprio)
{
    (void)which; (void)who; (void)ioprio;
    return 0;
}

long xposix_sys_ioprio_get(int which, int who)
{
    (void)which; (void)who;
    return 0;
}