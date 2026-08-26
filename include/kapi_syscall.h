

#ifndef KAPI_SYSCALL_H
#define KAPI_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_read                    0
#define SYS_write                   1
#define SYS_open                    2
#define SYS_close                   3
#define SYS_stat                    4
#define SYS_fstat                   5
#define SYS_lstat                   6
#define SYS_poll                    7
#define SYS_lseek                   8
#define SYS_mmap                    9
#define SYS_mprotect                10
#define SYS_munmap                  11
#define SYS_brk                     12
#define SYS_rt_sigaction            13
#define SYS_rt_sigprocmask          14
#define SYS_rt_sigreturn            15
#define SYS_ioctl                   16
#define SYS_pread64                 17
#define SYS_pwrite64                18
#define SYS_readv                   19
#define SYS_writev                  20
#define SYS_access                  21
#define SYS_pipe                    22
#define SYS_select                  23
#define SYS_sched_yield             24
#define SYS_mremap                  25
#define SYS_msync                   26
#define SYS_mincore                 27
#define SYS_madvise                 28
#define SYS_shmget                  29
#define SYS_shmat                   30
#define SYS_shmctl                  31
#define SYS_dup                     32
#define SYS_dup2                    33
#define SYS_pause                   34
#define SYS_nanosleep               35
#define SYS_getitimer               36
#define SYS_alarm                   37
#define SYS_setitimer               38
#define SYS_getpid                  39
#define SYS_sendfile                40
#define SYS_socket                  41
#define SYS_connect                 42
#define SYS_accept                  43
#define SYS_sendto                  44
#define SYS_recvfrom                45
#define SYS_sendmsg                 46
#define SYS_recvmsg                 47
#define SYS_shutdown                48
#define SYS_bind                    49
#define SYS_listen                  50
#define SYS_getsockname             51
#define SYS_getpeername             52
#define SYS_socketpair              53
#define SYS_setsockopt              54
#define SYS_getsockopt              55
#define SYS_clone                   56
#define SYS_fork                    57
#define SYS_vfork                   58
#define SYS_execve                  59
#define SYS_exit                    60
#define SYS_wait4                   61
#define SYS_kill                    62
#define SYS_uname                   63
#define SYS_semget                  64
#define SYS_semop                   65
#define SYS_semctl                  66
#define SYS_shmdt                   67
#define SYS_msgget                  68
#define SYS_msgsnd                  69
#define SYS_msgrcv                  70
#define SYS_msgctl                  71
#define SYS_fcntl                   72
#define SYS_flock                   73
#define SYS_fsync                   74
#define SYS_fdatasync               75
#define SYS_truncate                76
#define SYS_ftruncate               77
#define SYS_getdents                78
#define SYS_getcwd                  79
#define SYS_chdir                   80
#define SYS_fchdir                  81
#define SYS_rename                  82
#define SYS_mkdir                   83
#define SYS_rmdir                   84
#define SYS_creat                   85
#define SYS_link                    86
#define SYS_unlink                  87
#define SYS_symlink                 88
#define SYS_readlink                89
#define SYS_chmod                   90
#define SYS_fchmod                  91
#define SYS_chown                   92
#define SYS_fchown                  93
#define SYS_lchown                  94
#define SYS_umask                   95
#define SYS_gettimeofday            96
#define SYS_getrlimit               97
#define SYS_getrusage               98
#define SYS_sysinfo                 99
#define SYS_times                   100
#define SYS_ptrace                  101
#define SYS_getuid                  102
#define SYS_syslog                  103
#define SYS_getgid                  104
#define SYS_setuid                  105
#define SYS_setgid                  106
#define SYS_geteuid                 107
#define SYS_getegid                 108
#define SYS_setpgid                 109
#define SYS_getppid                 110
#define SYS_getpgrp                 111
#define SYS_setsid                  112
#define SYS_setreuid                113
#define SYS_setregid                114
#define SYS_getgroups               115
#define SYS_setgroups               116
#define SYS_setresuid               117
#define SYS_getresuid               118
#define SYS_setresgid               119
#define SYS_getresgid               120
#define SYS_getpgid                 121
#define SYS_setfsuid                122
#define SYS_setfsgid                123
#define SYS_getsid                  124
#define SYS_capget                  125
#define SYS_capset                  126
#define SYS_rt_sigpending           127
#define SYS_rt_sigtimedwait         128
#define SYS_rt_sigqueueinfo         129
#define SYS_rt_sigsuspend           130
#define SYS_sigaltstack             131
#define SYS_utime                   132
#define SYS_mknod                   133
#define SYS_uselib                  134
#define SYS_personality             135
#define SYS_ustat                   136
#define SYS_statfs                  137
#define SYS_fstatfs                 138
#define SYS_sysfs                   139
#define SYS_getpriority             140
#define SYS_setpriority             141
#define SYS_sched_setparam          142
#define SYS_sched_getparam          143
#define SYS_sched_setscheduler      144
#define SYS_sched_getscheduler      145
#define SYS_sched_get_priority_max  146
#define SYS_sched_get_priority_min  147
#define SYS_sched_rr_get_interval   148
#define SYS_mlock                   149
#define SYS_munlock                 150
#define SYS_mlockall                151
#define SYS_munlockall              152
#define SYS_vhangup                 153
#define SYS_modify_ldt              154
#define SYS_pivot_root              155
#define SYS__sysctl                 156
#define SYS_prctl                   157
#define SYS_arch_prctl              158
#define SYS_adjtimex                159
#define SYS_setrlimit               160
#define SYS_chroot                  161
#define SYS_sync                    162
#define SYS_acct                    163
#define SYS_settimeofday            164
#define SYS_mount                   165
#define SYS_umount2                 166
#define SYS_swapon                  167
#define SYS_swapoff                 168
#define SYS_reboot                  169
#define SYS_sethostname             170
#define SYS_setdomainname           171
#define SYS_iopl                    172
#define SYS_ioperm                  173
#define SYS_create_module           174
#define SYS_init_module             175
#define SYS_delete_module           176
#define SYS_get_kernel_syms         177
#define SYS_query_module            178
#define SYS_quotactl                179
#define SYS_nfsservctl              180
#define SYS_getpmsg                 181
#define SYS_putpmsg                 182
#define SYS_afs_syscall             183
#define SYS_tuxcall                 184
#define SYS_security                185
#define SYS_gettid                  186
#define SYS_readahead               187
#define SYS_setxattr                188
#define SYS_lsetxattr               189
#define SYS_fsetxattr               190
#define SYS_getxattr                191
#define SYS_lgetxattr               192
#define SYS_fgetxattr               193
#define SYS_listxattr               194
#define SYS_llistxattr              195
#define SYS_flistxattr              196
#define SYS_removexattr             197
#define SYS_lremovexattr            198
#define SYS_fremovexattr            199
#define SYS_tkill                   200
#define SYS_time                    201
#define SYS_futex                   202
#define SYS_sched_setaffinity       203
#define SYS_sched_getaffinity       204
#define SYS_set_thread_area         205
#define SYS_io_setup                206
#define SYS_io_destroy              207
#define SYS_io_getevents            208
#define SYS_io_submit               209
#define SYS_io_cancel               210
#define SYS_get_thread_area         211
#define SYS_lookup_dcookie          212
#define SYS_epoll_create            213
#define SYS_epoll_ctl_old           214
#define SYS_epoll_wait_old          215
#define SYS_remap_file_pages        216
#define SYS_getdents64              217
#define SYS_set_tid_address         218
#define SYS_restart_syscall         219
#define SYS_semtimedop              220
#define SYS_fadvise64               221
#define SYS_timer_create            222
#define SYS_timer_settime           223
#define SYS_timer_gettime           224
#define SYS_timer_getoverrun        225
#define SYS_timer_delete            226
#define SYS_clock_settime           227
#define SYS_clock_gettime           228
#define SYS_clock_getres            229
#define SYS_clock_nanosleep         230
#define SYS_exit_group              231
#define SYS_epoll_wait              232
#define SYS_epoll_ctl               233
#define SYS_tgkill                  234
#define SYS_utimes                  235
#define SYS_vserver                 236
#define SYS_mbind                   237
#define SYS_set_mempolicy           238
#define SYS_get_mempolicy           239
#define SYS_mq_open                 240
#define SYS_mq_unlink               241
#define SYS_mq_timedsend            242
#define SYS_mq_timedreceive         243
#define SYS_mq_notify               244
#define SYS_mq_getsetattr           245
#define SYS_kexec_load              246
#define SYS_waitid                  247
#define SYS_add_key                 248
#define SYS_request_key             249
#define SYS_keyctl                  250
#define SYS_ioprio_set              251
#define SYS_ioprio_get              252
#define SYS_inotify_init            253
#define SYS_inotify_add_watch       254
#define SYS_inotify_rm_watch        255
#define SYS_migrate_pages           256
#define SYS_openat                  257
#define SYS_mkdirat                 258
#define SYS_mknodat                 259
#define SYS_fchownat                260
#define SYS_futimesat               261
#define SYS_newfstatat              262
#define SYS_unlinkat                263
#define SYS_renameat                264
#define SYS_linkat                  265
#define SYS_symlinkat               266
#define SYS_readlinkat              267
#define SYS_fchmodat                268
#define SYS_faccessat               269
#define SYS_pselect6                270
#define SYS_ppoll                   271
#define SYS_unshare                 272
#define SYS_set_robust_list         273
#define SYS_get_robust_list         274
#define SYS_splice                  275
#define SYS_tee                     276
#define SYS_sync_file_range         277
#define SYS_vmsplice                278
#define SYS_move_pages              279
#define SYS_utimensat               280
#define SYS_epoll_pwait             281
#define SYS_signalfd                282
#define SYS_timerfd_create          283
#define SYS_eventfd                 284
#define SYS_fallocate               285
#define SYS_timerfd_settime         286
#define SYS_timerfd_gettime         287
#define SYS_accept4                 288
#define SYS_signalfd4               289
#define SYS_eventfd2                290
#define SYS_epoll_create1           291
#define SYS_dup3                    292
#define SYS_pipe2                   293
#define SYS_inotify_init1           294
#define SYS_preadv                  295
#define SYS_pwritev                 296
#define SYS_rt_tgsigqueueinfo       297
#define SYS_perf_event_open         298
#define SYS_recvmmsg                299
#define SYS_fanotify_init           300
#define SYS_fanotify_mark           301
#define SYS_prlimit64               302
#define SYS_name_to_handle_at       303
#define SYS_open_by_handle_at       304
#define SYS_clock_adjtime           305
#define SYS_syncfs                  306
#define SYS_sendmmsg                307
#define SYS_setns                   308
#define SYS_getcpu                  309
#define SYS_process_vm_readv        310
#define SYS_process_vm_writev       311
#define SYS_kcmp                    312
#define SYS_finit_module            313
#define SYS_sched_setattr           314
#define SYS_sched_getattr           315
#define SYS_renameat2               316
#define SYS_seccomp                 317
#define SYS_getrandom               318
#define SYS_memfd_create            319
#define SYS_kexec_file_load         320
#define SYS_bpf                     321
#define SYS_execveat                322
#define SYS_userfaultfd             323
#define SYS_membarrier              324
#define SYS_mlock2                  325
#define SYS_copy_file_range         326
#define SYS_preadv2                 327
#define SYS_pwritev2                328
#define SYS_pkey_mprotect           329
#define SYS_pkey_alloc              330
#define SYS_pkey_free               331
#define SYS_statx                   332
#define SYS_io_pgetevents           333
#define SYS_rseq                    334
#define SYS_kexec_file_load2        335
#define SYS_pidfd_send_signal       424
#define SYS_io_uring_setup          425
#define SYS_io_uring_enter          426
#define SYS_io_uring_register       427
#define SYS_open_tree               428
#define SYS_move_mount              429
#define SYS_fsopen                  430
#define SYS_fsconfig                431
#define SYS_fsmount                 432
#define SYS_fspick                  433
#define SYS_pidfd_open              434
#define SYS_clone3                  435
#define SYS_close_range             436
#define SYS_openat2                 437
#define SYS_pidfd_getfd             438
#define SYS_faccessat2              439
#define SYS_process_madvise         440
#define SYS_epoll_pwait2            441
#define SYS_mount_setattr           442
#define SYS_quotactl_fd             443
#define SYS_landlock_create_ruleset 444
#define SYS_landlock_add_rule       445
#define SYS_landlock_restrict_self  446
#define SYS_memfd_secret            447
#define SYS_process_mrelease        448
#define SYS_futex_waitv             449
#define SYS_set_mempolicy_home_node 450

