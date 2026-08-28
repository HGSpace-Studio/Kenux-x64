#include "linux_syscall.h"
#include <string.h>

long linux_sys_read(int fd, void* buf, uint64_t count) { (void)fd; (void)buf; (void)count; return -1; }
long linux_sys_write(int fd, const void* buf, uint64_t count) { (void)fd; (void)buf; (void)count; return -1; }
long linux_sys_open(const char* path, int flags, int mode) { (void)path; (void)flags; (void)mode; return -1; }
long linux_sys_close(int fd) { (void)fd; return 0; }
long linux_sys_stat(const char* path, linux_stat_t* buf) { (void)path; (void)buf; return -1; }
long linux_sys_fstat(int fd, linux_stat_t* buf) { (void)fd; (void)buf; return -1; }
long linux_sys_lstat(const char* path, linux_stat_t* buf) { (void)path; (void)buf; return -1; }
long linux_sys_poll(void* fds, uint32_t nfds, int timeout) { (void)fds; (void)nfds; (void)timeout; return -1; }
long linux_sys_lseek(int fd, int64_t offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
long linux_sys_mmap(void* addr, uint64_t len, int prot, int flags, int fd, int64_t off) { (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)off; return -1; }
long linux_sys_mprotect(void* addr, uint64_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }
long linux_sys_munmap(void* addr, uint64_t len) { (void)addr; (void)len; return 0; }
long linux_sys_brk(void* addr) { (void)addr; return 0; }
long linux_sys_rt_sigaction(int signum, const void* act, void* oldact, uint64_t sigsetsize) { (void)signum; (void)act; (void)oldact; (void)sigsetsize; return 0; }
long linux_sys_rt_sigprocmask(int how, const void* set, void* oldset, uint64_t sigsetsize) { (void)how; (void)set; (void)oldset; (void)sigsetsize; return 0; }
long linux_sys_ioctl(int fd, uint64_t cmd, void* arg) { (void)fd; (void)cmd; (void)arg; return -1; }
long linux_sys_pread64(int fd, void* buf, uint64_t count, int64_t offset) { (void)fd; (void)buf; (void)count; (void)offset; return -1; }
long linux_sys_pwrite64(int fd, const void* buf, uint64_t count, int64_t offset) { (void)fd; (void)buf; (void)count; (void)offset; return -1; }
long linux_sys_readv(int fd, const void* iov, int iovcnt) { (void)fd; (void)iov; (void)iovcnt; return -1; }
long linux_sys_writev(int fd, const void* iov, int iovcnt) { (void)fd; (void)iov; (void)iovcnt; return -1; }
long linux_sys_access(const char* path, int mode) { (void)path; (void)mode; return -1; }
long linux_sys_pipe(int* pipefd) { (void)pipefd; return -1; }
long linux_sys_select(int nfds, void* readfds, void* writefds, void* exceptfds, void* timeout) { (void)nfds; (void)readfds; (void)writefds; (void)exceptfds; (void)timeout; return -1; }
long linux_sys_sched_yield(void) { return 0; }
long linux_sys_mremap(void* addr, uint64_t old_len, uint64_t new_len, int flags, void* new_addr) { (void)addr; (void)old_len; (void)new_len; (void)flags; (void)new_addr; return -1; }
long linux_sys_msync(void* addr, uint64_t len, int flags) { (void)addr; (void)len; (void)flags; return 0; }
long linux_sys_mincore(void* addr, uint64_t len, void* vec) { (void)addr; (void)len; (void)vec; return -1; }
long linux_sys_madvise(void* addr, uint64_t len, int advice) { (void)addr; (void)len; (void)advice; return 0; }
long linux_sys_shmget(int key, uint64_t size, int flags) { (void)key; (void)size; (void)flags; return -1; }
long linux_sys_shmat(int shmid, const void* addr, int flags) { (void)shmid; (void)addr; (void)flags; return -1; }
long linux_sys_shmctl(int shmid, int cmd, void* buf) { (void)shmid; (void)cmd; (void)buf; return -1; }
long linux_sys_dup(int fd) { (void)fd; return -1; }
long linux_sys_dup2(int oldfd, int newfd) { (void)oldfd; (void)newfd; return -1; }
long linux_sys_pause(void) { return -1; }
long linux_sys_nanosleep(const void* req, void* rem) { (void)req; (void)rem; return 0; }
long linux_sys_getitimer(int which, void* value) { (void)which; (void)value; return -1; }
long linux_sys_setitimer(int which, const void* value, void* ovalue) { (void)which; (void)value; (void)ovalue; return -1; }
long linux_sys_getpid(void) { return 1; }
long linux_sys_sendfile(int out_fd, int in_fd, int64_t* offset, uint64_t count) { (void)out_fd; (void)in_fd; (void)offset; (void)count; return -1; }
long linux_sys_socket(int domain, int type, int protocol) { (void)domain; (void)type; (void)protocol; return -1; }
long linux_sys_connect(int fd, const void* addr, int addrlen) { (void)fd; (void)addr; (void)addrlen; return -1; }
long linux_sys_accept(int fd, void* addr, int* addrlen) { (void)fd; (void)addr; (void)addrlen; return -1; }
long linux_sys_sendto(int fd, const void* buf, uint64_t len, int flags, const void* addr, int addrlen) { (void)fd; (void)buf; (void)len; (void)flags; (void)addr; (void)addrlen; return -1; }
long linux_sys_recvfrom(int fd, void* buf, uint64_t len, int flags, void* addr, int* addrlen) { (void)fd; (void)buf; (void)len; (void)flags; (void)addr; (void)addrlen; return -1; }
long linux_sys_sendmsg(int fd, const void* msg, int flags) { (void)fd; (void)msg; (void)flags; return -1; }
long linux_sys_recvmsg(int fd, void* msg, int flags) { (void)fd; (void)msg; (void)flags; return -1; }
long linux_sys_shutdown(int fd, int how) { (void)fd; (void)how; return -1; }
long linux_sys_bind(int fd, const void* addr, int addrlen) { (void)fd; (void)addr; (void)addrlen; return -1; }
long linux_sys_listen(int fd, int backlog) { (void)fd; (void)backlog; return -1; }
long linux_sys_getsockname(int fd, void* addr, int* addrlen) { (void)fd; (void)addr; (void)addrlen; return -1; }
long linux_sys_getpeername(int fd, void* addr, int* addrlen) { (void)fd; (void)addr; (void)addrlen; return -1; }
long linux_sys_socketpair(int domain, int type, int protocol, int* sv) { (void)domain; (void)type; (void)protocol; (void)sv; return -1; }
long linux_sys_setsockopt(int fd, int level, int optname, const void* optval, int optlen) { (void)fd; (void)level; (void)optname; (void)optval; (void)optlen; return -1; }
long linux_sys_getsockopt(int fd, int level, int optname, void* optval, int* optlen) { (void)fd; (void)level; (void)optname; (void)optval; (void)optlen; return -1; }
long linux_sys_clone(uint64_t flags, void* stack, int* parent_tid, int* child_tid, uint64_t tls) { (void)flags; (void)stack; (void)parent_tid; (void)child_tid; (void)tls; return -1; }
long linux_sys_fork(void) { return -1; }
long linux_sys_vfork(void) { return -1; }
long linux_sys_execve(const char* path, char* const* argv, char* const* envp) { (void)path; (void)argv; (void)envp; return -1; }
long linux_sys_exit(int status) { (void)status; while(1); return 0; }
long linux_sys_wait4(int pid, int* status, int options, void* rusage) { (void)pid; (void)status; (void)options; (void)rusage; return -1; }
long linux_sys_kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
long linux_sys_tkill(int tid, int sig) { (void)tid; (void)sig; return -1; }
long linux_sys_tgkill(int tgid, int tid, int sig) { (void)tgid; (void)tid; (void)sig; return -1; }
long linux_sys_sigaltstack(const linux_stack_t* ss, linux_stack_t* old_ss) { (void)ss; (void)old_ss; return 0; }
long linux_sys_rt_sigsuspend(const void* mask, uint64_t sigsetsize) { (void)mask; (void)sigsetsize; return -1; }
long linux_sys_rt_sigpending(void* set, uint64_t sigsetsize) { (void)set; (void)sigsetsize; return 0; }
long linux_sys_rt_sigtimedwait(const void* set, linux_siginfo_t* info, const void* timeout, uint64_t sigsetsize) { (void)set; (void)info; (void)timeout; (void)sigsetsize; return -1; }
long linux_sys_getuid(void) { return 0; }
long linux_sys_geteuid(void) { return 0; }
long linux_sys_getgid(void) { return 0; }
long linux_sys_getegid(void) { return 0; }
long linux_sys_setuid(uid_t uid) { (void)uid; return 0; }
long linux_sys_setgid(gid_t gid) { (void)gid; return 0; }
long linux_sys_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid) { if(ruid)*ruid=0; if(euid)*euid=0; if(suid)*suid=0; return 0; }
long linux_sys_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid) { if(rgid)*rgid=0; if(egid)*egid=0; if(sgid)*sgid=0; return 0; }
long linux_sys_setresuid(uid_t ruid, uid_t euid, uid_t suid) { (void)ruid; (void)euid; (void)suid; return 0; }
long linux_sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid) { (void)rgid; (void)egid; (void)sgid; return 0; }
long linux_sys_setpgid(int pid, int pgid) { (void)pid; (void)pgid; return 0; }
long linux_sys_getpgid(int pid) { (void)pid; return 1; }
long linux_sys_getppid(void) { return 0; }
long linux_sys_getsid(int pid) { (void)pid; return 1; }
long linux_sys_setsid(void) { return 1; }
long linux_sys_getgroups(int size, gid_t* list) { (void)size; (void)list; return 0; }
long linux_sys_setgroups(int size, const gid_t* list) { (void)size; (void)list; return 0; }
long linux_sys_umask(int mask) { (void)mask; return 0; }
long linux_sys_chmod(const char* path, int mode) { (void)path; (void)mode; return 0; }
long linux_sys_fchmod(int fd, int mode) { (void)fd; (void)mode; return 0; }
long linux_sys_chown(const char* path, uid_t uid, gid_t gid) { (void)path; (void)uid; (void)gid; return 0; }
long linux_sys_fchown(int fd, uid_t uid, gid_t gid) { (void)fd; (void)uid; (void)gid; return 0; }
long linux_sys_lchown(const char* path, uid_t uid, gid_t gid) { (void)path; (void)uid; (void)gid; return 0; }
long linux_sys_chroot(const char* path) { (void)path; return 0; }
long linux_sys_pivot_root(const char* new_root, const char* put_old) { (void)new_root; (void)put_old; return -1; }
long linux_sys_mkdir(const char* path, int mode) { (void)path; (void)mode; return -1; }
long linux_sys_rmdir(const char* path) { (void)path; return -1; }
long linux_sys_unlink(const char* path) { (void)path; return -1; }
long linux_sys_symlink(const char* target, const char* linkpath) { (void)target; (void)linkpath; return -1; }
long linux_sys_readlink(const char* path, char* buf, uint64_t bufsiz) { (void)path; (void)buf; (void)bufsiz; return -1; }
long linux_sys_link(const char* oldpath, const char* newpath) { (void)oldpath; (void)newpath; return -1; }
long linux_sys_rename(const char* oldpath, const char* newpath) { (void)oldpath; (void)newpath; return -1; }
long linux_sys_getcwd(char* buf, uint64_t size) { if(buf&&size>1){buf[0]='/';buf[1]=0;return 1;} return -1; }
long linux_sys_mount(const char* src, const char* tgt, const char* type, uint64_t flags, const void* data) { (void)src; (void)tgt; (void)type; (void)flags; (void)data; return -1; }
long linux_sys_umount2(const char* tgt, int flags) { (void)tgt; (void)flags; return -1; }
long linux_sys_truncate(const char* path, int64_t length) { (void)path; (void)length; return -1; }
long linux_sys_ftruncate(int fd, int64_t length) { (void)fd; (void)length; return -1; }
long linux_sys_sync(void) { return 0; }
long linux_sys_fsync(int fd) { (void)fd; return 0; }
long linux_sys_fdatasync(int fd) { (void)fd; return 0; }
long linux_sys_acct(const char* path) { (void)path; return -1; }
long linux_sys_getdents64(int fd, linux_dirent_t* dirp, uint64_t count) { (void)fd; (void)dirp; (void)count; return -1; }
long linux_sys_fcntl(int fd, int cmd, uint64_t arg) { (void)fd; (void)cmd; (void)arg; return -1; }
long linux_sys_flock(int fd, int cmd) { (void)fd; (void)cmd; return 0; }
long linux_sys_fadvise64(int fd, int64_t offset, int64_t len, int advice) { (void)fd; (void)offset; (void)len; (void)advice; return 0; }
long linux_sys_getrlimit(int resource, void* rlim) { (void)resource; (void)rlim; return -1; }
long linux_sys_setrlimit(int resource, const void* rlim) { (void)resource; (void)rlim; return -1; }
long linux_sys_prlimit64(int pid, int resource, const void* new_limit, void* old_limit) { (void)pid; (void)resource; (void)new_limit; (void)old_limit; return -1; }
long linux_sys_getrusage(int who, linux_rusage_t* usage) { (void)who; (void)usage; return -1; }
long linux_sys_prctl(int option, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) { (void)option; (void)arg2; (void)arg3; (void)arg4; (void)arg5; return -1; }
long linux_sys_gettimeofday(void* tv, void* tz) { (void)tv; (void)tz; return 0; }
long linux_sys_settimeofday(const void* tv, const void* tz) { (void)tv; (void)tz; return -1; }
long linux_sys_clock_gettime(int clk_id, void* tp) { (void)clk_id; (void)tp; return 0; }
long linux_sys_clock_settime(int clk_id, const void* tp) { (void)clk_id; (void)tp; return -1; }
long linux_sys_clock_getres(int clk_id, void* res) { (void)clk_id; (void)res; return 0; }
long linux_sys_clock_nanosleep(int clk_id, int flags, const void* req, void* rem) { (void)clk_id; (void)flags; (void)req; (void)rem; return 0; }
long linux_sys_timer_create(int clockid, const void* sevp, linux_timer_t* timerid) { (void)clockid; (void)sevp; (void)timerid; return -1; }
long linux_sys_timer_settime(linux_timer_t* timerid, int flags, const void* new_value, void* old_value) { (void)timerid; (void)flags; (void)new_value; (void)old_value; return -1; }
long linux_sys_timer_gettime(linux_timer_t* timerid, void* curr_value) { (void)timerid; (void)curr_value; return -1; }
long linux_sys_timer_delete(linux_timer_t* timerid) { (void)timerid; return 0; }
long linux_sys_nice(int inc) { (void)inc; return 0; }
long linux_sys_sched_setparam(int pid, const void* param) { (void)pid; (void)param; return 0; }
long linux_sys_sched_getparam(int pid, void* param) { (void)pid; (void)param; return 0; }
long linux_sys_sched_setscheduler(int pid, int policy, const void* param) { (void)pid; (void)policy; (void)param; return 0; }
long linux_sys_sched_getscheduler(int pid) { (void)pid; return 0; }
long linux_sys_sched_get_priority_max(int policy) { (void)policy; return 99; }
long linux_sys_sched_get_priority_min(int policy) { (void)policy; return 1; }
long linux_sys_sched_setaffinity(int pid, uint64_t cpusetsize, const void* mask) { (void)pid; (void)cpusetsize; (void)mask; return 0; }
long linux_sys_sched_getaffinity(int pid, uint64_t cpusetsize, void* mask) { (void)pid; (void)cpusetsize; (void)mask; return -1; }
long linux_sys_setpriority(int which, int who, int prio) { (void)which; (void)who; (void)prio; return 0; }
long linux_sys_getpriority(int which, int who) { (void)which; (void)who; return 0; }
long linux_sys_mlock(const void* addr, uint64_t len) { (void)addr; (void)len; return 0; }
long linux_sys_munlock(const void* addr, uint64_t len) { (void)addr; (void)len; return 0; }
long linux_sys_mlockall(int flags) { (void)flags; return 0; }
long linux_sys_munlockall(void) { return 0; }
long linux_sys_personality(uint64_t persona) { (void)persona; return 0; }
long linux_sys_uname(void* buf) { (void)buf; return 0; }
long linux_sys_sethostname(const char* name, uint64_t len) { (void)name; (void)len; return 0; }
long linux_sys_setdomainname(const char* name, uint64_t len) { (void)name; (void)len; return 0; }
long linux_sys_getdomainname(char* name, uint64_t len) { (void)name; (void)len; return 0; }
long linux_sys_sysinfo(void* info) { (void)info; return 0; }
long linux_sys_reboot(int magic1, int magic2, int cmd, void* arg) { (void)magic1; (void)magic2; (void)cmd; (void)arg; return -1; }
long linux_sys_init_module(const void* mod_image, uint64_t len, const char* param_values) { (void)mod_image; (void)len; (void)param_values; return -1; }
long linux_sys_delete_module(const char* name, int flags) { (void)name; (void)flags; return -1; }
long linux_sys_quotactl(int cmd, const char* special, int id, void* addr) { (void)cmd; (void)special; (void)id; (void)addr; return -1; }
long linux_sys_gettid(void) { return 1; }
long linux_sys_set_tid_address(int* tidptr) { (void)tidptr; return 1; }
long linux_sys_unshare(int flags) { (void)flags; return -1; }
long linux_sys_futex(int* uaddr, int op, int val, const void* timeout, int* uaddr2, int val3) { (void)uaddr; (void)op; (void)val; (void)timeout; (void)uaddr2; (void)val3; return -1; }
long linux_sys_epoll_create(int size) { (void)size; return -1; }
long linux_sys_epoll_create1(int flags) { (void)flags; return -1; }
long linux_sys_epoll_ctl(int epfd, int op, int fd, void* event) { (void)epfd; (void)op; (void)fd; (void)event; return -1; }
long linux_sys_epoll_wait(int epfd, void* events, int maxevents, int timeout) { (void)epfd; (void)events; (void)maxevents; (void)timeout; return -1; }
long linux_sys_epoll_pwait(int epfd, void* events, int maxevents, int timeout, const void* sigmask, uint64_t sigsetsize) { (void)epfd; (void)events; (void)maxevents; (void)timeout; (void)sigmask; (void)sigsetsize; return -1; }
long linux_sys_inotify_init(void) { return -1; }
long linux_sys_inotify_init1(int flags) { (void)flags; return -1; }
long linux_sys_inotify_add_watch(int fd, const char* path, uint32_t mask) { (void)fd; (void)path; (void)mask; return -1; }
long linux_sys_inotify_rm_watch(int fd, int wd) { (void)fd; (void)wd; return -1; }
long linux_sys_signalfd(int fd, const void* mask, uint64_t sigsetsize) { (void)fd; (void)mask; (void)sigsetsize; return -1; }
long linux_sys_timerfd_create(int clockid, int flags) { (void)clockid; (void)flags; return -1; }
long linux_sys_timerfd_settime(int fd, int flags, const void* new_value, void* old_value) { (void)fd; (void)flags; (void)new_value; (void)old_value; return -1; }
long linux_sys_timerfd_gettime(int fd, void* curr_value) { (void)fd; (void)curr_value; return -1; }
long linux_sys_eventfd(int initval) { (void)initval; return -1; }
long linux_sys_eventfd2(int initval, int flags) { (void)initval; (void)flags; return -1; }
long linux_sys_pipe2(int* pipefd, int flags) { (void)pipefd; (void)flags; return -1; }
long linux_sys_dup3(int oldfd, int newfd, int flags) { (void)oldfd; (void)newfd; (void)flags; return -1; }
long linux_sys_preadv(int fd, const void* iov, int iovcnt, int64_t offset) { (void)fd; (void)iov; (void)iovcnt; (void)offset; return -1; }
long linux_sys_pwritev(int fd, const void* iov, int iovcnt, int64_t offset) { (void)fd; (void)iov; (void)iovcnt; (void)offset; return -1; }
long linux_sys_recvmmsg(int fd, void* msgvec, uint32_t vlen, int flags, const void* timeout) { (void)fd; (void)msgvec; (void)vlen; (void)flags; (void)timeout; return -1; }
long linux_sys_sendmmsg(int fd, const void* msgvec, uint32_t vlen, int flags) { (void)fd; (void)msgvec; (void)vlen; (void)flags; return -1; }
long linux_sys_process_vm_readv(int pid, const void* lvec, uint64_t liovcnt, const void* rvec, uint64_t riovcnt, uint64_t flags) { (void)pid; (void)lvec; (void)liovcnt; (void)rvec; (void)riovcnt; (void)flags; return -1; }
long linux_sys_process_vm_writev(int pid, const void* lvec, uint64_t liovcnt, const void* rvec, uint64_t riovcnt, uint64_t flags) { (void)pid; (void)lvec; (void)liovcnt; (void)rvec; (void)riovcnt; (void)flags; return -1; }
long linux_sys_kcmp(int pid1, int pid2, int type, uint64_t idx1, uint64_t idx2) { (void)pid1; (void)pid2; (void)type; (void)idx1; (void)idx2; return -1; }
long linux_sys_memfd_create(const char* name, uint64_t flags) { (void)name; (void)flags; return -1; }
long linux_sys_userfaultfd(int flags) { (void)flags; return -1; }
long linux_sys_bpf(int cmd, void* attr, uint32_t size) { (void)cmd; (void)attr; (void)size; return -1; }
long linux_sys_execveat(int dirfd, const char* pathname, char* const* argv, char* const* envp, int flags) { (void)dirfd; (void)pathname; (void)argv; (void)envp; (void)flags; return -1; }
long linux_sys_getrandom(void* buf, uint64_t count, uint32_t flags) { (void)buf; (void)count; (void)flags; return -1; }
long linux_sys_membarrier(int cmd, int flags) { (void)cmd; (void)flags; return -1; }
long linux_sys_mlock2(const void* addr, uint64_t len, int flags) { (void)addr; (void)len; (void)flags; return 0; }
long linux_sys_copy_file_range(int fd_in, int64_t* off_in, int fd_out, int64_t* off_out, uint64_t len, uint32_t flags) { (void)fd_in; (void)off_in; (void)fd_out; (void)off_out; (void)len; (void)flags; return -1; }
long linux_sys_preadv2(int fd, const void* iov, int iovcnt, int64_t offset, uint32_t flags) { (void)fd; (void)iov; (void)iovcnt; (void)offset; (void)flags; return -1; }
long linux_sys_pwritev2(int fd, const void* iov, int iovcnt, int64_t offset, uint32_t flags) { (void)fd; (void)iov; (void)iovcnt; (void)offset; (void)flags; return -1; }
long linux_sys_statx(int dirfd, const char* pathname, int flags, uint32_t mask, void* statxbuf) { (void)dirfd; (void)pathname; (void)flags; (void)mask; (void)statxbuf; return -1; }
long linux_sys_rseq(void* rseq, uint32_t rseq_len, int flags, uint32_t sig) { (void)rseq; (void)rseq_len; (void)flags; (void)sig; return -1; }
long linux_sys_pidfd_open(int pid, uint32_t flags) { (void)pid; (void)flags; return -1; }
long linux_sys_clone3(void* cl_args, uint64_t size) { (void)cl_args; (void)size; return -1; }
long linux_sys_close_range(int first, int last, uint32_t flags) { (void)first; (void)last; (void)flags; return -1; }
long linux_sys_openat2(int dirfd, const char* pathname, void* how, uint64_t size) { (void)dirfd; (void)pathname; (void)how; (void)size; return -1; }
long linux_sys_faccessat2(int dirfd, const char* pathname, int mode, int flags) { (void)dirfd; (void)pathname; (void)mode; (void)flags; return -1; }
long linux_sys_process_madvise(int pid, const void* vec, uint64_t vlen, int advice, uint32_t flags) { (void)pid; (void)vec; (void)vlen; (void)advice; (void)flags; return -1; }
long linux_sys_epoll_pwait2(int epfd, void* events, int maxevents, const void* timeout, const void* sigmask, uint64_t sigsetsize) { (void)epfd; (void)events; (void)maxevents; (void)timeout; (void)sigmask; (void)sigsetsize; return -1; }
long linux_sys_mount_setattr(int dfd, const char* path, uint32_t flags, void* attr, uint64_t size) { (void)dfd; (void)path; (void)flags; (void)attr; (void)size; return -1; }
long linux_sys_landlock_create_ruleset(const void* attr, uint64_t size, uint32_t flags) { (void)attr; (void)size; (void)flags; return -1; }
long linux_sys_landlock_add_rule(int ruleset_fd, uint32_t rule_type, const void* rule_attr, uint32_t flags) { (void)ruleset_fd; (void)rule_type; (void)rule_attr; (void)flags; return -1; }
long linux_sys_landlock_restrict_self(int ruleset_fd, uint32_t flags) { (void)ruleset_fd; (void)flags; return -1; }
long linux_sys_memfd_secret(uint32_t flags) { (void)flags; return -1; }
long linux_sys_process_mrelease(int pidfd, uint32_t flags) { (void)pidfd; (void)flags; return -1; }
long linux_sys_futex_waitv(void* waiters, uint32_t nr_futexes, uint32_t flags, const void* timeout, uint32_t clockid) { (void)waiters; (void)nr_futexes; (void)flags; (void)timeout; (void)clockid; return -1; }
long linux_sys_set_mempolicy_home_node(uint64_t start, uint64_t len, uint64_t home_node, uint64_t flags) { (void)start; (void)len; (void)home_node; (void)flags; return -1; }
long linux_sys_cachestat(int fd, const void* range, void* cstat, uint32_t flags) { (void)fd; (void)range; (void)cstat; (void)flags; return -1; }
long linux_sys_map_shadow_stack(uint64_t addr, uint64_t size, uint32_t flags) { (void)addr; (void)size; (void)flags; return -1; }
long linux_sys_futex_wake(void* uaddr, uint32_t nr_wake, uint32_t flags) { (void)uaddr; (void)nr_wake; (void)flags; return -1; }
long linux_sys_futex_wait(void* uaddr, uint64_t val, uint64_t mask, uint32_t flags, void* timeout, uint32_t clockid) { (void)uaddr; (void)val; (void)mask; (void)flags; (void)timeout; (void)clockid; return -1; }
long linux_sys_futex_requeue(void* uaddr1, void* uaddr2, uint64_t nr_wake, uint64_t nr_requeue, uint64_t cmpval, uint32_t flags) { (void)uaddr1; (void)uaddr2; (void)nr_wake; (void)nr_requeue; (void)cmpval; (void)flags; return -1; }

