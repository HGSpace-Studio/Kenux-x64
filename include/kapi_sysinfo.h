#ifndef KAPI_SYSINFO_H
#define KAPI_SYSINFO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_SYS_NAME_MAX    64
#define KAPI_SYS_VERSION_MAX 64
#define KAPI_SYS_HOSTNAME_MAX 256
#define KAPI_SYS_DOMAIN_MAX   256

typedef struct {
    char sysname[KAPI_SYS_NAME_MAX];
    char nodename[KAPI_SYS_HOSTNAME_MAX];
    char release[KAPI_SYS_VERSION_MAX];
    char version[KAPI_SYS_VERSION_MAX];
    char machine[32];
    char domainname[KAPI_SYS_DOMAIN_MAX];
} kapi_utsname_t;

typedef struct {
    uint64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    char _f[20-2*sizeof(uint64_t)-sizeof(uint32_t)];
} kapi_sysinfo_t;

typedef struct {
    int cpu_count;
    int online_cpus;
    int active_cpus;
    int possible_cpus;
    int present_cpus;
    int cores_per_socket;
    int threads_per_core;
    int socket_count;
    char vendor_id[48];
    char brand_string[96];
    int family;
    int model;
    int stepping;
    int cache_size_l1d;
    int cache_size_l1i;
    int cache_size_l2;
    int cache_size_l3;
    double mhz;
    bool has_fpu;
    bool has_mmx;
    bool has_sse;
    bool has_sse2;
    bool has_sse3;
    bool has_sse4_1;
    bool has_sse4_2;
    bool has_avx;
    bool has_avx2;
    bool has_avx512f;
    bool has_aes;
    bool has_pclmulqdq;
    bool has_rdrand;
    bool has_rdseed;
    bool has_bmi1;
    bool has_bmi2;
    bool has_fma;
    bool has_popcnt;
    bool has_lzcnt;
    bool has_tsc_adjust;
    bool has_tsc_deadline;
    bool has_xsave;
    bool has_osxsave;
    bool has_vmx;
    bool has_svm;
    bool has_hypervisor;
    bool is_64bit;
    bool supports_ht;
    bool supports_turbo;
    bool supports_speed_step;
} kapi_cpuinfo_t;

typedef struct {
    int cpu_id;
    uint64_t user_time;
    uint64_t nice_time;
    uint64_t system_time;
    uint64_t idle_time;
    uint64_t iowait_time;
    uint64_t irq_time;
    uint64_t softirq_time;
    uint64_t steal_time;
    uint64_t guest_time;
    uint64_t guest_nice_time;
    uint64_t total_time;
    double user_percent;
    double system_percent;
    double idle_percent;
    double iowait_percent;
    double irq_percent;
    double steal_percent;
} kapi_cpu_stats_t;

typedef struct {
    uint64_t total_bytes_read;
    uint64_t total_bytes_written;
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_errors;
    uint64_t write_errors;
    uint64_t read_merge_ops;
    uint64_t write_merge_ops;
    uint64_t time_spent_reading_ms;
    uint64_t time_spent_writing_ms;
    uint64_t time_spent_io_ms;
    uint64_t weighted_time_spent_io_ms;
    uint64_t current_ios_in_progress;
    uint64_t io_time_ms;
    uint64_t weighted_io_time_ms;
    double read_throughput_mb;
    double write_throughput_mb;
    double avg_queue_length;
    double avg_wait_time_ms;
    double avg_service_time_ms;
    double utilization_percent;
} kapi_disk_stats_t;

typedef struct {
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t packets_received;
    uint64_t packets_sent;
    uint64_t receive_errors;
    uint64_t transmit_errors;
    uint64_t receive_dropped;
    uint64_t transmit_dropped;
    uint64_t receive_fifo_errors;
    uint64_t transmit_fifo_errors;
    uint64_t frame_errors;
    uint64_t collisions;
    uint64_t multicast_received;
    uint64_t multicast_sent;
    uint64_t carrier_losses;
    uint64_t compressed_packets_rx;
    uint64_t compressed_packets_tx;
    double rx_throughput_mbps;
    double tx_throughput_mbps;
    double error_rate;
    double collision_rate;
    double drop_rate;
} kapi_net_stats_t;

