#pragma once

#include "sys/types.h"

struct sysinfo {
    long   uptime;
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
    unsigned int  mem_unit;
    char          _f[20 - 2 * sizeof(long) - sizeof(int)];
};

struct utsname;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long   ru_maxrss;
    long   ru_ixrss;
    long   ru_idrss;
    long   ru_isrss;
    long   ru_minflt;
    long   ru_majflt;
    long   ru_nswap;
    long   ru_inblock;
    long   ru_oublock;
    long   ru_msgsnd;
    long   ru_msgrcv;
    long   ru_nsignals;
    long   ru_nvcsw;
    long   ru_nivcsw;
};

#define RLIMIT_CPU    0
#define RLIMIT_FSIZE  1
#define RLIMIT_DATA   2
#define RLIMIT_STACK  3
#define RLIMIT_CORE   4
#define RLIMIT_RSS    5
#define RLIMIT_NPROC  6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS     9
#define RLIMIT_LOCKS  10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE   13
#define RLIMIT_RTPRIO 14
#define RLIMIT_RTTIME 15
#define RLIMIT_NLIMITS 16
#define RLIM_INFINITY  (~0UL)

#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

#define CLONE_NEWNS   0x00020000
#define CLONE_NEWUTS  0x04000000
#define CLONE_NEWIPC  0x08000000
#define CLONE_NEWNET  0x40000000
#define CLONE_NEWPID  0x20000000
#define CLONE_NEWUSER 0x10000000
#define CLONE_NEWCGROUP 0x02000000

#define MS_RDONLY     1
#define MS_NOSUID     2
#define MS_NODEV      4
#define MS_NOEXEC     8
#define MS_SYNCHRONOUS 16
#define MS_REMOUNT   32
#define MS_MANDLOCK  64
#define MS_DIRSYNC   128
#define MS_NOATIME   1024
#define MS_NODIRATIME 2048
#define MS_BIND      4096
#define MS_MOVE      8192
#define MS_REC       16384
#define MS_SILENT    32768
#define MS_POSIXACL  (1 << 16)
#define MS_UNBINDABLE (1 << 17)
#define MS_PRIVATE   (1 << 18)
#define MS_SLAVE     (1 << 19)
#define MS_SHARED    (1 << 20)

#define MNT_FORCE       1
#define MNT_DETACH      2
#define MNT_EXPIRE      4
#define UMOUNT_NOFOLLOW 8

#ifdef __cplusplus
extern "C" {
#endif

int   sysinfo(struct sysinfo *info);
int   uname(struct utsname *buf);
int   getrlimit(int resource, struct rlimit *rlim);
int   setrlimit(int resource, const struct rlimit *rlim);
int   prlimit(pid_t pid, int resource, const struct rlimit *new_limit, struct rlimit *old_limit);

int   getrusage(int who, struct rusage *usage);

int   getpriority(int which, id_t who);
int   setpriority(int which, id_t who, int prio);

int   mount(const char *source, const char *target, const char *filesystemtype,
            unsigned long mountflags, const void *data);
int   umount(const char *target);
int   umount2(const char *target, int flags);
int   pivot_root(const char *new_root, const char *put_old);

long  syscall(long number, ...);
int   reboot(int cmd);

int   sethostname(const char *name, size_t len);
int   gethostname(char *name, size_t len);
int   setdomainname(const char *name, size_t len);
int   getdomainname(char *name, size_t len);

int   chroot(const char *path);
int   swapoff(const char *path);
int   swapon(const char *path, int flags);

int   sched_setaffinity(pid_t pid, size_t cpusetsize, const unsigned long *mask);
int   sched_getaffinity(pid_t pid, size_t cpusetsize, unsigned long *mask);
int   sched_setparam(pid_t pid, const struct sched_param *param);
int   sched_getparam(pid_t pid, struct sched_param *param);
int   sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);
int   sched_getscheduler(pid_t pid);
int   sched_get_priority_max(int policy);
int   sched_get_priority_min(int policy);

int   iopl(int level);
int   ioperm(unsigned long from, unsigned long num, int turn_on);

#ifdef __cplusplus
}
#endif