#define SYS_kenux_info              451
#define SYS_kenux_debug             452
#define SYS_kenux_get_version       453
#define SYS_kenux_get_uptime        454
#define SYS_kenux_get_loadavg       455
#define SYS_kenux_reboot            456
#define SYS_kenux_poweroff          457
#define SYS_kenux_halt              458
#define SYS_kenux_get_cpu_count     459
#define SYS_kenux_get_cpu_info      460
#define SYS_kenux_set_affinity      461
#define SYS_kenux_get_affinity      462
#define SYS_kenux_create_namespace  463
#define SYS_kenux_enter_namespace   464
#define SYS_kenux_get_namespace     465
#define SYS_kenux_vmspace_create    466
#define SYS_kenux_vmspace_destroy   467
#define SYS_kenux_vmspace_switch    468
#define SYS_kenux_iommu_map         469
#define SYS_kenux_iommu_unmap       470
#define SYS_kenux_dma_alloc         471
#define SYS_kenux_dma_free          472
#define SYS_kenux_pci_read          473
#define SYS_kenux_pci_write         474
#define SYS_kenux_pci_enum          475
#define SYS_kenux_acpi_query        476
#define SYS_kenux_smbios_get        477
#define SYS_kenux_fb_get_info       478
#define SYS_kenux_fb_map            479
#define SYS_kenux_fb_unmap          480
#define SYS_kenux_fb_flip           481
#define SYS_kenux_gpu_submit        482
#define SYS_kenux_gpu_wait          483
#define SYS_kenux_net_attach        484
#define SYS_kenux_net_detach        485
#define SYS_kenux_net_ioctl         486
#define SYS_kenux_fs_snapshot       487
#define SYS_kenux_fs_rollback       488
#define SYS_kenux_fs_compress       489
#define SYS_kenux_fs_encrypt        490
#define SYS_kenux_audit_log         491
#define SYS_kenux_audit_config      492
#define SYS_kenux_seccomp_install   493
#define SYS_kenux_seccomp_filter    494
#define SYS_kenux_trace_attach      495
#define SYS_kenux_trace_detach      496
#define SYS_kenux_trace_read        497
#define SYS_kenux_trace_write       498
#define SYS_kenux_kprobe_register   499
#define SYS_kenux_kprobe_unregister 500

