/*
 * Kenux OS - systemd user-space tools (systemctl, journalctl, loginctl)
 * Header file - complements the kernel/systemd C sources
 */

#ifndef _SYSTEMD_TOOLS_H
#define _SYSTEMD_TOOLS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Windows/MinGW fallback: uid_t and gid_t may not be defined by sys/types.h */
#ifdef _WIN32
  #ifndef _UID_T_DEFINED
    typedef int uid_t;
    #define _UID_T_DEFINED
  #endif
  #ifndef _GID_T_DEFINED
    typedef int gid_t;
    #define _GID_T_DEFINED
  #endif
#endif

#define SYSTEMD_TOOLS_VERSION_STR "KenuxK-systemd 255.3 (user-space tools)"
#define SYSCTL_MAX_NAME_LEN 256
#define SYSCTL_MAX_UNITS 256
#define SYSCTL_MAX_DEPS 16
#define SYSCTL_MAX_LINE 4096
#define SYSCTL_MAX_JOB_ID 1024
#define SYSCTL_MAX_CGROUP_LEN 512
#define SYSCTL_MAX_EXEC 4
#define JOURNAL_MAX_PATH 4096
#define JOURNAL_MAX_FIELDS 256
#define JOURNAL_MAX_ENTRIES (1 << 20)
#define LOGINCTL_MAX_SESSIONS 128
#define LOGINCTL_MAX_USERS 128
#define LOGINCTL_MAX_MACHINES 32
#define LOGINCTL_MAX_SEATS 32

/* ---------- Unit system ---------- */
typedef enum {
    UNIT_SERVICE = 0,
    UNIT_SOCKET,
    UNIT_TARGET,
    UNIT_DEVICE,
    UNIT_MOUNT,
    UNIT_AUTOMOUNT,
    UNIT_SWAP,
    UNIT_TIMER,
    UNIT_PATH,
    UNIT_SLICE,
    UNIT_SCOPE,
    UNIT_BUSNAME,
    UNIT_NETWORK,
    UNIT_NETDEV,
    UNIT_LINK,
    UNIT_TEMPLATE,  /* @instance */
    UNIT_TYPE_COUNT
} UnitType;

typedef enum {
    UNIT_ACTIVE_UNKNOWN = 0,
    UNIT_ACTIVE_ACTIVE,
    UNIT_ACTIVE_RELOADING,
    UNIT_ACTIVE_INACTIVE,
    UNIT_ACTIVE_FAILED,
    UNIT_ACTIVE_ACTIVATING,
    UNIT_ACTIVE_DEACTIVATING,
    UNIT_ACTIVE_MAINTENANCE,
} UnitActiveState;

typedef enum {
    UNIT_SUB_UNKNOWN = 0,
    /* service */
    UNIT_SUB_RUNNING,
    UNIT_SUB_DEAD,
    UNIT_SUB_START_PRE,
    UNIT_SUB_START,
    UNIT_SUB_START_POST,
    UNIT_SUB_EXITED,
    UNIT_SUB_RELOAD,
    UNIT_SUB_STOP,
    UNIT_SUB_STOP_WATCHDOG,
    UNIT_SUB_STOP_SIGTERM,
    UNIT_SUB_STOP_SIGKILL,
    UNIT_SUB_FINAL_SIGTERM,
    UNIT_SUB_FINAL_SIGKILL,
    UNIT_SUB_FAILED,
    UNIT_SUB_AUTO_RESTART,
    /* mount */
    UNIT_SUB_MOUNTED,
    UNIT_SUB_MOUNTING,
    UNIT_SUB_UNMOUNTING,
    UNIT_SUB_MOUNTING_DONE,
    UNIT_SUB_REMOUNTING,
    /* target */
    UNIT_SUB_DEAD_TARGET,
    UNIT_SUB_ACTIVE_TARGET,
    /* socket */
    UNIT_SUB_LISTENING,
    /* timer */
    UNIT_SUB_WAITING,
    UNIT_SUB_ELAPSED,
    /* swap */
    UNIT_SUB_ACTIVE_SWAP,
    /* path */
    UNIT_SUB_WAITING_PATH,
    /* automount */
    UNIT_SUB_WAITING_AUTOMOUNT,
    /* device */
    UNIT_SUB_PLUGGED,
    /* scope */
    UNIT_SUB_ABANDONED,
    /* slice */
    UNIT_SUB_ACTIVE_SLICE,
} UnitSubState;

