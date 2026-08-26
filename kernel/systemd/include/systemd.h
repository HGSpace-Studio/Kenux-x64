#ifndef KENUX_SYSTEMD_H
#define KENUX_SYSTEMD_H

#include <arch/types.h>
#include <kapi_list.h>
#include <arch/spinlock.h>
#include <timer.h>

#define time_get_timestamp() timer_get_jiffies()
#define time_sleep(sec) msleep((sec) * 1000)
#define spinlock_init spin_init
#define spinlock_lock spin_lock
#define spinlock_unlock spin_unlock

#define SYSTEMD_MAX_SERVICES     32
#define SYSTEMD_MAX_TARGETS      16
#define SYSTEMD_MAX_SOCKETS      16
#define SYSTEMD_MAX_TIMERS       16
#define SYSTEMD_MAX_MOUNTS       16
#define SYSTEMD_MAX_SLICE        8
#define SYSTEMD_MAX_DEVICE       16
#define SYSTEMD_MAX_AUTOMOUNT    8
#define SYSTEMD_MAX_PATH         8
#define SYSTEMD_MAX_SWAP         8
#define SYSTEMD_MAX_UNITS        64
#define SYSTEMD_MAX_ENV_VARS     16
#define SYSTEMD_MAX_CMDLINE      256
#define SYSTEMD_MAX_PATH_LEN     256
#define SYSTEMD_MAX_NAME         64
#define SYSTEMD_MAX_DESCRIPTION  128
#define SYSTEMD_MAX_DEPENDENCIES 16
#define SYSTEMD_MAX_EXEC_COMMANDS 4
#define SYSTEMD_MAX_ARGS         16

#define list_head_t kapi_list_head_t
#define INIT_LIST_HEAD(x) kapi_list_init(x)
#define list_add_tail(new, head) kapi_list_add_tail(new, head)
#define list_for_each(pos, head) kapi_list_for_each(pos, head)
#define list_entry(ptr, type, member) kapi_list_entry(ptr, type, member)

typedef enum {
    UNIT_TYPE_SERVICE,
    UNIT_TYPE_TARGET,
    UNIT_TYPE_SOCKET,
    UNIT_TYPE_TIMER,
    UNIT_TYPE_MOUNT,
    UNIT_TYPE_SLICE,
    UNIT_TYPE_DEVICE,
    UNIT_TYPE_AUTOMOUNT,
    UNIT_TYPE_PATH,
    UNIT_TYPE_SWAP,
    UNIT_TYPE_MAX,
} unit_type_t;

typedef enum {
    UNIT_STATE_DEAD,
    UNIT_STATE_LOADING,
    UNIT_STATE_LOADED,
    UNIT_STATE_ACTIVATING,
    UNIT_STATE_ACTIVE,
    UNIT_STATE_DEACTIVATING,
    UNIT_STATE_INACTIVE,
    UNIT_STATE_FAILED,
    UNIT_STATE_EXITED,
    UNIT_STATE_RUNNING,
    UNIT_STATE_CONFIRMED,
    UNIT_STATE_AWAITING,
} unit_state_t;

typedef enum {
    SERVICE_TYPE_SIMPLE,
    SERVICE_TYPE_FORKING,
    SERVICE_TYPE_ONESHOT,
    SERVICE_TYPE_DBUS,
    SERVICE_TYPE_NOTIFY,
    SERVICE_TYPE_IDLE,
    SERVICE_TYPE_MAX,
} service_type_t;

typedef enum {
    SERVICE_EXIT_SUCCESS,
    SERVICE_EXIT_FAILURE,
    SERVICE_EXIT_SIGNALED,
    SERVICE_EXIT_CORE_DUMPED,
} service_exit_type_t;

typedef enum {
    RESTART_NO,
    RESTART_ALWAYS,
    RESTART_ON_SUCCESS,
    RESTART_ON_FAILURE,
    RESTART_ON_ABNORMAL,
    RESTART_ON_WATCHDOG,
    RESTART_ON_ABORT,
} service_restart_t;

typedef enum {
    KILL_MODE_CONTROL_GROUP,
    KILL_MODE_PROCESS,
    KILL_MODE_MIXED,
    KILL_MODE_NONE,
} kill_mode_t;

typedef enum {
    SOCKET_TYPE_STREAM,
    SOCKET_TYPE_DGRAM,
    SOCKET_TYPE_SEQPACKET,
    SOCKET_TYPE_RAW,
    SOCKET_TYPE_RDM,
} socket_type_t;