#define SYS_kenux_reserved_start    501
#define SYS_kenux_reserved_end      999

#define KAPI_SYSCALL_MAX            1000
#define KAPI_SYSCALL_COUNT          (SYS_kenux_reserved_end + 1)

static inline long kapi_syscall0(long nr)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr) : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall1(long nr, long a1)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1) : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall2(long nr, long a1, long a2)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2) : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall3(long nr, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
                      : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall4(long nr, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                      : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall5(long nr, long a1, long a2, long a3, long a4, long a5)
{
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
                      : "rcx","r11","memory");
    return ret;
}

static inline long kapi_syscall6(long nr, long a1, long a2, long a3, long a4, long a5, long a6)
{
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                      : "rcx","r11","memory");
    return ret;
}

static inline long sys_read(int fd, void* buf, size_t count)
{ return kapi_syscall3(SYS_read, fd, (long)buf, count); }

static inline long sys_write(int fd, const void* buf, size_t count)
{ return kapi_syscall3(SYS_write, fd, (long)buf, count); }

static inline long sys_open(const char* pathname, int flags, int mode)
{ return kapi_syscall3(SYS_open, (long)pathname, flags, mode); }

static inline long sys_close(int fd)
{ return kapi_syscall1(SYS_close, fd); }

static inline long sys_lseek(int fd, long offset, int whence)
{ return kapi_syscall3(SYS_lseek, fd, offset, whence); }