typedef struct {
    int pid;
    int ppid;
    int state;
    int priority;
    int nice_value;
    uint64_t vsize;
    uint64_t rss;
    uint64_t minflt;
    uint64_t majflt;
    uint64_t utime;
    uint64_t stime;
    uint64_t cutime;
    uint64_t cstime;
    uint64_t priority_real;
    uint64_t num_threads;
    uint64_t itrealvalue;
    uint64_t starttime;
    uint64_t vsize_kb;
    uint64_t rss_pages;
    uint64_t rsslim;
    uint64_t startcode;
    uint64_t endcode;
    uint64_t startstack;
    uint64_t kstkesp;
    uint64_t kstkeip;
    uint64_t signal;
    uint64_t blocked;
    uint64_t sigignore;
    uint64_t sigcatch;
    uint64_t wchan;
    uint64_t nswap;
    uint64_t cnswap;
    int exit_signal;
    int processor;
    int rt_priority;
    int policy;
    uint64_t delayacct_blkio_ticks;
    uint64_t guest_time;
    uint64_t cguest_time;
    char comm[256];
    char state_char;
    double cpu_usage;
    double mem_usage;
} kapi_proc_stat_t;

typedef struct {
    uint64_t context_switches;
    uint64_t forks;
    uint64_t interrupts;
    uint64_t cpu migrations;
    uint64_t page_faults_major;
    uint64_t page_faults_minor;
    uint64_t processes_created;
    uint64_t processes_exited;
    uint64_t boot_time;
    uint64_t total_processes;
    uint64_t running_processes;
    uint64_t sleeping_processes;
    uint64_t stopped_processes;
    uint64_t zombie_processes;
    double load_avg_1min;
    double load_avg_5min;
    double load_avg_15min;
    double cpu_utilization_total;
    double memory_utilization;
    double swap_utilization;
    int process_count;
    int thread_count;
    int fd_count;
    int inode_count;
    int dentry_count;
} kapi_system_stats_t;

int kapi_uname(kapi_utsname_t* buf);

const char* kapi_get_sysname(void);

const char* kapi_get_release(void);

const char* kapi_get_version_str(void);

const char* kapi_get_machine(void);

const char* kapi_get_nodename(void);

int kapi_set_nodename(const char* name);

const char* kapi_get_domainname(void);

int kapi_set_domainname(const char* name);

int kapi_sysinfo(kapi_sysinfo_t* info);

uint64_t kapi_uptime(void);

void kapi_uptime_detail(uint64_t* uptime, uint64_t* idle);

double kapi_load_average(int which);

int kapi_get_cpu_info(kapi_cpuinfo_t* info);

int kapi_get_cpu_stats(int cpu, kapi_cpu_stats_t* stats);

int kapi_get_all_cpu_stats(kapi_cpu_stats_t* stats, int count);

double kapi_cpu_usage(int cpu);

double kapi_cpu_usage_total(void);

int kapi_set_cpu_affinity(int pid, uint64_t mask);

uint64_t kapi_get_cpu_affinity(int pid);

int kapi_set_process_priority(int pid, int priority);

int kapi_get_process_priority(int pid);

int kapi_set_process_nice(int pid, int nice);

int kapi_get_process_nice(int pid);

int kapi_get_disk_stats(const char* device, kapi_disk_stats_t* stats);

int kapi_get_net_stats(const char* interface, kapi_net_stats_t* stats);

int kapi_get_proc_stat(int pid, kapi_proc_stat_t* stat);

int kapi_get_proc_status(int pid, char* status, size_t size);

int kapi_get_proc_cmdline(int pid, char* cmdline, size_t size);

int kapi_get_proc_exe(int pid, char* exe, size_t size);

int kapi_get_proc_cwd(int pid, char* cwd, size_t size);

char** kapi_get_proc_environ(int pid);

int kapi_get_proc_maps(int pid, void** maps, int max_maps);

int kapi_get_proc_fds(int pid, int* fds, int max_fds);

int kapi_get_proc_children(int pid, int* children, int max_children);

int kapi_get_system_stats(kapi_system_stats_t* stats);

int kapi_get_memory_info(uint64_t* total, uint64_t* free, uint64_t* available,
                         uint64_t* buffers, uint64_t* cached);

int kapi_get_swap_info(uint64_t* total, uint64_t* free);

int kapi_get_process_list(int* pids, int max_pids);

int kapi_get_thread_list(int pid, int* tids, int max_tids);

int kapi_kill_all(int signum);

int kapi_reboot(int cmd);

int kapi_power_off(void);

int kapi_halt(void);

int kapi_suspend(enum suspend_state state);

int kapi_get_boot_time(time_t* boot_time);

int kapi_get_timezone(long* tz_minuteswest, int* dst);

int kapi_set_timezone(long tz_minuteswest, int dst);

int kapi_get_hostname(char* name, size_t len);

int kapi_set_hostname(const char* name);

#ifdef __cplusplus
}
#endif

#endif