typedef enum {
    SOCKET_PROTOCOL_TCP,
    SOCKET_PROTOCOL_UDP,
    SOCKET_PROTOCOL_UNIX,
} socket_protocol_t;

typedef enum {
    TIMER_REALTIME,
    TIMER_MONOTONIC,
    TIMER_BOOTTIME,
    TIMER_REALTIME_ALARM,
    TIMER_BOOTTIME_ALARM,
} timer_clock_t;

typedef enum {
    TIMER_ACCURACY_ALIGNMENT_NONE,
    TIMER_ACCURACY_ALIGNMENT_SECOND,
    TIMER_ACCURACY_ALIGNMENT_MINUTE,
    TIMER_ACCURACY_ALIGNMENT_HOUR,
} timer_accuracy_t;

typedef enum {
    SLICE_TYPE_SYSTEM,
    SLICE_TYPE_USER,
    SLICE_TYPE_MACHINE,
    SLICE_TYPE_MAX,
} slice_type_t;

typedef enum {
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_CHAR,
    DEVICE_TYPE_MAX,
} device_type_t;

typedef enum {
    MOUNT_FLAGS_READ_ONLY = 1,
    MOUNT_FLAGS_NOEXEC = 2,
    MOUNT_FLAGS_NODEV = 4,
    MOUNT_FLAGS_NOSUID = 8,
    MOUNT_FLAGS_NOATIME = 16,
    MOUNT_FLAGS_RELATIME = 32,
} mount_flags_t;

typedef enum {
    AUTOMOUNT_TYPE_PATH,
    AUTOMOUNT_TYPE_FSTAB,
} automount_type_t;

typedef struct {
    char name[SYSTEMD_MAX_NAME];
    char value[SYSTEMD_MAX_PATH_LEN];
} env_var_t;

typedef struct {
    char command[SYSTEMD_MAX_CMDLINE];
    char* argv[SYSTEMD_MAX_ARGS];
    int argc;
} exec_command_t;

typedef struct {
    char name[SYSTEMD_MAX_NAME];
    unit_type_t type;
    int satisfied;
} dependency_t;

typedef struct {
    char name[SYSTEMD_MAX_NAME];
    char description[SYSTEMD_MAX_DESCRIPTION];
    char documentation[SYSTEMD_MAX_PATH_LEN];
    char source_path[SYSTEMD_MAX_PATH_LEN];
    unit_type_t type;
    unit_state_t state;
    bool enabled;
    bool masked;
    bool allow_isolate;
    bool default_dependencies;
    
    dependency_t wants[SYSTEMD_MAX_DEPENDENCIES];
    int wants_count;
    dependency_t requires[SYSTEMD_MAX_DEPENDENCIES];
    int requires_count;
    dependency_t before[SYSTEMD_MAX_DEPENDENCIES];
    int before_count;
    dependency_t after[SYSTEMD_MAX_DEPENDENCIES];
    int after_count;
    dependency_t binds_to[SYSTEMD_MAX_DEPENDENCIES];
    int binds_to_count;
    dependency_t conflicts[SYSTEMD_MAX_DEPENDENCIES];
    int conflicts_count;
    dependency_t wants_wants[SYSTEMD_MAX_DEPENDENCIES];
    int wants_wants_count;
    dependency_t wants_requires[SYSTEMD_MAX_DEPENDENCIES];
    int wants_requires_count;
    
    list_head_t unit_list;
    uint64_t load_time;
    uint64_t active_time;
    uint64_t inactive_time;
    int start_count;
    int exit_code;
    
    int reload_result;
    int activation_result;
} unit_t;