static inline long sys_mmap(void* addr, size_t length, int prot, int flags, int fd, long offset)
{ return kapi_syscall6(SYS_mmap, (long)addr, length, prot, flags, fd, offset); }

static inline long sys_munmap(void* addr, size_t length)
{ return kapi_syscall2(SYS_munmap, (long)addr, length); }

static inline long sys_mprotect(void* addr, size_t len, int prot)
{ return kapi_syscall3(SYS_mprotect, (long)addr, len, prot); }

static inline long sys_brk(void* addr)
{ return kapi_syscall1(SYS_brk, (long)addr); }

static inline long sys_fork(void)
{ return kapi_syscall0(SYS_fork); }

static inline long sys_vfork(void)
{ return kapi_syscall0(SYS_vfork); }

static inline long sys_clone(unsigned long flags, void* stack, void* ptid, void* ctid, unsigned long tls)
{ return kapi_syscall5(SYS_clone, flags, (long)stack, (long)ptid, (long)ctid, tls); }

static inline long sys_execve(const char* filename, char* const argv[], char* const envp[])
{ return kapi_syscall3(SYS_execve, (long)filename, (long)argv, (long)envp); }

static inline long sys_exit(int error_code)
{ return kapi_syscall1(SYS_exit, error_code); }

static inline long sys_exit_group(int error_code)
{ return kapi_syscall1(SYS_exit_group, error_code); }

