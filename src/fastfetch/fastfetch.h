/*
 * Kenux OS - System Information Display Tool
 * Header file for fastfetch functionality
 */

#ifndef _FASTFETCH_H
#define _FASTFETCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>
#else
/* Windows MinGW compatibility shims */
#include <io.h>
#include <process.h>
#include <direct.h>

#ifndef uid_t
typedef int uid_t;
#endif
#ifndef gid_t
typedef int gid_t;
#endif
/* pid_t is provided by MinGW <sys/types.h> via <sys/stat.h>; do not redefine. */

/* struct utsname stub */
struct utsname {
    char sysname[64];
    char nodename[64];
    char release[64];
    char version[64];
    char machine[64];
};
static inline int uname(struct utsname *u) {
    if (!u) return -1;
    memset(u, 0, sizeof(*u));
    strcpy(u->sysname, "KenuxK");
    strcpy(u->nodename, "kenuxk");
    strcpy(u->release, "1.0.0");
    strcpy(u->version, "#1");
    strcpy(u->machine, "x86_64");
    return 0;
}

/* struct sysinfo stub */
struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20 - 2 * sizeof(long) - sizeof(int)];
};
static inline int sysinfo(struct sysinfo *s) { if (s) memset(s, 0, sizeof(*s)); return -1; }

/* passwd / group stubs */
struct passwd {
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};
struct group {
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};
static inline struct passwd *getpwuid(uid_t u) { (void)u; return NULL; }
static inline struct passwd *getpwnam(const char *n) { (void)n; return NULL; }
static inline struct group *getgrgid(gid_t g) { (void)g; return NULL; }
static inline struct group *getgrnam(const char *n) { (void)n; return NULL; }

/* rlimit stub */
struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};
enum { RLIMIT_CPU = 0, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK, RLIMIT_CORE };
static inline int getrlimit(int r, struct rlimit *l) { (void)r; if (l) memset(l, 0, sizeof(*l)); return 0; }
static inline int setrlimit(int r, const struct rlimit *l) { (void)r; (void)l; return 0; }

static inline int kenux_chdir(const char *p) { return _chdir(p); }
static inline int kenux_access(const char *p, int m) { return _access(p, m); }
#define chdir   kenux_chdir
#define access  kenux_access

#ifndef X_OK
#define X_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

/* gethostname stub */
static inline int kenux_gethostname(char *n, size_t l) {
    if (n && l) { strncpy(n, "kenuxk", l - 1); n[l - 1] = 0; }
    return 0;
}
#define gethostname kenux_gethostname

/* popen / pclose: MinGW uses _popen / _pclose. */
#ifndef popen
#define popen _popen
#endif
#ifndef pclose
#define pclose _pclose
#endif

/* getuid stub: Windows has no getuid; pretend we are root. */
static inline uid_t kenux_getuid(void) { return 0; }
#define getuid kenux_getuid

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

// Maximum length for various strings
#define MAX_LEN 256
#define MAX_CPU_INFO 512
#define MAX_MEM_INFO 256
#define MAX_DISK_INFO 512

// Display options
typedef struct {
    int show_os;
    int show_kernel;
    int show_uptime;
    int show_packages;
    int show_shell;
    int show_terminal;
    int show_cpu;
    int show_gpu;
    int show_memory;
    int show_disk;
    int show_host;
    int show_user;
    int show_local_ip;
    int show_public_ip;
    int show_battery;
    int show_temp;
    int show_colors;
    int logo;
    char separator[MAX_LEN];
    char color_os[MAX_LEN];
    char color_kernel[MAX_LEN];
    char color_host[MAX_LEN];
    char color_user[MAX_LEN];
    char color_separator[MAX_LEN];
} FastfetchConfig;

// System information
typedef struct {
    char os[MAX_LEN];
    char kernel[MAX_LEN];
    char hostname[MAX_LEN];
    char username[MAX_LEN];
    char shell[MAX_LEN];
    char terminal[MAX_LEN];
    char cpu_info[MAX_CPU_INFO];
    char gpu_info[MAX_CPU_INFO];
    char memory_info[MAX_MEM_INFO];
    char disk_info[MAX_DISK_INFO];
    char local_ip[MAX_LEN];
    char public_ip[MAX_LEN];
    char battery[MAX_LEN];
    char temp[MAX_LEN];
    unsigned long long uptime;
    int package_count;
} SystemInfo;

// Function prototypes
void fastfetch_init(FastfetchConfig *config);
void fastfetch_cleanup(void);
int parse_arguments(FastfetchConfig *config, int argc, char **argv);
int collect_system_info(SystemInfo *info);
int get_os_info(SystemInfo *info);
int get_kernel_info(SystemInfo *info);
int get_uptime_info(SystemInfo *info);
int get_package_info(SystemInfo *info);
int get_shell_info(SystemInfo *info);
int get_terminal_info(SystemInfo *info);
int get_cpu_info(SystemInfo *info);
int get_gpu_info(SystemInfo *info);
int get_memory_info(SystemInfo *info);
int get_disk_info(SystemInfo *info);
int get_host_info(SystemInfo *info);
int get_user_info(SystemInfo *info);
int get_local_ip_info(SystemInfo *info);
int get_public_ip_info(SystemInfo *info);
int get_battery_info(SystemInfo *info);
int get_temp_info(SystemInfo *info);
void display_system_info(const FastfetchConfig *config, const SystemInfo *info);
void print_info_line(const char *label, const char *value, const FastfetchConfig *config);
void print_colored(const char *text, const char *color);
void print_usage(void);

#endif /* _FASTFETCH_H */