typedef enum {
    UNIT_LOAD_STUB = 0,
    UNIT_LOAD_LOADED,
    UNIT_LOAD_NOT_FOUND,
    UNIT_LOAD_ERROR,
    UNIT_LOAD_MERGED,
    UNIT_LOAD_MASKED,
} UnitLoadState;

typedef enum {
    UNIT_START_NO = 0,
    UNIT_START_REPLACE,     /* isolate / start with replace-irreversibly */
    UNIT_START_IRREVERSIBLE,
    UNIT_START_ISOLATE,
    UNIT_START_FAIL,
} UnitStartMode;

typedef enum {
    JOB_WAITING = 0,
    JOB_RUNNING,
    JOB_DONE,
    JOB_FAILED,
} JobState;

typedef enum {
    JOB_START = 0,
    JOB_STOP,
    JOB_RELOAD,
    JOB_RESTART,
    JOB_TRY_RESTART,
    JOB_RELOAD_OR_RESTART,
    JOB_TRY_RELOAD_OR_RESTART,
    JOB_ISOLATE,
    JOB_KILL,
    JOB_TYPE_COUNT
} JobType;

typedef struct Unit Unit;
typedef struct Job Job;

/* Kill / restart / watchdog modes */
typedef enum {
    KILL_CONTROL_GROUP = 0,
    KILL_MIXED,
    KILL_NONE,
    KILL_PROCESS,
} KillMode;

typedef enum {
    RESTART_NO = 0,
    RESTART_ON_SUCCESS,
    RESTART_ON_FAILURE,
    RESTART_ON_ABNORMAL,
    RESTART_ON_WATCHDOG,
    RESTART_ON_ABORT,
    RESTART_ALWAYS,
} RestartMode;

typedef enum {
    NOTIFY_NONE = 0,
    NOTIFY_MAIN,
    NOTIFY_ALL,
    NOTIFY_EXEC,
} NotifyAccess;