static inline long sys_wait4(int pid, int* wstatus, int options, void* rusage)
{ return kapi_syscall4(SYS_wait4, pid, (long)wstatus, options, (long)rusage); }

static inline long sys_getpid(void)
{ return kapi_syscall0(SYS_getpid); }

static inline long sys_getppid(void)
{ return kapi_syscall0(SYS_getppid); }

static inline long sys_gettid(void)
{ return kapi_syscall0(SYS_gettid); }

static inline long sys_kill(int pid, int sig)
{ return kapi_syscall2(SYS_kill, pid, sig); }

static inline long sys_stat(const char* pathname, void* statbuf)
{ return kapi_syscall2(SYS_stat, (long)pathname, (long)statbuf); }

static inline long sys_fstat(int fd, void* statbuf)
{ return kapi_syscall2(SYS_fstat, fd, (long)statbuf); }

static inline long sys_lstat(const char* pathname, void* statbuf)
{ return kapi_syscall2(SYS_lstat, (long)pathname, (long)statbuf); }

static inline long sys_access(const char* pathname, int mode)
{ return kapi_syscall2(SYS_access, (long)pathname, mode); }

static inline long sys_chdir(const char* path)
{ return kapi_syscall1(SYS_chdir, (long)path); }

static inline long sys_getcwd(char* buf, size_t size)
{ return kapi_syscall2(SYS_getcwd, (long)buf, size); }

static inline long sys_mkdir(const char* pathname, int mode)
{ return kapi_syscall2(SYS_mkdir, (long)pathname, mode); }

static inline long sys_rmdir(const char* pathname)
{ return kapi_syscall1(SYS_rmdir, (long)pathname); }

static inline long sys_unlink(const char* pathname)
{ return kapi_syscall1(SYS_unlink, (long)pathname); }

static inline long sys_rename(const char* oldpath, const char* newpath)
{ return kapi_syscall2(SYS_rename, (long)oldpath, (long)newpath); }

static inline long sys_creat(const char* pathname, int mode)
{ return kapi_syscall2(SYS_creat, (long)pathname, mode); }

static inline long sys_link(const char* oldpath, const char* newpath)
{ return kapi_syscall2(SYS_link, (long)oldpath, (long)newpath); }

static inline long sys_symlink(const char* target, const char* linkpath)
{ return kapi_syscall2(SYS_symlink, (long)target, (long)linkpath); }

static inline long sys_readlink(const char* pathname, char* buf, size_t bufsiz)
{ return kapi_syscall3(SYS_readlink, (long)pathname, (long)buf, bufsiz); }

static inline long sys_chmod(const char* pathname, int mode)
{ return kapi_syscall2(SYS_chmod, (long)pathname, mode); }

static inline long sys_chown(const char* pathname, int owner, int group)
{ return kapi_syscall3(SYS_chown, (long)pathname, owner, group); }