long linux_syscall_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
{
    switch (nr) {
    case 0: return linux_sys_read((int)a1, (void*)a2, a3);
    case 1: return linux_sys_write((int)a1, (const void*)a2, a3);
    case 2: return linux_sys_open((const char*)a1, (int)a2, (int)a3);
    case 3: return linux_sys_close((int)a1);
    case 4: return linux_sys_stat((const char*)a1, (linux_stat_t*)a2);
    case 5: return linux_sys_fstat((int)a1, (linux_stat_t*)a2);
    case 6: return linux_sys_lstat((const char*)a1, (linux_stat_t*)a2);
    case 7: return linux_sys_poll((void*)a1, (uint32_t)a2, (int)a3);
    case 8: return linux_sys_lseek((int)a1, (int64_t)a2, (int)a3);
    case 9: return linux_sys_mmap((void*)a1, a2, (int)a3, (int)a4, (int)a5, (int64_t)a6);
    case 10: return linux_sys_mprotect((void*)a1, a2, (int)a3);
    case 11: return linux_sys_munmap((void*)a1, a2);
    case 12: return linux_sys_brk((void*)a1);
    case 13: return linux_sys_rt_sigaction((int)a1, (const void*)a2, (void*)a3, a4);
    case 14: return linux_sys_rt_sigprocmask((int)a1, (const void*)a2, (void*)a3, a4);
    case 16: return linux_sys_ioctl((int)a1, a2, (void*)a3);
    case 17: return linux_sys_pread64((int)a1, (void*)a2, a3, (int64_t)a4);
    case 18: return linux_sys_pwrite64((int)a1, (const void*)a2, a3, (int64_t)a4);
    case 19: return linux_sys_readv((int)a1, (const void*)a2, (int)a3);
    case 20: return linux_sys_writev((int)a1, (const void*)a2, (int)a3);
    case 21: return linux_sys_access((const char*)a1, (int)a2);
    case 22: return linux_sys_pipe((int*)a1);
    case 23: return linux_sys_select((int)a1, (void*)a2, (void*)a3, (void*)a4, (void*)a5);
    case 24: return linux_sys_sched_yield();
    case 39: return linux_sys_getpid();
    case 56: return linux_sys_clone(a1, (void*)a2, (int*)a3, (int*)a4, a5);
    case 57: return linux_sys_fork();
    case 58: return linux_sys_vfork();
    case 59: return linux_sys_execve((const char*)a1, (char* const*)a2, (char* const*)a3);
    case 60: return linux_sys_exit((int)a1);
    case 61: return linux_sys_wait4((int)a1, (int*)a2, (int)a3, (void*)a4);
    case 62: return linux_sys_kill((int)a1, (int)a2);
    case 63: return linux_sys_tkill((int)a1, (int)a2);
    case 78: return linux_sys_getcwd((char*)a1, a2);
    case 79: return linux_sys_chdir((const char*)a1);
    case 80: return linux_sys_fchdir((int)a1);
    case 81: return linux_sys_rename((const char*)a1, (const char*)a2);
    case 82: return linux_sys_mkdir((const char*)a1, (int)a2);
    case 83: return linux_sys_rmdir((const char*)a1);
    case 84: return linux_sys_creat((const char*)a1, (int)a2);
    case 85: return linux_sys_link((const char*)a1, (const char*)a2);
    case 86: return linux_sys_unlink((const char*)a1);
    case 87: return linux_sys_symlink((const char*)a1, (const char*)a2);
    case 88: return linux_sys_readlink((const char*)a1, (char*)a2, a3);
    case 89: return linux_sys_chmod((const char*)a1, (int)a2);
    case 90: return linux_sys_fchmod((int)a1, (int)a2);
    case 91: return linux_sys_chown((const char*)a1, (uid_t)a2, (gid_t)a3);
    case 92: return linux_sys_fchown((int)a1, (uid_t)a2, (gid_t)a3);
    case 93: return linux_sys_lchown((const char*)a1, (uid_t)a2, (gid_t)a3);
    case 94: return linux_sys_umask((int)a1);
    case 96: return linux_sys_gettimeofday((void*)a1, (void*)a2);
    case 97: return linux_sys_settimeofday((const void*)a1, (const void*)a2);
    case 98: return linux_sys_getrlimit((int)a1, (void*)a2);
    case 99: return linux_sys_getrusage((int)a1, (linux_rusage_t*)a2);
    case 100: return linux_sys_sysinfo((void*)a1);
    case 102: return linux_sys_getuid();
    case 103: return linux_sys_syslog((int)a1, (char*)a2, (int)a3);
    case 104: return linux_sys_getgid();
    case 107: return linux_sys_geteuid();
    case 108: return linux_sys_getegid();
    case 109: return linux_sys_setpgid((int)a1, (int)a2);
    case 110: return linux_sys_getppid();
    case 111: return linux_sys_getpgid((int)a1);
    case 131: return linux_sys_sigaltstack((const linux_stack_t*)a1, (linux_stack_t*)a2);
    case 140: return linux_sys_gettid();
    case 186: return linux_sys_gettid();
    case 200: return linux_sys_tgkill((int)a1, (int)a2, (int)a3);
    case 202: return linux_sys_futex((int*)a1, (int)a2, (int)a3, (const void*)a4, (int*)a5, (int)a6);
    case 217: return linux_sys_madvise((void*)a1, a2, (int)a3);
    case 232: return linux_sys_epoll_wait((int)a1, (void*)a2, (int)a3, (int)a4);
    case 233: return linux_sys_epoll_ctl((int)a1, (int)a2, (int)a3, (void*)a4);
    case 257: return linux_sys_openat((int)a1, (const char*)a2, (int)a3, (int)a4);
    case 262: return linux_sys_newfstatat((int)a1, (const char*)a2, (linux_stat_t*)a3, (int)a4);
    case 272: return linux_sys_uname((void*)a1);
    case 281: return linux_sys_epoll_create1((int)a1);
    case 282: return linux_sys_pipe2((int*)a1, (int)a2);
    case 283: return linux_sys_inotify_init1((int)a1);
    case 291: return linux_sys_inotify_add_watch((int)a1, (const char*)a2, (uint32_t)a3);
    case 292: return linux_sys_inotify_rm_watch((int)a1, (int)a2);
    case 314: return linux_sys_sched_setaffinity((int)a1, a2, (const void*)a3);
    case 315: return linux_sys_sched_getaffinity((int)a1, a2, (void*)a3);
    case 318: return linux_sys_getrandom((void*)a1, a2, (uint32_t)a3);
    case 319: return linux_sys_memfd_create((const char*)a1, a2);
    case 322: return linux_sys_execveat((int)a1, (const char*)a2, (char* const*)a3, (char* const*)a4, (int)a5);
    case 330: return linux_sys_pkey_alloc((int)a1, (uint64_t)a2);
    case 331: return linux_sys_pkey_free((int)a1);
    case 332: return linux_sys_statx((int)a1, (const char*)a2, (int)a3, (uint32_t)a4, (void*)a5);
    case 334: return linux_sys_rseq((void*)a1, (uint32_t)a2, (int)a3, (uint32_t)a4);
    case 435: return linux_sys_clone3((void*)a1, a2);
    case 436: return linux_sys_close_range((int)a1, (int)a2, (uint32_t)a3);
    case 437: return linux_sys_openat2((int)a1, (const char*)a2, (void*)a3, a4);
    case 439: return linux_sys_faccessat2((int)a1, (const char*)a2, (int)a3, (int)a4);
    case 440: return linux_sys_process_madvise((int)a1, (const void*)a2, a3, (int)a4, (uint32_t)a5);
    case 441: return linux_sys_epoll_pwait2((int)a1, (void*)a2, (int)a3, (const void*)a4, (const void*)a5, a6);
    case 442: return linux_sys_mount_setattr((int)a1, (const char*)a2, (uint32_t)a3, (void*)a4, a5);
    case 444: return linux_sys_landlock_create_ruleset((const void*)a1, a2, (uint32_t)a3);
    case 445: return linux_sys_landlock_add_rule((int)a1, (uint32_t)a2, (const void*)a3, (uint32_t)a4);
    case 446: return linux_sys_landlock_restrict_self((int)a1, (uint32_t)a2);
    case 447: return linux_sys_memfd_secret((uint32_t)a1);
    case 448: return linux_sys_process_mrelease((int)a1, (uint32_t)a2);
    case 449: return linux_sys_futex_waitv((void*)a1, (uint32_t)a2, (uint32_t)a3, (const void*)a4, (uint32_t)a5);
    case 450: return linux_sys_set_mempolicy_home_node(a1, a2, a3, a4);
    case 451: return linux_sys_cachestat((int)a1, (const void*)a2, (void*)a3, (uint32_t)a4);
    case 452: return linux_sys_map_shadow_stack(a1, a2, (uint32_t)a3);
    case 453: return linux_sys_futex_wake((void*)a1, (uint32_t)a2, (uint32_t)a3);
    case 454: return linux_sys_futex_wait((void*)a1, a2, a3, (uint32_t)a4, (void*)a5, (uint32_t)a6);
    case 455: return linux_sys_futex_requeue((void*)a1, (void*)a2, a3, a4, a5, (uint32_t)a6);
    default: return -1;
    }
}