typedef struct {
    /* [Unit] */
    char description[1024];
    char documentation[4096];
    char requires[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  requires_count;
    char wants[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  wants_count;
    char binds_to[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  binds_to_count;
    char part_of[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  part_of_count;
    char requisite[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  requisite_count;
    char conflicts[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  conflicts_count;
    char before[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  before_count;
    char after[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  after_count;
    char on_failure[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  on_failure_count;
    int  stop_when_unneeded;
    int  start_limit_interval_s;
    int  start_limit_burst;
    /* [Install] */
    char wanted_by[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  wanted_by_count;
    char required_by[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  required_by_count;
    char also[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  also_count;
    char alias[SYSCTL_MAX_DEPS][SYSCTL_MAX_NAME_LEN];
    int  alias_count;
    char default_instance[256];
} UnitCommonData;

/* Service-specific */
typedef enum {
    SERVICE_SIMPLE = 0,
    SERVICE_FORKING,
    SERVICE_ONESHOT,
    SERVICE_DBUS,
    SERVICE_NOTIFY,
    SERVICE_IDLE,
    SERVICE_EXEC,
} ServiceType;

typedef struct {
    ServiceType type;
    char exec_start[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_start_count;
    char exec_start_pre[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_start_pre_count;
    char exec_start_post[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_start_post_count;
    char exec_reload[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_reload_count;
    char exec_stop[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_stop_count;
    char exec_stop_post[SYSCTL_MAX_EXEC][SYSCTL_MAX_LINE];
    int  exec_stop_post_count;
    char timeout_start_sec[64];
    char timeout_stop_sec[64];
    int  timeout_abort_sec;
    char runtime_max_sec[64];
    char watchdog_sec[64];
    char restart_sec[64];
    RestartMode restart;
    KillMode kill_mode;
    char kill_signal[32];
    char final_kill_signal[32];
    int send_sigkill;
    int send_sighup;
    char success_exit_status[256];
    char restart_prevent_exit_status[256];
    char restart_force_exit_status[256];
    NotifyAccess notify_access;
    char pid_file[SYSCTL_MAX_CGROUP_LEN];
    char bus_name[SYSCTL_MAX_NAME_LEN];
    char working_directory[SYSCTL_MAX_CGROUP_LEN];
    char root_directory[SYSCTL_MAX_CGROUP_LEN];
    char root_image[SYSCTL_MAX_CGROUP_LEN];
    char user[128];
    char group[128];
    char supplementary_groups[256];
    char dynamic_user;
    char nice[8];
    char oom_score_adjust[16];
    char cpu_shares[32];
    char cpu_quota[32];
    char cpu_weight[32];
    char memory_max[32];
    char memory_high[32];
    char memory_min[32];
    char memory_limit[32];
    char tasks_max[32];
    char io_weight[32];
    char slice[SYSCTL_MAX_NAME_LEN];
    char delegate[16];
    char standard_input[64];
    char standard_output[64];
    char standard_error[64];
    char syslog_identifier[256];
    char syslog_level_prefix[8];
    char tty_path[SYSCTL_MAX_CGROUP_LEN];
    char tmp_files[SYSCTL_MAX_CGROUP_LEN];
    char environment[SYSCTL_MAX_LINE];
    char environment_file[SYSCTL_MAX_CGROUP_LEN];
    char environment_files[8][SYSCTL_MAX_CGROUP_LEN];
    int  environment_file_count;
    int  pass_environment_count;
    char pass_environment[64][128];
    char umask[16];
    int  private_tmp;
    int  private_devices;
    int  private_users;
    int  private_mounts;
    int  protect_system;   /* 0 off, 1 true, 2 full, 3 strict */
    int  protect_home;     /* 0 off, 1 read-only, 2 tmpfs, 3 none */
    int  protect_kernel_tunables;
    int  protect_kernel_modules;
    int  protect_kernel_logs;
    int  protect_control_groups;
    int  protect_clock;
    int  protect_hostname;
    int  no_new_privileges;
    int  mount_apivfs;
    char read_only_paths[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  read_only_paths_count;
    char read_write_paths[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  read_write_paths_count;
    char bind_paths[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  bind_paths_count;
    char bind_ro_paths[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  bind_ro_paths_count;
    char capability_bounding_set[256];
    char ambient_capabilities[256];
    char system_call_filter[4096];
    char system_call_error_number[32];
    char restrict_address_families[256];
    char restrict_namespaces[64];
    int  memory_deny_write_execute;
    int  lock_personality;
    int  restrict_realtime;
    int  restrict_suid_sgid;
    char seccomp_profile[SYSCTL_MAX_CGROUP_LEN];
    char apparmor_profile[256];
    char smack_process_label[256];
    char selinux_context[256];
    /* live state */
    pid_t main_pid;
    pid_t control_pid;
    int   n_fds;
    int   n_tasks;
    long  memory_current_bytes;
    long  cpu_usage_us;
    uint64_t invocations;
    time_t active_enter_time;
    time_t active_exit_time;
    time_t inactive_exit_time;
    uint64_t active_sec_total;
} ServiceData;

/* Mount-specific */
typedef struct {
    char what[SYSCTL_MAX_CGROUP_LEN];
    char where[SYSCTL_MAX_CGROUP_LEN];
    char type[64];
    char options[4096];
    int  sloppy_options;
    int  lazy_unmount;
    int  read_write_only;
    int  force_unmount;
    char timeout_sec[64];
    /* live state: */
    int  is_mounted;
} MountData;

/* Automount-specific */
typedef struct {
    char where[SYSCTL_MAX_CGROUP_LEN];
    int  directory_mode;
    char timeout_idle_sec[64];
    int  result;
} AutomountData;

/* Swap-specific */
typedef struct {
    char what[SYSCTL_MAX_CGROUP_LEN];
    int  priority;
    int  is_swapon;
} SwapData;

/* Socket-specific */
typedef enum {
    SOCKET_SOCKET = 0,
    SOCKET_FIFO,
    SOCKET_SPECIAL,
    SOCKET_MQUEUE,
    SOCKET_POSIX_MQ,
    SOCKET_TRANSPORT_UNIT,
} SocketKind;

typedef struct {
    SocketKind kind;
    char listen[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  listen_count;
    char listen_stream[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  listen_stream_count;
    char listen_dgram[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  listen_dgram_count;
    char listen_seqpacket[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  listen_seqpacket_count;
    char bind_to_device[256];
    int  socket_user;
    int  socket_group;
    int  socket_mode;
    char accept;
    char pass_credentials;
    char pass_security;
    char tcp_congestion[32];
    char service[SYSCTL_MAX_NAME_LEN];
    /* live: */
    int fd_count;
} SocketData;

/* Timer-specific */
typedef struct {
    char on_active_sec[SYSCTL_MAX_DEPS][64];
    int  on_active_sec_count;
    char on_boot_sec[SYSCTL_MAX_DEPS][64];
    int  on_boot_sec_count;
    char on_startup_sec[SYSCTL_MAX_DEPS][64];
    int  on_startup_sec_count;
    char on_unit_active_sec[SYSCTL_MAX_DEPS][64];
    int  on_unit_active_sec_count;
    char on_unit_inactive_sec[SYSCTL_MAX_DEPS][64];
    int  on_unit_inactive_sec_count;
    char on_calendar[SYSCTL_MAX_DEPS][128]; /* e.g. "Mon..Fri 12:30:00" */
    int  on_calendar_count;
    char unit[SYSCTL_MAX_NAME_LEN];
    char persistent;
    char wake_system;
    char fixed_random_delay[64];
    char randomized_delay_sec[64];
    char accuracy_sec[64];
    /* live: */
    time_t next_elapse;
    time_t last_elapse;
    int    misses;
} TimerData;

/* Path-specific */
typedef struct {
    char path_exists[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  path_exists_count;
    char path_changed[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  path_changed_count;
    char path_modified[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  path_modified_count;
    char directory_not_empty[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  directory_not_empty_count;
    char unit[SYSCTL_MAX_NAME_LEN];
    char make_directory;
    char directory_mode[16];
} PathData;

/* Scope / Slice */
typedef struct {
    char cgroup_path[SYSCTL_MAX_CGROUP_LEN];
    char description[1024];
    int  pid_count;
    pid_t pids[256];
} ScopeData;

typedef struct {
    char memory_max[32];
    char cpu_weight[32];
    char tasks_max[32];
    char io_weight[32];
} SliceData;

/* Target / Device / Busname / Net* */
typedef struct {
    char netdev_kind[64];
    char netdev_name[256];
    char ifname[256];
    int  mtu;
    char mac[32];
} NetdevData;

struct Unit {
    char name[SYSCTL_MAX_NAME_LEN];
    char id[SYSCTL_MAX_NAME_LEN];
    char instance[SYSCTL_MAX_NAME_LEN];  /* for template@x.service */
    char instance_pattern[SYSCTL_MAX_NAME_LEN];
    UnitType type;

    UnitLoadState load_state;
    UnitActiveState active_state;
    UnitSubState sub_state;
    char sub_state_name[64];

    char fragment_path[SYSCTL_MAX_CGROUP_LEN];
    char dropin_paths[SYSCTL_MAX_DEPS][SYSCTL_MAX_CGROUP_LEN];
    int  dropin_count;

    time_t load_time;
    time_t active_enter_monotonic;
    time_t active_exit_monotonic;
    time_t inactive_enter_monotonic;
    time_t inactive_exit_monotonic;

    /* Job currently assigned */
    int job_id;
    JobType job_type;
    JobState job_state;
    char job_result[64];

    /* Common settings */
    UnitCommonData common;

    /* Type-specific */
    union {
        ServiceData service;
        MountData mount;
        AutomountData automount;
        SwapData swap;
        SocketData socket;
        TimerData timer;
        PathData path;
        ScopeData scope;
        SliceData slice;
        NetdevData netdev;
    } data;
};

/* Job */
struct Job {
    int id;
    JobType type;
    UnitType unit_type;
    char unit_name[SYSCTL_MAX_NAME_LEN];
    JobState state;
    int result; /* 0 ok, -1 failed */
    time_t created_at;
    time_t started_at;
    time_t elapsed_us;
    uint32_t dependencies[64];
    int dep_count;
    char errno_str[128];
};

/* ---------- Journal ---------- */
typedef enum {
    JOURNAL_FIELD_MESSAGE = 0,
    JOURNAL_FIELD_PRIORITY,
    JOURNAL_FIELD_CODE_FILE,
    JOURNAL_FIELD_CODE_LINE,
    JOURNAL_FIELD_CODE_FUNC,
    JOURNAL_FIELD_ERRNO,
    JOURNAL_FIELD_SYSLOG_FACILITY,
    JOURNAL_FIELD_SYSLOG_IDENTIFIER,
    JOURNAL_FIELD_SYSLOG_TIMESTAMP,
    JOURNAL_FIELD_SYSLOG_RAW,
    JOURNAL_FIELD_SYSLOG_PID,
    JOURNAL_FIELD__BOOT_ID,
    JOURNAL_FIELD__MACHINE_ID,
    JOURNAL_FIELD__HOSTNAME,
    JOURNAL_FIELD__KERNEL,
    JOURNAL_FIELD__TRANSPORT,
    JOURNAL_FIELD__UDEV_SYSNAME,
    JOURNAL_FIELD__UDEV_DEVNODE,
    JOURNAL_FIELD_PID,
    JOURNAL_FIELD_UID,
    JOURNAL_FIELD_GID,
    JOURNAL_FIELD_COMM,
    JOURNAL_FIELD_EXE,
    JOURNAL_FIELD_CMDLINE,
    JOURNAL_FIELD_CAP_EFFECTIVE,
    JOURNAL_FIELD_AUDIT_SESSION,
    JOURNAL_FIELD_AUDIT_LOGINUID,
    JOURNAL_FIELD_SELINUX_CONTEXT,
    JOURNAL_FIELD__SYSTEMD_CGROUP,
    JOURNAL_FIELD__SYSTEMD_UNIT,
    JOURNAL_FIELD__SYSTEMD_SLICE,
    JOURNAL_FIELD__SYSTEMD_SESSION,
    JOURNAL_FIELD__SYSTEMD_OWNER_UID,
    JOURNAL_FIELD_OBJECT_SYSTEMD_UNIT,
    JOURNAL_FIELD_MEMBER,
    JOURNAL_FIELD_COREDUMP_UNIT,
    JOURNAL_FIELD_TIME_USEC,
    JOURNAL_FIELD_REALTIME_USEC,
    JOURNAL_FIELD_MONOTONIC_USEC,
    JOURNAL_FIELD_CUSTOM_START = 1024,
} JournalFieldId;

typedef struct {
    uint64_t realtime_us;
    uint64_t monotonic_us;
    char boot_id[33];
    char machine_id[33];
    char hostname[256];
    char syslog_ident[256];
    int  syslog_facility;
    int  priority;           /* LOG_EMERG..LOG_DEBUG */
    pid_t pid;
    pid_t tid;
    uid_t uid;
    gid_t gid;
    char comm[64];
    char exe[JOURNAL_MAX_PATH];
    char cmdline[4096];
    char unit[SYSCTL_MAX_NAME_LEN];
    char slice[SYSCTL_MAX_NAME_LEN];
    char session_id[256];
    char cgroup[SYSCTL_MAX_CGROUP_LEN];
    char transport[64];  /* journal, syslog, stdout, kernel, driver */
    int  kernel_thread;
    int  errno_val;
    char code_file[512];
    int  code_line;
    char code_func[256];
    char message[SYSCTL_MAX_LINE];
    uint8_t raw_fields[8192];
    int raw_fields_len;
} JournalEntry;

/* journalctl filter flags */
typedef enum {
    JOURNAL_OUTPUT_SHORT = 0,
    JOURNAL_OUTPUT_SHORT_ISO,
    JOURNAL_OUTPUT_SHORT_PRECISE,
    JOURNAL_OUTPUT_SHORT_MONOTONIC,
    JOURNAL_OUTPUT_SHORT_UNIX,
    JOURNAL_OUTPUT_VERBOSE,
    JOURNAL_OUTPUT_EXPORT,
    JOURNAL_OUTPUT_JSON,
    JOURNAL_OUTPUT_JSON_PRETTY,
    JOURNAL_OUTPUT_JSON_SHORT,
    JOURNAL_OUTPUT_CAT,
    JOURNAL_OUTPUT_WITH_UNIT,
    JOURNAL_OUTPUT_JSON_SSE,
} JournalOutputFormat;

typedef struct {
    int reverse;
    int follow;
    int lines;                    /* -n, default latest 10 */
    int all;                      /* -a show all fields */
    int no_pager;
    int case_insensitive;
    int output_fields_count;
    char output_fields[64][64];   /* --output-fields=... */
    int merge;
    int utc;
    JournalOutputFormat output;
    /* Priority mask */
    int priority_min;              /* -p emerg..debug mask */
    int priority_max;
    /* Filter strings */
    char unit_filter[64][SYSCTL_MAX_NAME_LEN];
    int  unit_filter_count;
    char identifier_filter[64][256];
    int  identifier_filter_count;
    char pid_filter[64][16];
    int  pid_filter_count;
    char gid_filter[64][16];
    int  gid_filter_count;
    char uid_filter[64][16];
    int  uid_filter_count;
    char field_grep[64][SYSCTL_MAX_LINE];   /* arbitrary FIELD=value matches */
    int  field_grep_count;
    char boot_id_filter[64][33];
    int  boot_filter_count;
    int  this_boot;               /* -b */
    int  boot_offset;             /* -b -1 */
    /* Time bounds */
    char since[64];
    char until[64];
    uint64_t since_us;
    uint64_t until_us;
    /* --dmesg */
    int dmesg_only;
    int quiet_privileged;
    /* verify / check */
    int verify;
    int check;
    int show_cursor;
    char after_cursor[512];
    int list_boot_ids;
    int list_catalog;
    int catalog_message_id[64][64];
    int disk_usage;                 /* --disk-usage */
    int vacuum_size_mb;              /* --vacuum-size= */
    int vacuum_time_days;            /* --vacuum-time= */
    int vacuum_files;                /* --vacuum-files= */
    int rotate;
    int flush;
    int header_show;
} JournalctlOptions;

/* ---------- loginctl ---------- */
typedef enum {
    SESSION_UNKNOWN = 0,
    SESSION_LOGIN,
    SESSION_WAYLAND,
    SESSION_X11,
    SESSION_TTY,
    SESSION_SSH,
    SESSION_SERIAL,
    SESSION_MIRROR,
    SESSION_WEB,
} SessionType;

typedef enum {
    SESSION_STATE_OPENING = 0,
    SESSION_STATE_ACTIVE,
    SESSION_STATE_CLOSING,
    SESSION_STATE_ONLINE,
    SESSION_STATE_IDLE,
    SESSION_STATE_LINGERING,
} SessionState;

typedef struct SeatInfo Seat;   /* forward declaration for SessionInfo */

typedef struct {
    uint32_t session_id;       /* "c2", "102", stored numerically */
    char session_str[32];
    uid_t uid;
    char user[128];
    Seat *seat_id;
    Seat *seat_owner;
    char seat_name[64];
    char tty[64];
    char display[64];
    char remote_host[256];
    char remote_user[256];
    char service[256];
    char desktop_environment[64];
    SessionType type;
    SessionState state;
    int active;
    pid_t leader;
    pid_t audit_id;
    time_t login_time;
    char idle_hint;
    time_t idle_since_hint;
    char locked_hint;
    time_t since_timestamp;
    char class[64];              /* user, greeter, lock-screen, background */
    int vt_nr;
    uint64_t scope_start_time;
} SessionInfo;

typedef struct {
    uid_t uid;
    char name[128];
    char state[32];              /* offline, online, lingering, closing, active */
    int lingering;
    int sessions_count;
    uint32_t session_ids[16];
    char display[64];
    time_t last_login;
    time_t last_activity;
    pid_t manager_pid;
    char runtime_path[SYSCTL_MAX_CGROUP_LEN];
    char slice[SYSCTL_MAX_NAME_LEN];
    int n_processes;
} UserInfo;

typedef struct SeatInfo {
    char id[64];
    char path[512];
    char active_session[32];
    char available;
    int can_multi_session;
    int can_tty;
    int can_graphical;
    int active_vt;
    uint32_t session_ids[LOGINCTL_MAX_SESSIONS];
    int n_sessions;
    char seat_class[64];
} SeatInfo;

typedef struct {
    char name[128];
    char state[32];
    uid_t leader_uid;
    char class[32];
    char root_directory[512];
    char bind_mount_point[512];
    char service[SYSCTL_MAX_NAME_LEN];
    pid_t leader;
    int n_ip_addresses;
    char addresses[16][64];
    time_t started_at;
} MachineInfo;

/* ---------- Main structures ---------- */

typedef struct {
    /* System state (copy from kernel/systemd) */
    int is_systemd_system_running;      /* is-system-running result */
    char system_running_state[64];

    /* Systemctl CLI options */
    int systemctl_verbose;
    int systemctl_quiet;
    int systemctl_ask_password;
    int systemctl_no_wall;
    int systemctl_no_reload;
    int systemctl_no_block;
    int systemctl_no_pager;
    int systemctl_no_legend;
    int systemctl_all;
    int systemctl_failed;
    int systemctl_reversed;
    int systemctl_recursive;
    int systemctl_isolate;
    int systemctl_now;
    int systemctl_root;
    int systemctl_runtime;
    int systemctl_full;
    int systemctl_plain;
    int systemctl_user;
    int systemctl_global;
    int systemctl_preset_mode;
    int systemctl_no_checks;
    char systemctl_host[256];
    int  systemctl_wall_message_mode;
    int  systemctl_type_filter_count;
    UnitType systemctl_type_filter[32];
    int  systemctl_state_filter_count;
    UnitActiveState systemctl_state_filter[32];

    int systemctl_show_properties_mode;
    char systemctl_show_properties[64][256];
    int  systemctl_show_properties_count;
    int  systemctl_value_only;

    /* Loaded units + jobs tables */
    Unit units[SYSCTL_MAX_UNITS];
    int unit_count;
    Job  jobs[SYSCTL_MAX_JOB_ID];
    int  job_count;
    int  job_id_counter;

    /* Journal state */
    char journal_path[JOURNAL_MAX_PATH];
    char journal_machine_id_dir[JOURNAL_MAX_PATH];
    int journal_writable;
    JournalEntry *journal_entries;
    int journal_cap;
    int journal_len;
    int journal_fd_system;
    int journal_fd_user;

    /* loginctl state */
    SessionInfo sessions[LOGINCTL_MAX_SESSIONS];
    int session_count;
    UserInfo users[LOGINCTL_MAX_USERS];
    int user_count;
    SeatInfo seats[LOGINCTL_MAX_SEATS];
    int seat_count;
    MachineInfo machines[LOGINCTL_MAX_MACHINES];
    int machine_count;

} SystemdToolsState;

/* ---------- systemctl commands ---------- */
int sysctl_parse_arguments(SystemdToolsState *s, int argc, char **argv);

/* Unit */
int sysctl_load_unit_files(SystemdToolsState *s);
int sysctl_save_unit_file(SystemdToolsState *s, const Unit *u);
int sysctl_add_unit(SystemdToolsState *s, const Unit *u, int *out_index);
Unit *sysctl_find_unit(SystemdToolsState *s, const char *name);
int sysctl_unit_get_type_from_name(const char *name, UnitType *out);

/* Job control */
int sysctl_start_unit(SystemdToolsState *s, const char *name, UnitStartMode mode);
int sysctl_stop_unit(SystemdToolsState *s, const char *name);
int sysctl_restart_unit(SystemdToolsState *s, const char *name, int try);
int sysctl_reload_unit(SystemdToolsState *s, const char *name);
int sysctl_isolate_unit(SystemdToolsState *s, const char *name);
int sysctl_status_unit(SystemdToolsState *s, const char *name);
int sysctl_reset_failed_unit(SystemdToolsState *s, const char *name_or_all);
int sysctl_kill_unit(SystemdToolsState *s, const char *name, int sig, const char *who);
int sysctl_show_unit(SystemdToolsState *s, const char *name, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE],
                     int *out_lines);

/* File system linkers (enable/disable/preset) */
int sysctl_enable_unit(SystemdToolsState *s, const char *name, int now);
int sysctl_disable_unit(SystemdToolsState *s, const char *name, int now);
int sysctl_mask_unit(SystemdToolsState *s, const char *name, int runtime);
int sysctl_unmask_unit(SystemdToolsState *s, const char *name, int runtime);
int sysctl_reexecute(SystemdToolsState *s);
int sysctl_daemon_reload(SystemdToolsState *s);
int sysctl_link_unit(SystemdToolsState *s, const char *path, int runtime);

/* Lifecycle commands */
int sysctl_poweroff(SystemdToolsState *s);
int sysctl_reboot(SystemdToolsState *s);
int sysctl_halt(SystemdToolsState *s);
int sysctl_suspend(SystemdToolsState *s);
int sysctl_hibernate(SystemdToolsState *s);
int sysctl_hybrid_sleep(SystemdToolsState *s);
int sysctl_kexec(SystemdToolsState *s);
int sysctl_switch_root(SystemdToolsState *s, const char *newroot, const char *init);
int sysctl_is_system_running(SystemdToolsState *s);

/* Targets + preset */
int sysctl_set_default(SystemdToolsState *s, const char *target_name);
int sysctl_get_default(SystemdToolsState *s, char out[SYSCTL_MAX_NAME_LEN]);
int sysctl_preset_all(SystemdToolsState *s);
int sysctl_preset(SystemdToolsState *s, const char *name);

/* List commands */
int sysctl_list_units(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_unit_files(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_machines(SystemdToolsState *s, char out[LOGINCTL_MAX_MACHINES][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_jobs(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_sockets(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_timers(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_mounts(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_automounts(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_swaps(SystemdToolsState *s, char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines);
int sysctl_list_dependencies(SystemdToolsState *s, const char *unit_name,
                              char out[SYSCTL_MAX_DEPS * 4][SYSCTL_MAX_LINE], int *out_lines, int reverse);

/* ---------- journalctl commands ---------- */
int journalctl_parse_arguments(SystemdToolsState *s, int argc, char **argv, JournalctlOptions *opts);
int journalctl_load(SystemdToolsState *s);
int journalctl_write(SystemdToolsState *s, const JournalEntry *entry);
int journalctl_iter_filtered(SystemdToolsState *s, const JournalctlOptions *opts,
                             int (*emit_line)(void *ctx, const JournalEntry *e, int idx),
                             void *ctx);
int journalctl_dump(SystemdToolsState *s, const JournalctlOptions *opts);
int journalctl_get_usage(SystemdToolsState *s, uint64_t *out_used_bytes,
                         uint64_t *out_archived_bytes);
int journalctl_vacuum(SystemdToolsState *s, const JournalctlOptions *opts);
int journalctl_flush(SystemdToolsState *s);
int journalctl_rotate(SystemdToolsState *s);

/* ---------- loginctl commands ---------- */
int loginctl_parse_arguments(SystemdToolsState *s, int argc, char **argv);
int loginctl_list_sessions(SystemdToolsState *s,
                           char out[LOGINCTL_MAX_SESSIONS][SYSCTL_MAX_LINE], int *out_lines);
int loginctl_list_users(SystemdToolsState *s,
                        char out[LOGINCTL_MAX_USERS][SYSCTL_MAX_LINE], int *out_lines);
int loginctl_list_seats(SystemdToolsState *s,
                        char out[LOGINCTL_MAX_SEATS][SYSCTL_MAX_LINE], int *out_lines);
int loginctl_list_machines(SystemdToolsState *s,
                           char out[LOGINCTL_MAX_MACHINES][SYSCTL_MAX_LINE], int *out_lines);
int loginctl_session_status(SystemdToolsState *s, const char *id);
int loginctl_user_status(SystemdToolsState *s, const char *user_or_uid);
int loginctl_seat_status(SystemdToolsState *s, const char *id);
int loginctl_machine_status(SystemdToolsState *s, const char *name);
int loginctl_terminate_session(SystemdToolsState *s, const char *id, int signal);
int loginctl_kill_session(SystemdToolsState *s, const char *id, int sig, const char *who);
int loginctl_lock_session(SystemdToolsState *s, const char *id);
int loginctl_unlock_session(SystemdToolsState *s, const char *id);
int loginctl_activate(SystemdToolsState *s, const char *id);
int loginctl_activate_lingering(SystemdToolsState *s, uid_t uid, int on);
int loginctl_enable_linger(SystemdToolsState *s, const char *user);
int loginctl_disable_linger(SystemdToolsState *s, const char *user);

/* Main tool entry helpers */
void systemd_tools_init(SystemdToolsState *s);
void systemd_tools_cleanup(SystemdToolsState *s);
int systemctl_main_internal(SystemdToolsState *s, int argc, char **argv);
int journalctl_main_internal(SystemdToolsState *s, int argc, char **argv);
int loginctl_main_internal(SystemdToolsState *s, int argc, char **argv);

void systemctl_print_help(void);
void systemctl_print_version(void);
void journalctl_print_help(void);
void journalctl_print_version(void);
void loginctl_print_help(void);
void loginctl_print_version(void);

#endif