static inline long sys_truncate(const char* path, long length)
{ return kapi_syscall2(SYS_truncate, (long)path, length); }

static inline long sys_ftruncate(int fd, long length)
{ return kapi_syscall2(SYS_ftruncate, fd, length); }

static inline long sys_fsync(int fd)
{ return kapi_syscall1(SYS_fsync, fd); }

static inline long sys_fdatasync(int fd)
{ return kapi_syscall1(SYS_fdatasync, fd); }

static inline long sys_fcntl(int fd, int cmd, long arg)
{ return kapi_syscall3(SYS_fcntl, fd, cmd, arg); }

static inline long sys_dup(int oldfd)
{ return kapi_syscall1(SYS_dup, oldfd); }

static inline long sys_dup2(int oldfd, int newfd)
{ return kapi_syscall2(SYS_dup2, oldfd, newfd); }

static inline long sys_dup3(int oldfd, int newfd, int flags)
{ return kapi_syscall3(SYS_dup3, oldfd, newfd, flags); }

static inline long sys_pipe(int* pipefd)
{ return kapi_syscall1(SYS_pipe, (long)pipefd); }

static inline long sys_pipe2(int* pipefd, int flags)
{ return kapi_syscall2(SYS_pipe2, (long)pipefd, flags); }

static inline long sys_mount(const char* source, const char* target,
                             const char* filesystemtype, unsigned long mountflags,
                             const void* data)
{ return kapi_syscall5(SYS_mount, (long)source, (long)target, (long)filesystemtype, mountflags, (long)data); }

static inline long sys_umount2(const char* target, int flags)
{ return kapi_syscall2(SYS_umount2, (long)target, flags); }

static inline long sys_swapon(const char* path, int swapflags)
{ return kapi_syscall2(SYS_swapon, (long)path, swapflags); }

static inline long sys_swapoff(const char* path)
{ return kapi_syscall1(SYS_swapoff, (long)path); }

static inline long sys_nanosleep(const void* req, void* rem)
{ return kapi_syscall2(SYS_nanosleep, (long)req, (long)rem); }

static inline long sys_gettimeofday(void* tv, void* tz)
{ return kapi_syscall2(SYS_gettimeofday, (long)tv, (long)tz); }

static inline long sys_settimeofday(const void* tv, const void* tz)
{ return kapi_syscall2(SYS_settimeofday, (long)tv, (long)tz); }

static inline long sys_time(long* tloc)
{ return kapi_syscall1(SYS_time, (long)tloc); }

static inline long sys_clock_gettime(int clk_id, void* tp)
{ return kapi_syscall2(SYS_clock_gettime, clk_id, (long)tp); }

static inline long sys_clock_settime(int clk_id, const void* tp)
{ return kapi_syscall2(SYS_clock_settime, clk_id, (long)tp); }

static inline long sys_clock_nanosleep(int clk_id, int flags, const void* req, void* rem)
{ return kapi_syscall4(SYS_clock_nanosleep, clk_id, flags, (long)req, (long)rem); }

static inline long sys_rt_sigaction(int sig, const void* act, void* oact, size_t sigsetsize)
{ return kapi_syscall4(SYS_rt_sigaction, sig, (long)act, (long)oact, sigsetsize); }

static inline long sys_rt_sigprocmask(int how, const void* set, void* oldset, size_t sigsetsize)
{ return kapi_syscall4(SYS_rt_sigprocmask, how, (long)set, (long)oldset, sigsetsize); }

static inline long sys_tkill(int tid, int sig)
{ return kapi_syscall2(SYS_tkill, tid, sig); }

static inline long sys_tgkill(int tgid, int tid, int sig)
{ return kapi_syscall3(SYS_tgkill, tgid, tid, sig); }

static inline long sys_ioctl(int fd, unsigned long request, long arg)
{ return kapi_syscall3(SYS_ioctl, fd, request, arg); }

static inline long sys_pread64(int fd, void* buf, size_t count, long offset)
{ return kapi_syscall4(SYS_pread64, fd, (long)buf, count, offset); }

static inline long sys_pwrite64(int fd, const void* buf, size_t count, long offset)
{ return kapi_syscall4(SYS_pwrite64, fd, (long)buf, count, offset); }