typedef struct {
    unit_t base;
    
    service_type_t type;
    service_restart_t restart;
    kill_mode_t kill_mode;
    int kill_signal;
    int final_sigterm_timeout;
    int final_sigkill_timeout;
    
    exec_command_t exec_start_pre[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_start_pre_count;
    exec_command_t exec_start[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_start_count;
    exec_command_t exec_start_post[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_start_post_count;
    exec_command_t exec_stop[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_stop_count;
    exec_command_t exec_stop_post[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_stop_post_count;
    exec_command_t exec_reload[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_reload_count;
    exec_command_t exec_condition[SYSTEMD_MAX_EXEC_COMMANDS];
    int exec_condition_count;
    
    char working_dir[SYSTEMD_MAX_PATH_LEN];
    char user[64];
    char group[64];
    uint64_t uid;
    uint64_t gid;
    bool dynamic_user;
    bool transient;
    
    env_var_t env[SYSTEMD_MAX_ENV_VARS];
    int env_count;
    char environment_file[SYSTEMD_MAX_PATH_LEN];
    
    uint64_t restart_sec;
    uint64_t restart_usec;
    uint64_t start_limit_interval;
    uint64_t start_limit_burst;
    uint64_t start_limit_count;
    uint64_t timeout_start_sec;
    uint64_t timeout_stop_sec;
    uint64_t timeout_abort_sec;
    time_t last_start_time;
    time_t last_exit_time;
    time_t watchdog_last_ping;
    time_t last_watchdog_time;
    
    int exit_code;
    service_exit_type_t exit_type;
    int restart_count;
    bool auto_restart;
    bool success_exit_status[256];
    bool failure_action;
    
    uint64_t pid;
    bool running;
    bool stopping;
    bool reloading;
    bool watchdog_enabled;
    
    int main_pid;
    int control_pid;
    uint64_t watchdog_usec;
    bool watchdog_ping;
    
    struct cgroup* cgroup;
    struct unix_sock* watchdog_sock;
    struct unix_sock* notify_sock;
    
    int nice;
    int oom_score_adj;
    uint64_t cpu_shares;
    uint64_t memory_limit;
    uint64_t memory_max;
    uint64_t memory_high;
    uint64_t memory_low;
    mode_t umask;
    
    char capabilities[256];
    char ambient_capabilities[256];
    bool capability_bounding_set;
    bool ambient_capabilities_enabled;
    
    char limit_cpu[64];
    char limit_memory[64];
    char limit_blockio[64];
    char limit_nofile[64];
    char limit_nproc[64];
    char limit_stack[64];
    
    bool standard_input;
    bool standard_output;
    bool standard_error;
    char standard_input_path[SYSTEMD_MAX_PATH_LEN];
    char standard_output_path[SYSTEMD_MAX_PATH_LEN];
    char standard_error_path[SYSTEMD_MAX_PATH_LEN];
    
    char slice[SYSTEMD_MAX_NAME];
    char cgroup_path[SYSTEMD_MAX_PATH_LEN];
    
    list_head_t sockets;
    list_head_t cgroup_list;
    list_head_t service_list;
    
    bool needs_stop_when_unmasked;
    bool stop_when_unneeded;
    bool restart_prevent_exit_status[256];
} service_t;

typedef struct {
    unit_t base;
    list_head_t services;
    list_head_t targets;
    list_head_t sockets;
} target_t;

typedef struct {
    unit_t base;
    
    char socket_path[SYSTEMD_MAX_PATH_LEN];
    char listen_addr[512];
    uint64_t port;
    socket_type_t type;
    socket_protocol_t protocol;
    int socket_fd;
    bool activated;
    bool listen_stream;
    bool listen_datagram;
    bool accept;
    int backlog;
    int bind_ipv6_only;
    bool transparent;
    bool broadcast;
    bool tproxy;
    
    char service_name[SYSTEMD_MAX_NAME];
    list_head_t service_link;
    
    struct unix_sock* unix_sock;
    int accept_thread_id;
    int socket_count;
    int fd_max;
    
    char bind_to_device[64];
    char priority[32];
} systemd_socket_t;

typedef struct {
    unit_t base;
    
    char on_unit[SYSTEMD_MAX_NAME];
    timer_clock_t clock;
    timer_accuracy_t accuracy;
    
    char on_calendar[256];
    char on_active_sec[128];
    char on_boot_sec[128];
    char on_startup_sec[128];
    char on_unit_active_sec[128];
    char on_unit_inactive_sec[128];
    
    uint64_t interval_sec;
    uint64_t interval_usec;
    uint64_t random_delay_sec;
    uint64_t accuracy_sec;
    uint64_t wake_system_usec;
    
    bool persistent;
    bool wake_system;
    bool remain_after_elapse;
    bool monotonic;
    
    uint64_t next_elapse;
    uint64_t last_elapse;
    uint64_t base_elapse;
    
    char unit_name[SYSTEMD_MAX_NAME];
    
    uint32_t n_elapsed;
    bool expired;
    
    uint64_t next_elapse_time;
    uint64_t last_trigger_time;
    uint64_t wake_time;
    int trigger_count;
    int triggers_left;
    int total_triggers;
    char calendar[SYSTEMD_MAX_PATH_LEN];
    uint64_t accuracy_usec;
    char unit[SYSTEMD_MAX_NAME];
} timer_t;

typedef struct {
    unit_t base;
    
    char what[SYSTEMD_MAX_PATH_LEN];
    char where[SYSTEMD_MAX_PATH_LEN];
    char type[64];
    char options[1024];
    uint64_t dump_freq;
    uint64_t pass_num;
    
    bool nofail;
    bool noauto;
    bool lazy;
    bool x_initrd;
    bool force;
    
    mount_flags_t mount_flags;
    
    int mount_result;
    bool needs_mount;
} mount_t;

typedef struct {
    unit_t base;
    
    slice_type_t type;
    char parent[SYSTEMD_MAX_NAME];
    char controller[SYSTEMD_MAX_NAME];
    
    uint64_t cpu_weight;
    uint64_t io_weight;
    uint64_t memory_weight;
    uint64_t cpu_max;
    uint64_t io_max;
    uint64_t memory_max;
    
    bool allow_kill;
    bool delegate;
    
    struct cgroup* cgroup;
    list_head_t children;
    list_head_t tasks;
} slice_t;

typedef struct {
    unit_t base;
    
    device_type_t device_type;
    char devpath[SYSTEMD_MAX_PATH_LEN];
    char sysname[64];
    char devname[64];
    char subsystem[64];
    
    int major;
    int minor;
    
    uint64_t pci_addr;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
    uint8_t pci_class;
    uint8_t pci_subclass;
    
    char driver[64];
    char udev_rules[256];
    
    bool nofail;
    bool noauto;
} device_t;

typedef struct {
    unit_t base;
    
    char where[SYSTEMD_MAX_PATH_LEN];
    char directory_mode[16];
    automount_type_t type;
    
    char timeout_idle_sec[64];
    uint64_t timeout_usec;
    
    bool nofail;
    bool lazy;
    
    char mount_unit[SYSTEMD_MAX_NAME];
    
    bool active;
    bool expired;
} automount_t;

typedef struct {
    unit_t base;
    
    char path[SYSTEMD_MAX_PATH_LEN];
    bool exists;
    bool exists_glob;
    bool changed;
    bool modified;
    bool is_directory;
    
    char service_name[SYSTEMD_MAX_NAME];
    
    uint64_t inotify_wd;
} systemd_path_t;

typedef struct {
    unit_t base;
    
    char what[SYSTEMD_MAX_PATH_LEN];
    char options[1024];
    
    bool nofail;
    bool noauto;
    bool x_initrd;
    
    bool swapon_done;
} swap_t;

typedef struct {
    char source[SYSTEMD_MAX_PATH_LEN];
    char destination[SYSTEMD_MAX_PATH_LEN];
    char fstype[64];
    char options[256];
    uint64_t dump_freq;
    uint64_t pass_num;
} fstab_entry_t;

typedef struct {
    time_t timestamp;
    char unit[SYSTEMD_MAX_NAME];
    char message[4096];
    uint32_t priority;
    uint64_t pid;
    char comm[64];
    char hostname[64];
    uint32_t code;
    uint32_t errno;
} journal_entry_t;

typedef struct {
    int priority;
    char unit[SYSTEMD_MAX_NAME];
    uint64_t since;
    uint64_t until;
    bool reverse;
} journal_query_t;

typedef struct {
    spinlock_t lock;
    
    service_t services[SYSTEMD_MAX_SERVICES];
    int service_count;
    target_t targets[SYSTEMD_MAX_TARGETS];
    int target_count;
    systemd_socket_t sockets[SYSTEMD_MAX_SOCKETS];
    int socket_count;
    timer_t timers[SYSTEMD_MAX_TIMERS];
    int timer_count;
    mount_t mounts[SYSTEMD_MAX_MOUNTS];
    int mount_count;
    slice_t slices[SYSTEMD_MAX_SLICE];
    int slice_count;
    device_t devices[SYSTEMD_MAX_DEVICE];
    int device_count;
    automount_t automounts[SYSTEMD_MAX_AUTOMOUNT];
    int automount_count;
    systemd_path_t paths[SYSTEMD_MAX_PATH];
    int path_count;
    swap_t swaps[SYSTEMD_MAX_SWAP];
    int swap_count;
    
    unit_t* units[SYSTEMD_MAX_UNITS];
    int unit_count;
    
    char default_target[SYSTEMD_MAX_NAME];
    char emergency_target[SYSTEMD_MAX_NAME];
    char rescue_target[SYSTEMD_MAX_NAME];
    
    fstab_entry_t fstab[256];
    int fstab_count;
    
    bool initialized;
    bool running;
    bool shutting_down;
    bool emergency_mode;
    
    list_head_t unit_list_head;
    
    uint64_t boot_id[16];
    char machine_id[32];
    
    uint64_t manager_pid;
    char manager_version[64];
    
    bool log_target_console;
    bool log_target_kmsg;
    bool log_target_journal;
    
    uint32_t log_level;
    
    bool first_boot;
    bool first_boot_done;
    
    char root_device[SYSTEMD_MAX_PATH_LEN];
    char root_mount_options[256];
} systemd_t;

extern systemd_t systemd;

void systemd_init(void);
void systemd_run(void);
void systemd_shutdown(void);
int systemd_start_target(const char* name);
int systemd_start_service(const char* name);
int systemd_stop_service(const char* name);
int systemd_restart_service(const char* name);
int systemd_reload_service(const char* name);
int systemd_enable_service(const char* name);
int systemd_disable_service(const char* name);
int systemd_list_services(char* buffer, int size);
int systemd_get_service_status(const char* name, char* buffer, int size);
unit_state_t systemd_get_service_state(const char* name);
int systemd_load_units_from_dir(const char* dir);
int systemd_isolate_target(const char* name);
int systemd_start_unit(const char* name);
int systemd_stop_unit(const char* name);
int systemd_reload_unit(const char* name);
int systemd_restart_unit(const char* name);
int systemd_list_units(char* buffer, int size);
int systemd_list_units_by_type(char* buffer, int size, unit_type_t type);
int systemd_daemon_reload(void);
int systemd_kill_unit(const char* name, int signal);
int systemd_show_unit(const char* name, char* buffer, int size);
int systemd_enable_unit(const char* name);
int systemd_disable_unit(const char* name);
int systemd_reload_config(void);
int systemd_isolate(const char* name);

int service_load(const char* path);
int service_start(service_t* service);
int service_stop(service_t* service);
int service_kill(service_t* service, int signal);
int service_reload(service_t* service);
void service_monitor(service_t* service);
int service_run_command(exec_command_t* cmd, const char* working_dir);

int target_load(const char* path);
int target_start(target_t* target);

int socket_load(const char* path);
int socket_start(systemd_socket_t* socket);
int socket_stop(systemd_socket_t* socket);
int socket_activate(systemd_socket_t* socket);
int socket_listen(systemd_socket_t* socket);

int timer_load(const char* path);
int timer_start(timer_t* timer);
int timer_stop(timer_t* timer);
void timer_monitor(timer_t* timer);

int mount_load(const char* path);
int mount_start(mount_t* mount);
int mount_stop(mount_t* mount);

int slice_load(const char* path);
int slice_start(slice_t* slice);

int device_load(const char* path);
int device_start(device_t* device);

int automount_load(const char* path);
int automount_start(automount_t* automount);

int path_load(const char* path);
int path_start(systemd_path_t* path);

int swap_load(const char* path);
int swap_start(swap_t* swap);

int fstab_load(const char* path);
int fstab_mount_all(void);

int udev_load_rules(const char* path);
int udev_scan_devices(void);

void journal_init(bool persist);
void journal_log(const char* unit, const char* message, int priority);
void journal_flush(void);
void journal_close(void);
int journal_query(journal_query_t* query, journal_entry_t* entries, int max_entries);
int journal_count(journal_query_t* query);
void journal_clear(void);

int fs_read_file_content(const char* path, char* buffer, int size);
int fs_list_dir(const char* path, char* entries, int size);
char* trim(char* str);
char* trim_left(char* str);
char* trim_right(char* str);
int parse_ini_section(const char* line, char* section, int size);
int parse_ini_keyvalue(const char* line, char* key, int key_size, char* value, int value_size);
int parse_dependency_list(const char* value, dependency_t* deps, int* count, int max);
int parse_unit_section(unit_t* unit, const char* key, const char* value);
int parse_install_section(unit_t* unit, const char* key, const char* value);
void parse_exec_command(const char* cmdline, exec_command_t* exec);

#define LOG_EMERG    0
#define LOG_ALERT    1
#define LOG_CRIT     2
#define LOG_ERR      3
#define LOG_WARNING  4
#define LOG_NOTICE   5
#define LOG_INFO     6
#define LOG_DEBUG    7

#define CPU_SHARES_DEFAULT 1024
#define MEM_LIMIT_MAX ((uint64_t)-1)

extern int thread_create(void* entry, void* arg);

unit_t* find_unit(const char* name);

#endif