static inline long sys_readv(int fd, const void* iov, int iovcnt)
{ return kapi_syscall3(SYS_readv, fd, (long)iov, iovcnt); }

static inline long sys_writev(int fd, const void* iov, int iovcnt)
{ return kapi_syscall3(SYS_writev, fd, (long)iov, iovcnt); }

static inline long sys_getdents(int fd, void* dirp, unsigned int count)
{ return kapi_syscall3(SYS_getdents, fd, (long)dirp, count); }

static inline long sys_getdents64(int fd, void* dirp, unsigned int count)
{ return kapi_syscall3(SYS_getdents64, fd, (long)dirp, count); }

static inline long sys_getuid(void)
{ return kapi_syscall0(SYS_getuid); }

static inline long sys_getgid(void)
{ return kapi_syscall0(SYS_getgid); }

static inline long sys_geteuid(void)
{ return kapi_syscall0(SYS_geteuid); }

static inline long sys_getegid(void)
{ return kapi_syscall0(SYS_getegid); }

static inline long sys_setuid(int uid)
{ return kapi_syscall1(SYS_setuid, uid); }

static inline long sys_setgid(int gid)
{ return kapi_syscall1(SYS_setgid, gid); }

static inline long sys_getgroups(int size, int* list)
{ return kapi_syscall2(SYS_getgroups, size, (long)list); }

static inline long sys_setgroups(int size, const int* list)
{ return kapi_syscall2(SYS_setgroups, size, (long)list); }

static inline long sys_uname(void* buf)
{ return kapi_syscall1(SYS_uname, (long)buf); }

static inline long sys_sysinfo(void* info)
{ return kapi_syscall1(SYS_sysinfo, (long)info); }

static inline long sys_getrlimit(int resource, void* rlim)
{ return kapi_syscall2(SYS_getrlimit, resource, (long)rlim); }

static inline long sys_setrlimit(int resource, const void* rlim)
{ return kapi_syscall2(SYS_setrlimit, resource, (long)rlim); }

static inline long sys_getrusage(int who, void* usage)
{ return kapi_syscall2(SYS_getrusage, who, (long)usage); }

static inline long sys_times(void* tbuf)
{ return kapi_syscall1(SYS_times, (long)tbuf); }

static inline long sys_sched_yield(void)
{ return kapi_syscall0(SYS_sched_yield); }

static inline long sys_sched_get_priority_max(int policy)
{ return kapi_syscall1(SYS_sched_get_priority_max, policy); }

static inline long sys_sched_get_priority_min(int policy)
{ return kapi_syscall1(SYS_sched_get_priority_min, policy); }

static inline long sys_nice(int inc)
{ return kapi_syscall1(SYS_getpriority, inc); }

static inline long sys_reboot(int magic1, int magic2, int cmd, void* arg)
{ return kapi_syscall4(SYS_reboot, magic1, magic2, cmd, (long)arg); }

static inline long sys_chroot(const char* path)
{ return kapi_syscall1(SYS_chroot, (long)path); }

static inline long sys_sethostname(char* name, int len)
{ return kapi_syscall2(SYS_sethostname, (long)name, len); }

static inline long sys_setdomainname(char* name, int len)
{ return kapi_syscall2(SYS_setdomainname, (long)name, len); }

static inline long sys_init_module(void* module_image, unsigned long len, const char* param_values)
{ return kapi_syscall3(SYS_init_module, (long)module_image, len, (long)param_values); }

static inline long sys_delete_module(const char* name, unsigned int flags)
{ return kapi_syscall2(SYS_delete_module, (long)name, flags); }

static inline long sys_finit_module(int fd, const char* param_values, int flags)
{ return kapi_syscall3(SYS_finit_module, fd, (long)param_values, flags); }

typedef long (*kapi_syscall_fn_t)(long, long, long, long, long, long);

int kapi_syscall_register(int nr, kapi_syscall_fn_t fn);

int kapi_syscall_unregister(int nr);

long kapi_syscall_dispatch(int nr, long a1, long a2, long a3, long a4, long a5, long a6);

int kapi_syscall_init(void);

#ifdef __cplusplus
}
#endif

#endif
