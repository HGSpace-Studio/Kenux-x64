/*
 * Kenux OS - systemd user-space tools (systemctl, journalctl, loginctl)
 * Skeleton implementation
 *
 * This file complements the kernel/systemd C sources and the systemd_tools.h header.
 * Behaviour is intentionally minimal: the in-memory state is exercised and
 * the public API returns success/failure in a way that mirrors upstream
 * systemctl/journalctl/loginctl, but no real PID 1 IPC or filesystem layout
 * is required for the build to succeed.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* exposes localtime_r, symlink, readlink, etc. */
#endif

#include "systemd_tools.h"

#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <direct.h>   /* _mkdir */
#include <io.h>        /* _mkdir, _access */
#endif

/* ============================================================ *
 * Windows/MinGW POSIX compatibility shims
 * ============================================================ */
#ifdef _WIN32
/* Windows mkdir takes one argument; remap POSIX 2-arg form */
#define mkdir(path, mode) _mkdir(path)

/* kill() not declared in MinGW signal.h; provide a stub */
static inline int kill(int pid, int sig) {
    (void)pid; (void)sig;
    errno = EPERM;
    return -1;
}

/* symlink() not available on MinGW; stub */
static inline int symlink(const char *target, const char *linkpath) {
    (void)target; (void)linkpath;
    errno = ENOSYS;
    return -1;
}

/* readlink() not available on MinGW; stub */
static inline ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    (void)path; (void)buf; (void)bufsiz;
    errno = ENOSYS;
    return -1;
}

/* localtime_r compatibility using localtime_s (note arg order swap) */
static inline struct tm *localtime_r(const time_t *timep, struct tm *result) {
    if (localtime_s(result, timep) == 0) return result;
    return NULL;
}
#endif /* _WIN32 */

/* ============================================================ *
 * Static helpers: enum -> string conversions
 * ============================================================ */

static const char *unit_type_name(UnitType t)
{
    switch (t) {
    case UNIT_SERVICE:   return "service";
    case UNIT_SOCKET:    return "socket";
    case UNIT_TARGET:    return "target";
    case UNIT_DEVICE:    return "device";
    case UNIT_MOUNT:     return "mount";
    case UNIT_AUTOMOUNT: return "automount";
    case UNIT_SWAP:      return "swap";
    case UNIT_TIMER:     return "timer";
    case UNIT_PATH:      return "path";
    case UNIT_SLICE:     return "slice";
    case UNIT_SCOPE:     return "scope";
    case UNIT_BUSNAME:   return "busname";
    case UNIT_NETWORK:   return "network";
    case UNIT_NETDEV:    return "netdev";
    case UNIT_LINK:      return "link";
    case UNIT_TEMPLATE:  return "template";
    default:             return "unknown";
    }
}

static const char *unit_load_state_name(UnitLoadState s)
{
    switch (s) {
    case UNIT_LOAD_STUB:      return "stub";
    case UNIT_LOAD_LOADED:    return "loaded";
    case UNIT_LOAD_NOT_FOUND: return "not-found";
    case UNIT_LOAD_ERROR:     return "error";
    case UNIT_LOAD_MERGED:    return "merged";
    case UNIT_LOAD_MASKED:    return "masked";
    default:                  return "unknown";
    }
}

static const char *unit_active_state_name(UnitActiveState s)
{
    switch (s) {
    case UNIT_ACTIVE_UNKNOWN:     return "unknown";
    case UNIT_ACTIVE_ACTIVE:      return "active";
    case UNIT_ACTIVE_RELOADING:   return "reloading";
    case UNIT_ACTIVE_INACTIVE:    return "inactive";
    case UNIT_ACTIVE_FAILED:      return "failed";
    case UNIT_ACTIVE_ACTIVATING:  return "activating";
    case UNIT_ACTIVE_DEACTIVATING:return "deactivating";
    case UNIT_ACTIVE_MAINTENANCE: return "maintenance";
    default:                       return "unknown";
    }
}

static const char *unit_sub_state_name(UnitSubState s)
{
    switch (s) {
    case UNIT_SUB_RUNNING:            return "running";
    case UNIT_SUB_DEAD:               return "dead";
    case UNIT_SUB_START_PRE:          return "start-pre";
    case UNIT_SUB_START:              return "start";
    case UNIT_SUB_START_POST:         return "start-post";
    case UNIT_SUB_EXITED:             return "exited";
    case UNIT_SUB_RELOAD:             return "reload";
    case UNIT_SUB_STOP:               return "stop";
    case UNIT_SUB_STOP_WATCHDOG:      return "stop-watchdog";
    case UNIT_SUB_STOP_SIGTERM:       return "stop-sigterm";
    case UNIT_SUB_STOP_SIGKILL:       return "stop-sigkill";
    case UNIT_SUB_FINAL_SIGTERM:     return "final-sigterm";
    case UNIT_SUB_FINAL_SIGKILL:     return "final-sigkill";
    case UNIT_SUB_FAILED:            return "failed";
    case UNIT_SUB_AUTO_RESTART:      return "auto-restart";
    case UNIT_SUB_MOUNTED:           return "mounted";
    case UNIT_SUB_MOUNTING:          return "mounting";
    case UNIT_SUB_UNMOUNTING:        return "unmounting";
    case UNIT_SUB_MOUNTING_DONE:     return "mounting-done";
    case UNIT_SUB_REMOUNTING:        return "remounting";
    case UNIT_SUB_DEAD_TARGET:      return "dead";
    case UNIT_SUB_ACTIVE_TARGET:     return "active";
    case UNIT_SUB_LISTENING:         return "listening";
    case UNIT_SUB_WAITING:           return "waiting";
    case UNIT_SUB_ELAPSED:           return "elapsed";
    case UNIT_SUB_ACTIVE_SWAP:       return "active";
    case UNIT_SUB_WAITING_PATH:      return "waiting";
    case UNIT_SUB_WAITING_AUTOMOUNT: return "waiting";
    case UNIT_SUB_PLUGGED:           return "plugged";
    case UNIT_SUB_ABANDONED:         return "abandoned";
    case UNIT_SUB_ACTIVE_SLICE:      return "active";
    default:                          return "unknown";
    }
}

static const char *job_type_name(JobType t)
{
    switch (t) {
    case JOB_START:                 return "start";
    case JOB_STOP:                  return "stop";
    case JOB_RELOAD:                return "reload";
    case JOB_RESTART:               return "restart";
    case JOB_TRY_RESTART:           return "try-restart";
    case JOB_RELOAD_OR_RESTART:     return "reload-or-restart";
    case JOB_TRY_RELOAD_OR_RESTART: return "try-reload-or-restart";
    case JOB_ISOLATE:               return "isolate";
    case JOB_KILL:                  return "kill";
    default:                         return "unknown";
    }
}

static const char *job_state_name(JobState s)
{
    switch (s) {
    case JOB_WAITING: return "waiting";
    case JOB_RUNNING: return "running";
    case JOB_DONE:    return "done";
    case JOB_FAILED:  return "failed";
    default:          return "unknown";
    }
}

static const char *kill_mode_name(KillMode k)
{
    switch (k) {
    case KILL_CONTROL_GROUP: return "control-group";
    case KILL_MIXED:         return "mixed";
    case KILL_NONE:          return "none";
    case KILL_PROCESS:       return "process";
    default:                 return "control-group";
    }
}

static const char *restart_mode_name(RestartMode r)
{
    switch (r) {
    case RESTART_NO:           return "no";
    case RESTART_ON_SUCCESS:   return "on-success";
    case RESTART_ON_FAILURE:   return "on-failure";
    case RESTART_ON_ABNORMAL:  return "on-abnormal";
    case RESTART_ON_WATCHDOG:  return "on-watchdog";
    case RESTART_ON_ABORT:     return "on-abort";
    case RESTART_ALWAYS:       return "always";
    default:                    return "no";
    }
}

static const char *service_type_name(ServiceType t)
{
    switch (t) {
    case SERVICE_SIMPLE:  return "simple";
    case SERVICE_FORKING: return "forking";
    case SERVICE_ONESHOT: return "oneshot";
    case SERVICE_DBUS:    return "dbus";
    case SERVICE_NOTIFY:  return "notify";
    case SERVICE_IDLE:    return "idle";
    case SERVICE_EXEC:    return "exec";
    default:              return "simple";
    }
}

static const char *session_type_name(SessionType t)
{
    switch (t) {
    case SESSION_LOGIN:    return "login";
    case SESSION_WAYLAND: return "wayland";
    case SESSION_X11:     return "x11";
    case SESSION_TTY:     return "tty";
    case SESSION_SSH:     return "ssh";
    case SESSION_SERIAL:  return "serial";
    case SESSION_MIRROR:  return "mirror";
    case SESSION_WEB:     return "web";
    default:              return "unspecified";
    }
}

static const char *session_state_name(SessionState s)
{
    switch (s) {
    case SESSION_STATE_OPENING:  return "opening";
    case SESSION_STATE_ACTIVE:   return "active";
    case SESSION_STATE_CLOSING:  return "closing";
    case SESSION_STATE_ONLINE:   return "online";
    case SESSION_STATE_IDLE:     return "idle";
    case SESSION_STATE_LINGERING:return "lingering";
    default:                     return "unknown";
    }
}

static int starts_with(const char *s, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static int unit_type_from_string(const char *s, UnitType *out)
{
    if (!s || !out) return -1;
    if      (strcmp(s, "service")   == 0) *out = UNIT_SERVICE;
    else if (strcmp(s, "socket")    == 0) *out = UNIT_SOCKET;
    else if (strcmp(s, "target")    == 0) *out = UNIT_TARGET;
    else if (strcmp(s, "device")    == 0) *out = UNIT_DEVICE;
    else if (strcmp(s, "mount")     == 0) *out = UNIT_MOUNT;
    else if (strcmp(s, "automount") == 0) *out = UNIT_AUTOMOUNT;
    else if (strcmp(s, "swap")      == 0) *out = UNIT_SWAP;
    else if (strcmp(s, "timer")     == 0) *out = UNIT_TIMER;
    else if (strcmp(s, "path")      == 0) *out = UNIT_PATH;
    else if (strcmp(s, "slice")     == 0) *out = UNIT_SLICE;
    else if (strcmp(s, "scope")     == 0) *out = UNIT_SCOPE;
    else if (strcmp(s, "busname")   == 0) *out = UNIT_BUSNAME;
    else if (strcmp(s, "network")   == 0) *out = UNIT_NETWORK;
    else if (strcmp(s, "netdev")    == 0) *out = UNIT_NETDEV;
    else if (strcmp(s, "link")      == 0) *out = UNIT_LINK;
    else return -1;
    return 0;
}

/* Push a job into the state's job table; returns job id or -1 */
static int sysctl_enqueue_job(SystemdToolsState *s, JobType type,
                              const char *unit_name, UnitType unit_type)
{
    if (!s || !unit_name) return -1;
    if (s->job_count >= SYSCTL_MAX_JOB_ID) return -1;
    int id = s->job_id_counter++;
    if (id <= 0) id = 1;
    Job *j = &s->jobs[s->job_count++];
    memset(j, 0, sizeof(*j));
    j->id = id;
    j->type = type;
    j->unit_type = unit_type;
    strncpy(j->unit_name, unit_name, sizeof(j->unit_name) - 1);
    j->state = JOB_WAITING;
    j->result = 0;
    j->created_at = time(NULL);
    return id;
}

static int sysctl_finish_job(SystemdToolsState *s, int job_id, JobState state, int result)
{
    if (!s) return -1;
    for (int i = 0; i < s->job_count; i++) {
        if (s->jobs[i].id == job_id) {
            s->jobs[i].state = state;
            s->jobs[i].result = result;
            s->jobs[i].elapsed_us = (uint64_t)(time(NULL) - s->jobs[i].created_at) * 1000000ULL;
            return 0;
        }
    }
    return -1;
}

/* Locate-or-create helper used by start/stop/restart/reload/isolate */
static Unit *sysctl_get_or_load_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return NULL;
    Unit *u = sysctl_find_unit(s, name);
    if (u) return u;
    if (s->unit_count >= SYSCTL_MAX_UNITS) return NULL;
    u = &s->units[s->unit_count++];
    memset(u, 0, sizeof(*u));
    strncpy(u->name, name, sizeof(u->name) - 1);
    strncpy(u->id, name, sizeof(u->id) - 1);
    sysctl_unit_get_type_from_name(name, &u->type);
    u->load_state = UNIT_LOAD_LOADED;
    u->active_state = UNIT_ACTIVE_INACTIVE;
    u->sub_state = UNIT_SUB_DEAD;
    u->load_time = time(NULL);
    return u;
}

/* ============================================================ *
 * Initialisation / cleanup
 * ============================================================ */

void systemd_tools_init(SystemdToolsState *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    strcpy(s->system_running_state, "initializing");
    s->is_systemd_system_running = 0;
    s->journal_cap = 1024;
    s->journal_entries = calloc((size_t)s->journal_cap, sizeof(JournalEntry));
    s->journal_fd_system = -1;
    s->journal_fd_user = -1;
    s->job_id_counter = 1;
    snprintf(s->journal_path, sizeof(s->journal_path), "/var/log/journal");
    snprintf(s->journal_machine_id_dir, sizeof(s->journal_machine_id_dir), "/var/log/journal");
}

void systemd_tools_cleanup(SystemdToolsState *s)
{
    if (!s) return;
    if (s->journal_entries) {
        free(s->journal_entries);
        s->journal_entries = NULL;
    }
    s->journal_len = 0;
    s->journal_cap = 0;
    if (s->journal_fd_system >= 0) { close(s->journal_fd_system); s->journal_fd_system = -1; }
    if (s->journal_fd_user >= 0)  { close(s->journal_fd_user);  s->journal_fd_user = -1; }
}

/* ============================================================ *
 * systemctl argument parsing
 * ============================================================ */

int sysctl_parse_arguments(SystemdToolsState *s, int argc, char **argv)
{
    if (!s) return -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;

        if      (strcmp(a, "--user") == 0)             s->systemctl_user = 1;
        else if (strcmp(a, "--system") == 0)          s->systemctl_user = 0;
        else if (strcmp(a, "--global") == 0)          s->systemctl_global = 1;
        else if (strcmp(a, "--no-block") == 0)        s->systemctl_no_block = 1;
        else if (strcmp(a, "--no-pager") == 0)        s->systemctl_no_pager = 1;
        else if (strcmp(a, "--no-legend") == 0)       s->systemctl_no_legend = 1;
        else if (strcmp(a, "--no-reload") == 0)       s->systemctl_no_reload = 1;
        else if (strcmp(a, "--no-wall") == 0)         s->systemctl_no_wall = 1;
        else if (strcmp(a, "--no-ask-password") == 0) s->systemctl_ask_password = 0;
        else if (strcmp(a, "--ask-password") == 0)   s->systemctl_ask_password = 1;
        else if (strcmp(a, "--all") == 0 || strcmp(a, "-a") == 0)  s->systemctl_all = 1;
        else if (strcmp(a, "--failed") == 0)         s->systemctl_failed = 1;
        else if (strcmp(a, "--reverse") == 0)        s->systemctl_reversed = 1;
        else if (strcmp(a, "--recursive") == 0)      s->systemctl_recursive = 1;
        else if (strcmp(a, "--runtime") == 0)        s->systemctl_runtime = 1;
        else if (strcmp(a, "--full") == 0)           s->systemctl_full = 1;
        else if (strcmp(a, "--plain") == 0)          s->systemctl_plain = 1;
        else if (strcmp(a, "--now") == 0)           s->systemctl_now = 1;
        else if (strcmp(a, "--quiet") == 0 || strcmp(a, "-q") == 0) s->systemctl_quiet = 1;
        else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) s->systemctl_verbose = 1;
        else if (strcmp(a, "--isolate") == 0)       s->systemctl_isolate = 1;
        else if (strcmp(a, "--show-types") == 0)    s->systemctl_full = 1;
        else if (strcmp(a, "--value") == 0)         s->systemctl_value_only = 1;
        else if (starts_with(a, "--type=")) {
            UnitType t;
            if (unit_type_from_string(a + 7, &t) == 0 &&
                s->systemctl_type_filter_count < 32)
                s->systemctl_type_filter[s->systemctl_type_filter_count++] = t;
        }
        else if (starts_with(a, "--state=")) {
            const char *v = a + 8;
            if (s->systemctl_state_filter_count < 32) {
                if (strcmp(v, "active") == 0)
                    s->systemctl_state_filter[s->systemctl_state_filter_count++] = UNIT_ACTIVE_ACTIVE;
                else if (strcmp(v, "inactive") == 0)
                    s->systemctl_state_filter[s->systemctl_state_filter_count++] = UNIT_ACTIVE_INACTIVE;
                else if (strcmp(v, "failed") == 0)
                    s->systemctl_state_filter[s->systemctl_state_filter_count++] = UNIT_ACTIVE_FAILED;
            }
        }
        else if (starts_with(a, "--root=")) {
            /* --root is parsed for completeness; state has no slot for it. */
        }
        else if (starts_with(a, "--host=")) {
            strncpy(s->systemctl_host, a + 7, sizeof(s->systemctl_host) - 1);
        }
        else if (strcmp(a, "-H") == 0 && i + 1 < argc) {
            strncpy(s->systemctl_host, argv[++i], sizeof(s->systemctl_host) - 1);
        }
        else if (strcmp(a, "-M") == 0 && i + 1 < argc) {
            strncpy(s->systemctl_host, argv[++i], sizeof(s->systemctl_host) - 1);
        }
        else if (starts_with(a, "--property=") || strcmp(a, "-p") == 0) {
            const char *v = NULL;
            if (starts_with(a, "--property=")) v = a + 11;
            else if (i + 1 < argc) v = argv[++i];
            if (v && s->systemctl_show_properties_count < 64)
                strncpy(s->systemctl_show_properties[s->systemctl_show_properties_count++],
                       v, 255);
        }
        else if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            systemctl_print_version();
            return 1;
        }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            systemctl_print_help();
            return 1;
        }
        /* else: command or unit name - left for systemctl_main_internal */
    }
    return 0;
}

/* ============================================================ *
 * Unit file operations
 * ============================================================ */

int sysctl_unit_get_type_from_name(const char *name, UnitType *out)
{
    if (!name || !out) return -1;
    const char *dot = strrchr(name, '.');
    if (!dot) { *out = UNIT_SERVICE; return -1; }
    if      (strcmp(dot, ".service")   == 0) *out = UNIT_SERVICE;
    else if (strcmp(dot, ".socket")    == 0) *out = UNIT_SOCKET;
    else if (strcmp(dot, ".target")    == 0) *out = UNIT_TARGET;
    else if (strcmp(dot, ".device")    == 0) *out = UNIT_DEVICE;
    else if (strcmp(dot, ".mount")     == 0) *out = UNIT_MOUNT;
    else if (strcmp(dot, ".automount") == 0) *out = UNIT_AUTOMOUNT;
    else if (strcmp(dot, ".swap")      == 0) *out = UNIT_SWAP;
    else if (strcmp(dot, ".timer")     == 0) *out = UNIT_TIMER;
    else if (strcmp(dot, ".path")      == 0) *out = UNIT_PATH;
    else if (strcmp(dot, ".slice")     == 0) *out = UNIT_SLICE;
    else if (strcmp(dot, ".scope")     == 0) *out = UNIT_SCOPE;
    else if (strcmp(dot, ".busname")   == 0) *out = UNIT_BUSNAME;
    else if (strcmp(dot, ".network")   == 0) *out = UNIT_NETWORK;
    else if (strcmp(dot, ".netdev")    == 0) *out = UNIT_NETDEV;
    else if (strcmp(dot, ".link")      == 0) *out = UNIT_LINK;
    else { *out = UNIT_SERVICE; return -1; }
    return 0;
}

Unit *sysctl_find_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return NULL;
    for (int i = 0; i < s->unit_count; i++) {
        if (strcmp(s->units[i].name, name) == 0 ||
            strcmp(s->units[i].id, name) == 0)
            return &s->units[i];
    }
    /* Check aliases */
    for (int i = 0; i < s->unit_count; i++) {
        Unit *u = &s->units[i];
        for (int k = 0; k < u->common.alias_count; k++) {
            if (strcmp(u->common.alias[k], name) == 0)
                return u;
        }
    }
    return NULL;
}

int sysctl_add_unit(SystemdToolsState *s, const Unit *u, int *out_index)
{
    if (!s || !u) return -1;
    if (s->unit_count >= SYSCTL_MAX_UNITS) return -1;
    if (sysctl_find_unit(s, u->name)) return -1;  /* duplicate */
    int idx = s->unit_count++;
    s->units[idx] = *u;
    if (out_index) *out_index = idx;
    return 0;
}

int sysctl_load_unit_files(SystemdToolsState *s)
{
    if (!s) return -1;
    static const char *dirs[] = {
        "/etc/systemd/system",
        "/run/systemd/system",
        "/usr/lib/systemd/system",
        "/usr/local/lib/systemd/system",
        NULL
    };
    for (int d = 0; dirs[d]; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (de->d_name[0] == '.') continue;
            UnitType t;
            if (sysctl_unit_get_type_from_name(de->d_name, &t) != 0) continue;
            if (s->unit_count >= SYSCTL_MAX_UNITS) { closedir(dir); return 0; }
            Unit *u = &s->units[s->unit_count];
            memset(u, 0, sizeof(*u));
            strncpy(u->name, de->d_name, sizeof(u->name) - 1);
            strncpy(u->id, de->d_name, sizeof(u->id) - 1);
            u->type = t;
            u->load_state = UNIT_LOAD_LOADED;
            u->active_state = UNIT_ACTIVE_INACTIVE;
            u->sub_state = UNIT_SUB_DEAD;
            snprintf(u->fragment_path, sizeof(u->fragment_path), "%s/%s", dirs[d], de->d_name);
            u->load_time = time(NULL);
            s->unit_count++;
        }
        closedir(dir);
    }
    return 0;
}

int sysctl_save_unit_file(SystemdToolsState *s, const Unit *u)
{
    if (!s || !u) return -1;
    char path[SYSCTL_MAX_CGROUP_LEN + SYSCTL_MAX_NAME_LEN + 16];
    const char *dir = s->systemctl_user ? "/etc/systemd/user" : "/etc/systemd/system";
    snprintf(path, sizeof(path), "%s/%s", dir, u->name);
    FILE *f = fopen(path, "w");
    if (!f) {
        if (s->systemctl_verbose)
            fprintf(stderr, "systemctl: cannot write %s: %s\n", path, strerror(errno));
        return -1;
    }
    fprintf(f, "# Generated by KenuxK systemd_tools\n");
    fprintf(f, "[Unit]\n");
    if (u->common.description[0])
        fprintf(f, "Description=%s\n", u->common.description);
    if (u->common.documentation[0])
        fprintf(f, "Documentation=%s\n", u->common.documentation);
    for (int i = 0; i < u->common.requires_count; i++)
        fprintf(f, "Requires=%s\n", u->common.requires[i]);
    for (int i = 0; i < u->common.wants_count; i++)
        fprintf(f, "Wants=%s\n", u->common.wants[i]);
    for (int i = 0; i < u->common.after_count; i++)
        fprintf(f, "After=%s\n", u->common.after[i]);
    for (int i = 0; i < u->common.before_count; i++)
        fprintf(f, "Before=%s\n", u->common.before[i]);
    fprintf(f, "\n[Install]\n");
    for (int i = 0; i < u->common.wanted_by_count; i++)
        fprintf(f, "WantedBy=%s\n", u->common.wanted_by[i]);
    for (int i = 0; i < u->common.required_by_count; i++)
        fprintf(f, "RequiredBy=%s\n", u->common.required_by[i]);
    for (int i = 0; i < u->common.alias_count; i++)
        fprintf(f, "Alias=%s\n", u->common.alias[i]);
    fclose(f);
    return 0;
}

/* ============================================================ *
 * Job control: start / stop / restart / reload / isolate / status
 * ============================================================ */

int sysctl_start_unit(SystemdToolsState *s, const char *name, UnitStartMode mode)
{
    if (!s || !name) return -1;
    (void)mode;
    Unit *u = sysctl_get_or_load_unit(s, name);
    if (!u) return -1;
    int jid = sysctl_enqueue_job(s, JOB_START, u->name, u->type);
    if (jid < 0) return -1;
    u->job_id = jid;
    u->job_type = JOB_START;
    u->job_state = JOB_RUNNING;
    u->active_state = UNIT_ACTIVE_ACTIVATING;
    u->sub_state = UNIT_SUB_START;
    /* Synchronously complete (skeleton semantics) */
    u->active_state = UNIT_ACTIVE_ACTIVE;
    u->sub_state = UNIT_SUB_RUNNING;
    u->active_enter_monotonic = time(NULL);
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_stop_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) return -1;
    int jid = sysctl_enqueue_job(s, JOB_STOP, u->name, u->type);
    if (jid < 0) return -1;
    u->job_id = jid;
    u->job_type = JOB_STOP;
    u->job_state = JOB_RUNNING;
    u->active_state = UNIT_ACTIVE_DEACTIVATING;
    u->sub_state = UNIT_SUB_STOP;
    u->active_state = UNIT_ACTIVE_INACTIVE;
    u->sub_state = UNIT_SUB_DEAD;
    u->inactive_exit_monotonic = time(NULL);
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_restart_unit(SystemdToolsState *s, const char *name, int try)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) {
        if (try) return 0;  /* try-restart: do nothing if not loaded */
        return -1;
    }
    int jid = sysctl_enqueue_job(s, try ? JOB_TRY_RESTART : JOB_RESTART, u->name, u->type);
    if (jid < 0) return -1;
    u->job_id = jid;
    u->job_type = JOB_RESTART;
    u->job_state = JOB_RUNNING;
    u->active_state = UNIT_ACTIVE_ACTIVATING;
    u->sub_state = UNIT_SUB_AUTO_RESTART;
    u->active_state = UNIT_ACTIVE_ACTIVE;
    u->sub_state = UNIT_SUB_RUNNING;
    u->active_enter_monotonic = time(NULL);
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_reload_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) return -1;
    int jid = sysctl_enqueue_job(s, JOB_RELOAD, u->name, u->type);
    if (jid < 0) return -1;
    u->job_id = jid;
    u->job_type = JOB_RELOAD;
    u->job_state = JOB_RUNNING;
    u->active_state = UNIT_ACTIVE_RELOADING;
    u->sub_state = UNIT_SUB_RELOAD;
    u->active_state = UNIT_ACTIVE_ACTIVE;
    u->sub_state = UNIT_SUB_RUNNING;
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_isolate_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_get_or_load_unit(s, name);
    if (!u) return -1;
    int jid = sysctl_enqueue_job(s, JOB_ISOLATE, u->name, u->type);
    if (jid < 0) return -1;
    /* Isolate: stop everything not in the dependency tree of the target.
     * Skeleton: just start the target. */
    u->active_state = UNIT_ACTIVE_ACTIVE;
    u->sub_state = UNIT_SUB_ACTIVE_TARGET;
    u->active_enter_monotonic = time(NULL);
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_status_unit(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) {
        printf("Unit %s could not be found.\n", name);
        return -1;
    }
    printf("● %s - %s\n", u->id,
           u->common.description[0] ? u->common.description : u->id);
    printf("     Loaded: loaded (%s; %s)\n",
           u->fragment_path[0] ? u->fragment_path : "(no file)",
           unit_load_state_name(u->load_state));
    printf("     Active: %s (%s) since %ld\n",
           unit_active_state_name(u->active_state),
           unit_sub_state_name(u->sub_state),
           (long)u->active_enter_monotonic);
    if (u->type == UNIT_SERVICE) {
        ServiceData *svc = &u->data.service;
        if (svc->exec_start_count > 0)
            printf("     ExecStart: %s\n", svc->exec_start[0]);
        printf("   Main PID: %d (code=exited, status=0)\n", (int)svc->main_pid);
        if (svc->exec_start_count > 0 && svc->restart != RESTART_NO)
            printf("     Restart: %s\n", restart_mode_name(svc->restart));
    }
    if (u->common.after_count > 0) {
        printf("     After: ");
        for (int i = 0; i < u->common.after_count; i++)
            printf("%s%s", u->common.after[i], i + 1 < u->common.after_count ? " " : "");
        printf("\n");
    }
    return 0;
}

int sysctl_reset_failed_unit(SystemdToolsState *s, const char *name_or_all)
{
    if (!s) return -1;
    if (name_or_all && strcmp(name_or_all, "*") == 0) {
        for (int i = 0; i < s->unit_count; i++) {
            if (s->units[i].active_state == UNIT_ACTIVE_FAILED) {
                s->units[i].active_state = UNIT_ACTIVE_INACTIVE;
                s->units[i].sub_state = UNIT_SUB_DEAD;
            }
        }
        return 0;
    }
    Unit *u = sysctl_find_unit(s, name_or_all);
    if (!u) return -1;
    if (u->active_state == UNIT_ACTIVE_FAILED) {
        u->active_state = UNIT_ACTIVE_INACTIVE;
        u->sub_state = UNIT_SUB_DEAD;
    }
    return 0;
}

int sysctl_kill_unit(SystemdToolsState *s, const char *name, int sig, const char *who)
{
    if (!s || !name) return -1;
    (void)who;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) return -1;
    if (sig <= 0) sig = SIGTERM;
    int jid = sysctl_enqueue_job(s, JOB_KILL, u->name, u->type);
    if (jid < 0) return -1;
    /* Skeleton: just record the signal; would deliver to control group */
    if (u->type == UNIT_SERVICE && u->data.service.main_pid > 0)
        kill(u->data.service.main_pid, sig);
    sysctl_finish_job(s, jid, JOB_DONE, 0);
    return 0;
}

int sysctl_show_unit(SystemdToolsState *s, const char *name,
                     char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !name || !out || !out_lines) return -1;
    Unit *u = sysctl_find_unit(s, name);
    int n = 0;
    if (!u) {
        snprintf(out[n++], SYSCTL_MAX_LINE, "Id=%s", name);
        snprintf(out[n++], SYSCTL_MAX_LINE, "LoadState=not-found");
        *out_lines = n;
        return -1;
    }
    snprintf(out[n++], SYSCTL_MAX_LINE, "Id=%s", u->id);
    snprintf(out[n++], SYSCTL_MAX_LINE, "Names=%s", u->name);
    snprintf(out[n++], SYSCTL_MAX_LINE, "Type=%s", unit_type_name(u->type));
    snprintf(out[n++], SYSCTL_MAX_LINE, "LoadState=%s", unit_load_state_name(u->load_state));
    snprintf(out[n++], SYSCTL_MAX_LINE, "ActiveState=%s", unit_active_state_name(u->active_state));
    snprintf(out[n++], SYSCTL_MAX_LINE, "SubState=%s", unit_sub_state_name(u->sub_state));
    snprintf(out[n++], SYSCTL_MAX_LINE, "FragmentPath=%s",
             u->fragment_path[0] ? u->fragment_path : "");
    snprintf(out[n++], SYSCTL_MAX_LINE, "Description=%s",
             u->common.description[0] ? u->common.description : u->id);
    snprintf(out[n++], SYSCTL_MAX_LINE, "JobId=%d", u->job_id);
    if (u->type == UNIT_SERVICE) {
        ServiceData *svc = &u->data.service;
        snprintf(out[n++], SYSCTL_MAX_LINE, "Service.Type=%s", service_type_name(svc->type));
        snprintf(out[n++], SYSCTL_MAX_LINE, "Service.MainPID=%d", (int)svc->main_pid);
        snprintf(out[n++], SYSCTL_MAX_LINE, "Service.KillMode=%s", kill_mode_name(svc->kill_mode));
        snprintf(out[n++], SYSCTL_MAX_LINE, "Service.Restart=%s", restart_mode_name(svc->restart));
    }
    *out_lines = n;
    return 0;
}

/* ============================================================ *
 * File-system linkers: enable / disable / mask / unmask / link
 * ============================================================ */

static const char *unit_dir_for_state(SystemdToolsState *s, int runtime)
{
    if (s->systemctl_user)
        return runtime ? "/run/systemd/user" : "/etc/systemd/user";
    return runtime ? "/run/systemd/system" : "/etc/systemd/system";
}

static int mkdir_p(const char *path)
{
    char buf[JOURNAL_MAX_PATH];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int make_symlink(const char *target, const char *linkpath)
{
    char dir[JOURNAL_MAX_PATH];
    strncpy(dir, linkpath, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (mkdir_p(dir) != 0) return -1;
    }
    unlink(linkpath);
    if (symlink(target, linkpath) != 0) return -1;
    return 0;
}

int sysctl_enable_unit(SystemdToolsState *s, const char *name, int now)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (!u) {
        if (s->systemctl_verbose)
            fprintf(stderr, "systemctl: unit %s not found\n", name);
        return -1;
    }
    const char *base = unit_dir_for_state(s, s->systemctl_runtime);
    for (int i = 0; i < u->common.wanted_by_count; i++) {
        char path[JOURNAL_MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/%s.wants/%s", base,
                 u->common.wanted_by[i], u->name);
        char target[JOURNAL_MAX_PATH];
        snprintf(target, sizeof(target), "%s", u->fragment_path[0] ? u->fragment_path : u->name);
        make_symlink(target, path);
    }
    for (int i = 0; i < u->common.required_by_count; i++) {
        char path[JOURNAL_MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/%s.requires/%s", base,
                 u->common.required_by[i], u->name);
        char target[JOURNAL_MAX_PATH];
        snprintf(target, sizeof(target), "%s", u->fragment_path[0] ? u->fragment_path : u->name);
        make_symlink(target, path);
    }
    if (now) sysctl_start_unit(s, name, UNIT_START_REPLACE);
    return 0;
}

int sysctl_disable_unit(SystemdToolsState *s, const char *name, int now)
{
    if (!s || !name) return -1;
    Unit *u = sysctl_find_unit(s, name);
    if (u) {
        const char *base = unit_dir_for_state(s, s->systemctl_runtime);
        for (int i = 0; i < u->common.wanted_by_count; i++) {
            char path[JOURNAL_MAX_PATH * 2];
            snprintf(path, sizeof(path), "%s/%s.wants/%s", base,
                     u->common.wanted_by[i], u->name);
            unlink(path);
        }
        for (int i = 0; i < u->common.required_by_count; i++) {
            char path[JOURNAL_MAX_PATH * 2];
            snprintf(path, sizeof(path), "%s/%s.requires/%s", base,
                     u->common.required_by[i], u->name);
            unlink(path);
        }
    }
    if (now) sysctl_stop_unit(s, name);
    return 0;
}

int sysctl_mask_unit(SystemdToolsState *s, const char *name, int runtime)
{
    if (!s || !name) return -1;
    const char *base = unit_dir_for_state(s, runtime);
    char path[JOURNAL_MAX_PATH * 2];
    snprintf(path, sizeof(path), "%s/%s", base, name);
    make_symlink("/dev/null", path);
    Unit *u = sysctl_find_unit(s, name);
    if (u) u->load_state = UNIT_LOAD_MASKED;
    return 0;
}

int sysctl_unmask_unit(SystemdToolsState *s, const char *name, int runtime)
{
    if (!s || !name) return -1;
    const char *base = unit_dir_for_state(s, runtime);
    char path[JOURNAL_MAX_PATH * 2];
    snprintf(path, sizeof(path), "%s/%s", base, name);
    unlink(path);
    Unit *u = sysctl_find_unit(s, name);
    if (u && u->load_state == UNIT_LOAD_MASKED) {
        u->load_state = UNIT_LOAD_LOADED;
        u->active_state = UNIT_ACTIVE_INACTIVE;
    }
    return 0;
}

int sysctl_link_unit(SystemdToolsState *s, const char *path, int runtime)
{
    if (!s || !path) return -1;
    const char *base = unit_dir_for_state(s, runtime);
    const char *bn = strrchr(path, '/');
    bn = bn ? bn + 1 : path;
    char linkpath[JOURNAL_MAX_PATH * 2];
    snprintf(linkpath, sizeof(linkpath), "%s/%s", base, bn);
    return make_symlink(path, linkpath);
}

/* ============================================================ *
 * Daemon control
 * ============================================================ */

int sysctl_daemon_reload(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton: re-scan unit directories */
    return sysctl_load_unit_files(s);
}

int sysctl_reexecute(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton: same as reload since PID 1 is the kernel here */
    return sysctl_load_unit_files(s);
}

/* ============================================================ *
 * Lifecycle: power management
 * ============================================================ */

int sysctl_poweroff(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: poweroff requested (skeleton - no action)\n");
    return 0;
}

int sysctl_reboot(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: reboot requested (skeleton - no action)\n");
    return 0;
}

int sysctl_halt(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: halt requested (skeleton - no action)\n");
    return 0;
}

int sysctl_suspend(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: suspend requested (skeleton - no action)\n");
    return 0;
}

int sysctl_hibernate(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: hibernate requested (skeleton - no action)\n");
    return 0;
}

int sysctl_hybrid_sleep(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: hybrid-sleep requested (skeleton - no action)\n");
    return 0;
}

int sysctl_kexec(SystemdToolsState *s)
{
    if (!s) return -1;
    fprintf(stderr, "systemctl: kexec requested (skeleton - no action)\n");
    return 0;
}

int sysctl_switch_root(SystemdToolsState *s, const char *newroot, const char *init)
{
    if (!s || !newroot) return -1;
    (void)init;
    fprintf(stderr, "systemctl: switch-root to %s (skeleton - no action)\n", newroot);
    return 0;
}

int sysctl_is_system_running(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton heuristic: running if any service is active */
    int any_active = 0;
    for (int i = 0; i < s->unit_count; i++) {
        if (s->units[i].type == UNIT_SERVICE &&
            s->units[i].active_state == UNIT_ACTIVE_ACTIVE) {
            any_active = 1;
            break;
        }
    }
    strcpy(s->system_running_state, any_active ? "running" : "degraded");
    s->is_systemd_system_running = any_active ? 1 : 0;
    printf("%s\n", s->system_running_state);
    return any_active ? 0 : 1;
}

/* ============================================================ *
 * Targets / preset
 * ============================================================ */

int sysctl_set_default(SystemdToolsState *s, const char *target_name)
{
    if (!s || !target_name) return -1;
    const char *base = unit_dir_for_state(s, 0);
    char path[JOURNAL_MAX_PATH * 2];
    snprintf(path, sizeof(path), "%s/default.target", base);
    char target[JOURNAL_MAX_PATH];
    snprintf(target, sizeof(target), "/usr/lib/systemd/system/%s", target_name);
    make_symlink(target, path);
    return 0;
}

int sysctl_get_default(SystemdToolsState *s, char out[SYSCTL_MAX_NAME_LEN])
{
    if (!s || !out) return -1;
    char linkpath[JOURNAL_MAX_PATH];
    snprintf(linkpath, sizeof(linkpath), "/etc/systemd/system/default.target");
    char buf[JOURNAL_MAX_PATH];
    ssize_t n = readlink(linkpath, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        const char *bn = strrchr(buf, '/');
        bn = bn ? bn + 1 : buf;
        strncpy(out, bn, SYSCTL_MAX_NAME_LEN - 1);
    } else {
        strncpy(out, "graphical.target", SYSCTL_MAX_NAME_LEN - 1);
    }
    out[SYSCTL_MAX_NAME_LEN - 1] = '\0';
    return 0;
}

int sysctl_preset(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    /* Skeleton: always enable. Real preset consults *.preset files. */
    return sysctl_enable_unit(s, name, 0);
}

int sysctl_preset_all(SystemdToolsState *s)
{
    if (!s) return -1;
    for (int i = 0; i < s->unit_count; i++)
        sysctl_preset(s, s->units[i].name);
    return 0;
}

/* ============================================================ *
 * List commands
 * ============================================================ */

int sysctl_list_units(SystemdToolsState *s,
                      char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend) {
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s %-10s %s",
                 "UNIT", "LOAD", "ACTIVE", "SUB", "DESCRIPTION");
    }
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (s->systemctl_type_filter_count > 0) {
            int match = 0;
            for (int k = 0; k < s->systemctl_type_filter_count; k++)
                if (s->systemctl_type_filter[k] == u->type) { match = 1; break; }
            if (!match) continue;
        }
        if (s->systemctl_state_filter_count > 0) {
            int match = 0;
            for (int k = 0; k < s->systemctl_state_filter_count; k++)
                if (s->systemctl_state_filter[k] == u->active_state) { match = 1; break; }
            if (!match) continue;
        }
        if (s->systemctl_failed && u->active_state != UNIT_ACTIVE_FAILED) continue;
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s %-10s %s",
                 u->id,
                 unit_load_state_name(u->load_state),
                 unit_active_state_name(u->active_state),
                 unit_sub_state_name(u->sub_state),
                 u->common.description);
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_unit_files(SystemdToolsState *s,
                           char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-15s %s",
                 "UNIT FILE", "STATE", "VENDOR PRESET");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        const char *st = "static";
        if (u->common.wanted_by_count > 0 || u->common.required_by_count > 0)
            st = "enabled";
        else if (u->load_state == UNIT_LOAD_MASKED)
            st = "masked";
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-15s %s",
                 u->name, st, "enabled");
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_jobs(SystemdToolsState *s,
                     char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-6s %-30s %-15s %-10s",
                 "JOB", "UNIT", "TYPE", "STATE");
    for (int i = 0; i < s->job_count && n < SYSCTL_MAX_UNITS; i++) {
        Job *j = &s->jobs[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-6d %-30s %-15s %-10s",
                 j->id, j->unit_name, job_type_name(j->type), job_state_name(j->state));
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_machines(SystemdToolsState *s,
                         char out[LOGINCTL_MAX_MACHINES][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-20s %-15s %-15s %s",
                 "NAME", "STATE", "FAILED", "CLASS");
    for (int i = 0; i < s->machine_count && n < LOGINCTL_MAX_MACHINES; i++) {
        MachineInfo *m = &s->machines[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-20s %-15s %-15d %s",
                 m->name, m->state[0] ? m->state : "running", 0,
                 m->class[0] ? m->class : "container");
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_sockets(SystemdToolsState *s,
                        char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s %s",
                 "LISTEN", "UNIT", "ACTIVATES", "");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (u->type != UNIT_SOCKET) continue;
        SocketData *sk = &u->data.socket;
        for (int k = 0; k < sk->listen_count && n < SYSCTL_MAX_UNITS; k++) {
            snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s",
                     sk->listen[k], u->id,
                     sk->service[0] ? sk->service : "");
        }
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_timers(SystemdToolsState *s,
                       char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-30s %-15s %-25s %s",
                 "NEXT", "LAST", "UNIT", "ACTIVATES");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (u->type != UNIT_TIMER) continue;
        TimerData *t = &u->data.timer;
        char nbuf[64] = "-", lbuf[64] = "-";
        if (t->next_elapse > 0) {
            struct tm tm; time_t tt = t->next_elapse;
            localtime_r(&tt, &tm);
            strftime(nbuf, sizeof(nbuf), "%a %Y-%m-%d %H:%M:%S", &tm);
        }
        if (t->last_elapse > 0) {
            struct tm tm; time_t tt = t->last_elapse;
            localtime_r(&tt, &tm);
            strftime(lbuf, sizeof(lbuf), "%a %Y-%m-%d %H:%M:%S", &tm);
        }
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-30s %-15s %-25s %s",
                 nbuf, lbuf, u->id, t->unit[0] ? t->unit : "");
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_mounts(SystemdToolsState *s,
                       char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s %s",
                 "WHAT", "WHERE", "TYPE", "OPTIONS");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (u->type != UNIT_MOUNT) continue;
        MountData *m = &u->data.mount;
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %-10s %s",
                 m->what, m->where, m->type, m->options);
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_automounts(SystemdToolsState *s,
                           char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %s",
                 "WHERE", "TYPE", "STATE");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (u->type != UNIT_AUTOMOUNT) continue;
        AutomountData *a = &u->data.automount;
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %s",
                 a->where, "automount", unit_active_state_name(u->active_state));
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_swaps(SystemdToolsState *s,
                      char out[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %s",
                 "WHAT", "TYPE", "STATE");
    for (int i = 0; i < s->unit_count && n < SYSCTL_MAX_UNITS; i++) {
        Unit *u = &s->units[i];
        if (u->type != UNIT_SWAP) continue;
        SwapData *sw = &u->data.swap;
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-50s %-10s %s",
                 sw->what, "partition", unit_active_state_name(u->active_state));
    }
    *out_lines = n;
    return 0;
}

int sysctl_list_dependencies(SystemdToolsState *s, const char *unit_name,
                             char out[SYSCTL_MAX_DEPS * 4][SYSCTL_MAX_LINE],
                             int *out_lines, int reverse)
{
    if (!s || !unit_name || !out || !out_lines) return -1;
    Unit *u = sysctl_find_unit(s, unit_name);
    int n = 0;
    if (!u) {
        snprintf(out[n++], SYSCTL_MAX_LINE, "%s", unit_name);
        *out_lines = n;
        return -1;
    }
    snprintf(out[n++], SYSCTL_MAX_LINE, "%s", u->id);
    if (reverse) {
        for (int i = 0; i < u->common.required_by_count && n < SYSCTL_MAX_DEPS * 4; i++)
            snprintf(out[n++], SYSCTL_MAX_LINE, "└─%s", u->common.required_by[i]);
        for (int i = 0; i < u->common.wanted_by_count && n < SYSCTL_MAX_DEPS * 4; i++)
            snprintf(out[n++], SYSCTL_MAX_LINE, "└─%s", u->common.wanted_by[i]);
    } else {
        for (int i = 0; i < u->common.requires_count && n < SYSCTL_MAX_DEPS * 4; i++)
            snprintf(out[n++], SYSCTL_MAX_LINE, "└─%s", u->common.requires[i]);
        for (int i = 0; i < u->common.wants_count && n < SYSCTL_MAX_DEPS * 4; i++)
            snprintf(out[n++], SYSCTL_MAX_LINE, "└─%s", u->common.wants[i]);
        for (int i = 0; i < u->common.after_count && n < SYSCTL_MAX_DEPS * 4; i++)
            snprintf(out[n++], SYSCTL_MAX_LINE, "└─%s", u->common.after[i]);
    }
    *out_lines = n;
    return 0;
}

/* ============================================================ *
 * journalctl
 * ============================================================ */

int journalctl_parse_arguments(SystemdToolsState *s, int argc, char **argv,
                                JournalctlOptions *opts)
{
    if (!s || !opts) return -1;
    opts->lines = 10;
    opts->priority_min = 0;       /* emerg */
    opts->priority_max = 7;       /* debug */
    opts->this_boot = 0;
    opts->boot_offset = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;
        if      (strcmp(a, "-f") == 0 || strcmp(a, "--follow") == 0) opts->follow = 1;
        else if (strcmp(a, "-r") == 0 || strcmp(a, "--reverse") == 0) opts->reverse = 1;
        else if (strcmp(a, "-a") == 0 || strcmp(a, "--all") == 0) opts->all = 1;
        else if (strcmp(a, "--no-pager") == 0) opts->no_pager = 1;
        else if (strcmp(a, "-q") == 0) opts->quiet_privileged = 1;
        else if (strcmp(a, "-m") == 0 || strcmp(a, "--merge") == 0) opts->merge = 1;
        else if (strcmp(a, "--utc") == 0) opts->utc = 1;
        else if (strcmp(a, "-k") == 0 || strcmp(a, "--dmesg") == 0) opts->dmesg_only = 1;
        else if (strcmp(a, "--no-hostname") == 0) opts->no_pager = opts->no_pager;
        else if (strcmp(a, "--header") == 0) opts->header_show = 1;
        else if (strcmp(a, "--disk-usage") == 0) opts->disk_usage = 1;
        else if (strcmp(a, "--list-boots") == 0) opts->list_boot_ids = 1;
        else if (strcmp(a, "--list-catalog") == 0) opts->list_catalog = 1;
        else if (strcmp(a, "--verify") == 0) opts->verify = 1;
        else if (strcmp(a, "--check") == 0) opts->check = 1;
        else if (strcmp(a, "--show-cursor") == 0) opts->show_cursor = 1;
        else if (strcmp(a, "--rotate") == 0) opts->rotate = 1;
        else if (strcmp(a, "--flush") == 0) opts->flush = 1;
        else if (strcmp(a, "--since") == 0 && i + 1 < argc) {
            strncpy(opts->since, argv[++i], sizeof(opts->since) - 1);
        }
        else if (starts_with(a, "--since=")) {
            strncpy(opts->since, a + 8, sizeof(opts->since) - 1);
        }
        else if (strcmp(a, "--until") == 0 && i + 1 < argc) {
            strncpy(opts->until, argv[++i], sizeof(opts->until) - 1);
        }
        else if (starts_with(a, "--until=")) {
            strncpy(opts->until, a + 8, sizeof(opts->until) - 1);
        }
        else if (strcmp(a, "-n") == 0 || strcmp(a, "--lines") == 0) {
            if (i + 1 < argc) opts->lines = atoi(argv[++i]);
        }
        else if (starts_with(a, "-n")) opts->lines = atoi(a + 2);
        else if (starts_with(a, "--lines=")) opts->lines = atoi(a + 8);
        else if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            if (i + 1 < argc) {
                const char *v = argv[++i];
                if (strcmp(v, "short") == 0) opts->output = JOURNAL_OUTPUT_SHORT;
                else if (strcmp(v, "verbose") == 0) opts->output = JOURNAL_OUTPUT_VERBOSE;
                else if (strcmp(v, "json") == 0) opts->output = JOURNAL_OUTPUT_JSON;
                else if (strcmp(v, "json-pretty") == 0) opts->output = JOURNAL_OUTPUT_JSON_PRETTY;
                else if (strcmp(v, "cat") == 0) opts->output = JOURNAL_OUTPUT_CAT;
                else if (strcmp(v, "export") == 0) opts->output = JOURNAL_OUTPUT_EXPORT;
                else if (strcmp(v, "with-unit") == 0) opts->output = JOURNAL_OUTPUT_WITH_UNIT;
            }
        }
        else if (starts_with(a, "--output=")) {
            const char *v = a + 9;
            if (strcmp(v, "short") == 0) opts->output = JOURNAL_OUTPUT_SHORT;
            else if (strcmp(v, "verbose") == 0) opts->output = JOURNAL_OUTPUT_VERBOSE;
            else if (strcmp(v, "json") == 0) opts->output = JOURNAL_OUTPUT_JSON;
            else if (strcmp(v, "cat") == 0) opts->output = JOURNAL_OUTPUT_CAT;
        }
        else if (strcmp(a, "-p") == 0 || strcmp(a, "--priority") == 0) {
            if (i + 1 < argc) opts->priority_max = atoi(argv[++i]);
        }
        else if (starts_with(a, "--priority=")) opts->priority_max = atoi(a + 11);
        else if (strcmp(a, "-b") == 0 || strcmp(a, "--boot") == 0) {
            opts->this_boot = 1;
        }
        else if (starts_with(a, "-b")) {
            opts->this_boot = 1;
            opts->boot_offset = atoi(a + 2);
        }
        else if (strcmp(a, "--list-boots") == 0) opts->list_boot_ids = 1;
        else if (strcmp(a, "-u") == 0 || strcmp(a, "--unit") == 0) {
            if (i + 1 < argc && opts->unit_filter_count < 64)
                strncpy(opts->unit_filter[opts->unit_filter_count++],
                       argv[++i], SYSCTL_MAX_NAME_LEN - 1);
        }
        else if (starts_with(a, "--unit=")) {
            if (opts->unit_filter_count < 64)
                strncpy(opts->unit_filter[opts->unit_filter_count++],
                       a + 7, SYSCTL_MAX_NAME_LEN - 1);
        }
        else if (starts_with(a, "--vacuum-size=")) opts->vacuum_size_mb = atoi(a + 14);
        else if (starts_with(a, "--vacuum-time=")) opts->vacuum_time_days = atoi(a + 14);
        else if (starts_with(a, "--vacuum-files=")) opts->vacuum_files = atoi(a + 15);
        else if (starts_with(a, "--output-fields=")) {
            if (opts->output_fields_count < 64)
                strncpy(opts->output_fields[opts->output_fields_count++],
                       a + 16, 63);
        }
        else if (starts_with(a, "_PID=") || starts_with(a, "_UID=") ||
                 starts_with(a, "_GID=") || strchr(a, '=')) {
            /* arbitrary field=value match */
            if (opts->field_grep_count < 64)
                strncpy(opts->field_grep[opts->field_grep_count++],
                       a, SYSCTL_MAX_LINE - 1);
        }
        else if (strcmp(a, "--version") == 0) { journalctl_print_version(); return 1; }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            journalctl_print_help(); return 1;
        }
    }
    return 0;
}

int journalctl_load(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton: scan /var/log/journal for *.journal files; do not parse
     * binary format. Just touch the directory to mark loaded. */
    DIR *dir = opendir(s->journal_path);
    if (dir) {
        s->journal_writable = (access(s->journal_path, W_OK) == 0);
        closedir(dir);
    } else {
        s->journal_writable = 0;
    }
    return 0;
}

int journalctl_write(SystemdToolsState *s, const JournalEntry *entry)
{
    if (!s || !entry) return -1;
    if (!s->journal_entries) {
        s->journal_cap = 1024;
        s->journal_entries = calloc((size_t)s->journal_cap, sizeof(JournalEntry));
        if (!s->journal_entries) return -1;
    }
    if (s->journal_len >= s->journal_cap) {
        int newcap = s->journal_cap * 2;
        if (newcap > JOURNAL_MAX_ENTRIES) newcap = JOURNAL_MAX_ENTRIES;
        if (newcap <= s->journal_cap) return -1;
        JournalEntry *nb = realloc(s->journal_entries,
                                   (size_t)newcap * sizeof(JournalEntry));
        if (!nb) return -1;
        s->journal_entries = nb;
        s->journal_cap = newcap;
    }
    s->journal_entries[s->journal_len++] = *entry;
    return 0;
}

static int journal_entry_matches(const JournalEntry *e, const JournalctlOptions *opts)
{
    if (!opts) return 1;
    if (opts->priority_max < 7 && e->priority > opts->priority_max) return 0;
    if (opts->priority_min > 0 && e->priority < opts->priority_min) return 0;
    if (opts->dmesg_only && !e->kernel_thread) return 0;
    if (opts->unit_filter_count > 0) {
        int match = 0;
        for (int i = 0; i < opts->unit_filter_count; i++)
            if (strcmp(opts->unit_filter[i], e->unit) == 0) { match = 1; break; }
        if (!match) return 0;
    }
    if (opts->identifier_filter_count > 0) {
        int match = 0;
        for (int i = 0; i < opts->identifier_filter_count; i++)
            if (strcmp(opts->identifier_filter[i], e->syslog_ident) == 0) { match = 1; break; }
        if (!match) return 0;
    }
    if (opts->pid_filter_count > 0) {
        int match = 0;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", (int)e->pid);
        for (int i = 0; i < opts->pid_filter_count; i++)
            if (strcmp(opts->pid_filter[i], buf) == 0) { match = 1; break; }
        if (!match) return 0;
    }
    if (opts->uid_filter_count > 0) {
        int match = 0;
        char buf[16]; snprintf(buf, sizeof(buf), "%u", (unsigned)e->uid);
        for (int i = 0; i < opts->uid_filter_count; i++)
            if (strcmp(opts->uid_filter[i], buf) == 0) { match = 1; break; }
        if (!match) return 0;
    }
    if (opts->since_us > 0 && e->realtime_us < opts->since_us) return 0;
    if (opts->until_us > 0 && e->realtime_us > opts->until_us) return 0;
    return 1;
}

static void journal_format_short(const JournalEntry *e, char *buf, size_t bufsz)
{
    char tbuf[64];
    time_t t = (time_t)(e->realtime_us / 1000000ULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(tbuf, sizeof(tbuf), "%b %d %H:%M:%S", &tm);
    const char *ident = e->syslog_ident[0] ? e->syslog_ident :
                        (e->comm[0] ? e->comm : "unknown");
    snprintf(buf, bufsz, "%s %s %s[%d]: %s",
             tbuf,
             e->hostname[0] ? e->hostname : "",
             ident, (int)e->pid,
             e->message[0] ? e->message : "");
}

int journalctl_iter_filtered(SystemdToolsState *s, const JournalctlOptions *opts,
                              int (*emit_line)(void *ctx, const JournalEntry *e, int idx),
                              void *ctx)
{
    if (!s || !emit_line) return -1;
    int rc = 0;
    int n_match = 0;
    if (opts && opts->reverse) {
        for (int i = s->journal_len - 1; i >= 0; i--) {
            const JournalEntry *e = &s->journal_entries[i];
            if (!journal_entry_matches(e, opts)) continue;
            rc = emit_line(ctx, e, n_match++);
            if (rc != 0) break;
            if (opts->lines > 0 && n_match >= opts->lines) break;
        }
    } else {
        int start = 0;
        if (opts && opts->lines > 0 && s->journal_len > opts->lines)
            start = s->journal_len - opts->lines;
        for (int i = start; i < s->journal_len; i++) {
            const JournalEntry *e = &s->journal_entries[i];
            if (!journal_entry_matches(e, opts)) continue;
            rc = emit_line(ctx, e, n_match++);
            if (rc != 0) break;
            if (opts && opts->lines > 0 && n_match >= opts->lines) break;
        }
    }
    return rc;
}

static int journal_dump_emit(void *ctx, const JournalEntry *e, int idx)
{
    FILE *f = (FILE *)ctx;
    (void)idx;
    char line[SYSCTL_MAX_LINE];
    journal_format_short(e, line, sizeof(line));
    fprintf(f, "%s\n", line);
    return 0;
}

int journalctl_dump(SystemdToolsState *s, const JournalctlOptions *opts)
{
    if (!s) return -1;
    /* header */
    if (opts && opts->header_show) {
        fprintf(stdout, "File Path: %s\n", s->journal_path);
        fprintf(stdout, "Entries:   %d\n", s->journal_len);
    }
    if (opts && opts->list_boot_ids) {
        fprintf(stdout, "IDX BOOT ID                          FIRST ENTRY              LAST ENTRY\n");
        /* skeleton: report current boot only */
        fprintf(stdout, "  0 00000000000000000000000000000000 n/a                       n/a\n");
        return 0;
    }
    if (opts && opts->disk_usage) {
        uint64_t used = 0, arch = 0;
        journalctl_get_usage(s, &used, &arch);
        printf("Archived and active journals currently take up %lluM in total.\n",
               (unsigned long long)(used / (1024 * 1024)));
        return 0;
    }
    if (opts && (opts->vacuum_size_mb || opts->vacuum_time_days || opts->vacuum_files)) {
        return journalctl_vacuum(s, opts);
    }
    if (opts && opts->rotate) return journalctl_rotate(s);
    if (opts && opts->flush)  return journalctl_flush(s);
    return journalctl_iter_filtered(s, opts, journal_dump_emit, stdout);
}

int journalctl_get_usage(SystemdToolsState *s, uint64_t *out_used_bytes,
                         uint64_t *out_archived_bytes)
{
    if (!s) return -1;
    uint64_t used = (uint64_t)s->journal_len * (uint64_t)sizeof(JournalEntry);
    if (out_used_bytes)    *out_used_bytes = used;
    if (out_archived_bytes)*out_archived_bytes = 0;
    return 0;
}

int journalctl_vacuum(SystemdToolsState *s, const JournalctlOptions *opts)
{
    if (!s || !opts) return -1;
    /* Skeleton: trim in-memory entries */
    if (opts->vacuum_size_mb > 0) {
        size_t limit = (size_t)opts->vacuum_size_mb * 1024 * 1024;
        size_t per = sizeof(JournalEntry);
        size_t keep = limit / per;
        if ((size_t)s->journal_len > keep) {
            size_t drop = (size_t)s->journal_len - keep;
            memmove(s->journal_entries, s->journal_entries + drop,
                    keep * sizeof(JournalEntry));
            s->journal_len = (int)keep;
        }
    }
    if (opts->vacuum_files > 0 && s->journal_len > opts->vacuum_files) {
        s->journal_len = opts->vacuum_files;
    }
    return 0;
}

int journalctl_flush(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton: flush /run/log/journal -> /var/log/journal. No-op here. */
    return 0;
}

int journalctl_rotate(SystemdToolsState *s)
{
    if (!s) return -1;
    /* Skeleton: rotate active journal. No-op here. */
    return 0;
}

/* ============================================================ *
 * loginctl
 * ============================================================ */

int loginctl_parse_arguments(SystemdToolsState *s, int argc, char **argv)
{
    if (!s) return -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;
        if      (strcmp(a, "--no-pager") == 0)      s->systemctl_no_pager = 1;
        else if (strcmp(a, "--no-legend") == 0)    s->systemctl_no_legend = 1;
        else if (strcmp(a, "--no-ask-password") == 0) s->systemctl_ask_password = 0;
        else if (strcmp(a, "--ask-password") == 0) s->systemctl_ask_password = 1;
        else if (strcmp(a, "--system") == 0)       s->systemctl_user = 0;
        else if (strcmp(a, "--user") == 0)         s->systemctl_user = 1;
        else if (strcmp(a, "--all") == 0 || strcmp(a, "-a") == 0) s->systemctl_all = 1;
        else if (strcmp(a, "--full") == 0)         s->systemctl_full = 1;
        else if (strcmp(a, "--quiet") == 0 || strcmp(a, "-q") == 0) s->systemctl_quiet = 1;
        else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) s->systemctl_verbose = 1;
        else if (starts_with(a, "--host=")) {
            strncpy(s->systemctl_host, a + 7, sizeof(s->systemctl_host) - 1);
        }
        else if (strcmp(a, "-H") == 0 && i + 1 < argc) {
            strncpy(s->systemctl_host, argv[++i], sizeof(s->systemctl_host) - 1);
        }
        else if (strcmp(a, "-M") == 0 && i + 1 < argc) {
            strncpy(s->systemctl_host, argv[++i], sizeof(s->systemctl_host) - 1);
        }
        else if (strcmp(a, "--version") == 0) { loginctl_print_version(); return 1; }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            loginctl_print_help(); return 1;
        }
        /* else: command - left for loginctl_main_internal */
    }
    return 0;
}

int loginctl_list_sessions(SystemdToolsState *s,
                           char out[LOGINCTL_MAX_SESSIONS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-10s %-15s %-15s %-15s %s",
                 "SESSION", "UID", "USER", "SEAT", "TTY");
    for (int i = 0; i < s->session_count && n < LOGINCTL_MAX_SESSIONS; i++) {
        SessionInfo *si = &s->sessions[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-10s %-15u %-15s %-15s %s",
                 si->session_str, (unsigned)si->uid, si->user,
                 si->seat_name[0] ? si->seat_name : "-",
                 si->tty[0] ? si->tty : (si->display[0] ? si->display : "-"));
    }
    *out_lines = n;
    return 0;
}

int loginctl_list_users(SystemdToolsState *s,
                        char out[LOGINCTL_MAX_USERS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-15s %-15s %s",
                 "UID", "USER", "STATE");
    for (int i = 0; i < s->user_count && n < LOGINCTL_MAX_USERS; i++) {
        UserInfo *ui = &s->users[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-15u %-15s %s",
                 (unsigned)ui->uid, ui->name,
                 ui->state[0] ? ui->state : "offline");
    }
    *out_lines = n;
    return 0;
}

int loginctl_list_seats(SystemdToolsState *s,
                        char out[LOGINCTL_MAX_SEATS][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-15s %s", "SEAT", "STATE");
    if (s->seat_count == 0) {
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-15s %s", "seat0", "active");
    }
    for (int i = 0; i < s->seat_count && n < LOGINCTL_MAX_SEATS; i++) {
        SeatInfo *st = &s->seats[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-15s %s", st->id, "active");
    }
    *out_lines = n;
    return 0;
}

int loginctl_list_machines(SystemdToolsState *s,
                           char out[LOGINCTL_MAX_MACHINES][SYSCTL_MAX_LINE], int *out_lines)
{
    if (!s || !out || !out_lines) return -1;
    int n = 0;
    if (!s->systemctl_no_legend)
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-20s %-15s %s",
                 "NAME", "STATE", "CLASS");
    for (int i = 0; i < s->machine_count && n < LOGINCTL_MAX_MACHINES; i++) {
        MachineInfo *m = &s->machines[i];
        snprintf(out[n++], SYSCTL_MAX_LINE, "%-20s %-15s %s",
                 m->name, m->state[0] ? m->state : "running",
                 m->class[0] ? m->class : "container");
    }
    *out_lines = n;
    return 0;
}

int loginctl_session_status(SystemdToolsState *s, const char *id)
{
    if (!s || !id) return -1;
    for (int i = 0; i < s->session_count; i++) {
        SessionInfo *si = &s->sessions[i];
        if (strcmp(si->session_str, id) == 0) {
            printf("%s (%u)\n", si->user, (unsigned)si->uid);
            printf("           TTY: %s\n", si->tty);
            printf("        Display: %s\n", si->display);
            printf("    Remote Host: %s\n", si->remote_host);
            printf("    Remote User: %s\n", si->remote_user);
            printf("        Service: %s\n", si->service);
            printf("           Type: %s\n", session_type_name(si->type));
            printf("          State: %s\n", session_state_name(si->state));
            printf("         Active: %s\n", si->active ? "yes" : "no");
            printf("      Seat Class: %s\n", si->class);
            return 0;
        }
    }
    fprintf(stderr, "loginctl: session %s not found\n", id);
    return -1;
}

int loginctl_user_status(SystemdToolsState *s, const char *user_or_uid)
{
    if (!s || !user_or_uid) return -1;
    uid_t uid = (uid_t)strtoul(user_or_uid, NULL, 10);
    for (int i = 0; i < s->user_count; i++) {
        UserInfo *ui = &s->users[i];
        if (strcmp(ui->name, user_or_uid) == 0 || (uid != 0 && ui->uid == uid)) {
            printf("%s (%u)\n", ui->name, (unsigned)ui->uid);
            printf("       State: %s\n", ui->state);
            printf("    Sessions: %d\n", ui->sessions_count);
            printf("    Linger: %s\n", ui->lingering ? "yes" : "no");
            printf("   Processes: %d\n", ui->n_processes);
            return 0;
        }
    }
    fprintf(stderr, "loginctl: user %s not found\n", user_or_uid);
    return -1;
}

int loginctl_seat_status(SystemdToolsState *s, const char *id)
{
    if (!s || !id) return -1;
    for (int i = 0; i < s->seat_count; i++) {
        SeatInfo *st = &s->seats[i];
        if (strcmp(st->id, id) == 0) {
            printf("%s\n", st->id);
            printf("    Active Session: %s\n",
                   st->active_session[0] ? st->active_session : "-");
            printf("      Sessions: %d\n", st->n_sessions);
            printf("       Can TTY: %s\n", st->can_tty ? "yes" : "no");
            printf("  Can Graphical: %s\n", st->can_graphical ? "yes" : "no");
            return 0;
        }
    }
    fprintf(stderr, "loginctl: seat %s not found\n", id);
    return -1;
}

int loginctl_machine_status(SystemdToolsState *s, const char *name)
{
    if (!s || !name) return -1;
    for (int i = 0; i < s->machine_count; i++) {
        MachineInfo *m = &s->machines[i];
        if (strcmp(m->name, name) == 0) {
            printf("%s\n", m->name);
            printf("       State: %s\n", m->state);
            printf("        Class: %s\n", m->class);
            printf("        Leader UID: %u\n", (unsigned)m->leader_uid);
            printf("   Root Directory: %s\n", m->root_directory);
            printf("   IP Addresses: %d\n", m->n_ip_addresses);
            return 0;
        }
    }
    fprintf(stderr, "loginctl: machine %s not found\n", name);
    return -1;
}

int loginctl_terminate_session(SystemdToolsState *s, const char *id, int signal)
{
    if (!s || !id) return -1;
    if (signal <= 0) signal = SIGTERM;
    for (int i = 0; i < s->session_count; i++) {
        SessionInfo *si = &s->sessions[i];
        if (strcmp(si->session_str, id) == 0) {
            if (si->leader > 0) kill(si->leader, signal);
            si->state = SESSION_STATE_CLOSING;
            return 0;
        }
    }
    return -1;
}

int loginctl_kill_session(SystemdToolsState *s, const char *id, int sig, const char *who)
{
    if (!s || !id) return -1;
    (void)who;
    if (sig <= 0) sig = SIGTERM;
    return loginctl_terminate_session(s, id, sig);
}

int loginctl_lock_session(SystemdToolsState *s, const char *id)
{
    if (!s || !id) return -1;
    for (int i = 0; i < s->session_count; i++) {
        if (strcmp(s->sessions[i].session_str, id) == 0) {
            s->sessions[i].locked_hint = 1;
            return 0;
        }
    }
    return -1;
}

int loginctl_unlock_session(SystemdToolsState *s, const char *id)
{
    if (!s || !id) return -1;
    for (int i = 0; i < s->session_count; i++) {
        if (strcmp(s->sessions[i].session_str, id) == 0) {
            s->sessions[i].locked_hint = 0;
            return 0;
        }
    }
    return -1;
}

int loginctl_activate(SystemdToolsState *s, const char *id)
{
    if (!s || !id) return -1;
    for (int i = 0; i < s->session_count; i++) {
        SessionInfo *si = &s->sessions[i];
        if (strcmp(si->session_str, id) == 0) {
            si->active = 1;
            si->state = SESSION_STATE_ACTIVE;
            /* Mark others inactive on the same seat */
            for (int k = 0; k < s->session_count; k++) {
                if (k != i && s->sessions[k].seat_id == si->seat_id)
                    s->sessions[k].active = 0;
            }
            return 0;
        }
    }
    return -1;
}

int loginctl_activate_lingering(SystemdToolsState *s, uid_t uid, int on)
{
    if (!s) return -1;
    for (int i = 0; i < s->user_count; i++) {
        if (s->users[i].uid == uid) {
            s->users[i].lingering = on ? 1 : 0;
            strcpy(s->users[i].state, on ? "lingering" :
                   (s->users[i].sessions_count > 0 ? "active" : "offline"));
            return 0;
        }
    }
    return -1;
}

int loginctl_enable_linger(SystemdToolsState *s, const char *user)
{
    if (!s || !user) return -1;
    uid_t uid = 0;
    int found = 0;
    /* numeric? */
    if (user[strspn(user, "0123456789")] == '\0')
        uid = (uid_t)strtoul(user, NULL, 10);
    for (int i = 0; i < s->user_count; i++) {
        if ((uid != 0 && s->users[i].uid == uid) ||
            strcmp(s->users[i].name, user) == 0) {
            uid = s->users[i].uid;
            found = 1;
            break;
        }
    }
    if (!found) {
        if (s->user_count >= LOGINCTL_MAX_USERS) return -1;
        UserInfo *ui = &s->users[s->user_count++];
        memset(ui, 0, sizeof(*ui));
        strncpy(ui->name, user, sizeof(ui->name) - 1);
        ui->uid = (uid != 0) ? uid : (uid_t)(1000 + s->user_count);
        uid = ui->uid;
    }
    return loginctl_activate_lingering(s, uid, 1);
}

int loginctl_disable_linger(SystemdToolsState *s, const char *user)
{
    if (!s || !user) return -1;
    uid_t uid = 0;
    if (user[strspn(user, "0123456789")] == '\0')
        uid = (uid_t)strtoul(user, NULL, 10);
    for (int i = 0; i < s->user_count; i++) {
        if ((uid != 0 && s->users[i].uid == uid) ||
            strcmp(s->users[i].name, user) == 0) {
            return loginctl_activate_lingering(s, s->users[i].uid, 0);
        }
    }
    return -1;
}

/* ============================================================ *
 * Help / version
 * ============================================================ */

void systemctl_print_version(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("systemd 255 (skeleton user-space implementation)\n");
}

void systemctl_print_help(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("\nUSAGE: systemctl [OPTIONS...] COMMAND [UNIT...]\n\n");
    printf("QUERY OR VERIFY STATE:\n");
    printf("  list-units [PATTERN...]              List loaded units\n");
    printf("  list-unit-files [PATTERN...]         List installed unit files\n");
    printf("  list-sockets [PATTERN...]           List sockets\n");
    printf("  list-timers [PATTERN...]            List timers\n");
    printf("  list-jobs                            List jobs\n");
    printf("  status [PATTERN...]                  Show unit state\n");
    printf("  is-active [PATTERN...]               Check unit active state\n");
    printf("  is-failed [PATTERN...]              Check unit failed state\n");
    printf("  is-enabled [UNIT...]                Check unit enabled state\n");
    printf("  show [UNIT...]                       Show unit properties\n");
    printf("  cat [UNIT...]                        Show unit file content\n");
    printf("  list-dependencies [UNIT...]          Show dependencies\n\n");
    printf("START / STOP / RESTART:\n");
    printf("  start [UNIT...]                      Start unit\n");
    printf("  stop [UNIT...]                       Stop unit\n");
    printf("  restart [UNIT...]                    Restart unit\n");
    printf("  try-restart [UNIT...]                Try restart unit\n");
    printf("  reload [UNIT...]                     Reload unit\n");
    printf("  isolate [UNIT]                       Isolate to target\n");
    printf("  kill [UNIT...]                        Kill unit processes\n");
    printf("  reset-failed [UNIT...]               Reset failed state\n\n");
    printf("ENABLE / DISABLE:\n");
    printf("  enable [UNIT...]                     Enable unit\n");
    printf("  disable [UNIT...]                    Disable unit\n");
    printf("  mask [UNIT...]                       Mask unit\n");
    printf("  unmask [UNIT...]                     Unmask unit\n");
    printf("  link [PATH...]                       Link unit file\n");
    printf("  preset [UNIT...]                     Apply preset policy\n");
    printf("  preset-all                           Apply preset policy to all\n");
    printf("  set-default [TARGET]                 Set default target\n");
    printf("  get-default                          Get default target\n\n");
    printf("DAEMON:\n");
    printf("  daemon-reload                        Reload unit files\n");
    printf("  daemon-reexec                        Re-execute systemd\n\n");
    printf("POWER:\n");
    printf("  poweroff / reboot / halt / suspend / hibernate / hybrid-sleep / kexec\n");
    printf("  switch-root [ROOT] [INIT]            Switch root and exec init\n");
    printf("  is-system-running                    Check system state\n\n");
    printf("OPTIONS:\n");
    printf("  --user, --system, --global           Scope\n");
    printf("  --no-block, --no-pager, --no-legend, --no-wall, --no-reload\n");
    printf("  --all, --failed, --reverse, --recursive, --runtime, --full\n");
    printf("  --plain, --now, --quiet, --verbose, --isolate\n");
    printf("  --type=TYPE, --state=STATE, --property=NAME\n");
    printf("  --host=[USER@]HOST, -M MACHINE\n");
    printf("  --version, --help\n");
}

void journalctl_print_version(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("systemd-journal 255 (skeleton user-space implementation)\n");
}

void journalctl_print_help(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("\nUSAGE: journalctl [OPTIONS...] [MATCHES...]\n\n");
    printf("OUTPUT:\n");
    printf("  -f, --follow                Follow journal\n");
    printf("  -n, --lines=N               Show last N lines (default 10)\n");
    printf("  -r, --reverse               Reverse output\n");
    printf("  -a, --all                   Show all fields\n");
    printf("  -o, --output=MODE           short|verbose|json|json-pretty|cat|export|with-unit\n");
    printf("  --no-pager                  Disable pager\n");
    printf("  --utc                       Show time in UTC\n");
    printf("  -m, --merge                 Merge all journals\n");
    printf("  -k, --dmesg                 Show kernel messages only\n");
    printf("  --output-fields=LIST        Limit fields shown\n\n");
    printf("FILTERING:\n");
    printf("  -b, --boot[=ID]             Show current/specific boot\n");
    printf("  --list-boots               List boot IDs\n");
    printf("  -u, --unit=UNIT            Filter by unit\n");
    printf("  -p, --priority=PRIORITY     Filter by priority (emerg..debug)\n");
    printf("  --since=TIME, --until=TIME  Filter by time\n");
    printf("  _PID=N, _UID=N, FIELD=val   Filter by field\n\n");
    printf("MAINTENANCE:\n");
    printf("  --header                   Show journal header\n");
    printf("  --disk-usage               Show disk usage\n");
    printf("  --vacuum-size=SIZE         Remove old journals up to SIZE\n");
    printf("  --vacuum-time=TIME         Remove journals older than TIME\n");
    printf("  --vacuum-files=N           Keep only N journals\n");
    printf("  --rotate                   Rotate active journal\n");
    printf("  --flush                    Flush /run to /var\n");
    printf("  --verify                   Verify journal file\n\n");
    printf("OPTIONS:\n");
    printf("  --version, --help\n");
}

void loginctl_print_version(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("systemd-login 255 (skeleton user-space implementation)\n");
}

void loginctl_print_help(void)
{
    printf("%s\n", SYSTEMD_TOOLS_VERSION_STR);
    printf("\nUSAGE: loginctl [OPTIONS...] {COMMAND} ...\n\n");
    printf("SESSIONS:\n");
    printf("  list-sessions                       List sessions\n");
    printf("  session-status [ID...]              Show session status\n");
    printf("  show-session [ID...]                Show session properties\n");
    printf("  activate [ID]                       Activate session\n");
    printf("  lock-session [ID...], unlock-session [ID...]\n");
    printf("  lock-sessions, unlock-sessions\n");
    printf("  terminate-session [ID...]           Terminate session\n");
    printf("  kill-session [ID...]                Kill session\n\n");
    printf("USERS:\n");
    printf("  list-users                          List users\n");
    printf("  user-status [USER...]               Show user status\n");
    printf("  show-user [USER...]                 Show user properties\n");
    printf("  enable-linger [USER...], disable-linger [USER...]\n");
    printf("  terminate-user [USER...]            Terminate user sessions\n");
    printf("  kill-user [USER...]                 Kill user sessions\n\n");
    printf("SEATS:\n");
    printf("  list-seats                          List seats\n");
    printf("  seat-status [NAME...]                Show seat status\n");
    printf("  show-seat [NAME...]                  Show seat properties\n");
    printf("  attach [NAME] DEVICE...             Attach device to seat\n");
    printf("  flush-devices                       Flush device associations\n");
    printf("  terminate-seat [NAME...]            Terminate seat sessions\n\n");
    printf("MACHINES:\n");
    printf("  list-machines                       List machines\n");
    printf("  machine-status [MACHINE...]          Show machine status\n\n");
    printf("OPTIONS:\n");
    printf("  --no-pager, --no-legend, --no-ask-password\n");
    printf("  --system, --user, --all, --full, --quiet, --verbose\n");
    printf("  --host=[USER@]HOST, -M MACHINE\n");
    printf("  --version, --help\n");
}

/* ============================================================ *
 * Main entry helpers
 * ============================================================ */

/* Static scratch buffer shared by the three main entry points. Sized to
 * SYSCTL_MAX_UNITS so any list function may safely write into it without
 * overflowing; using a single static buffer avoids ~64MB of stack usage. */
static char s_main_scratch[SYSCTL_MAX_UNITS][SYSCTL_MAX_LINE];

static void print_lines(char (*buf)[SYSCTL_MAX_LINE], int n)
{
    for (int i = 0; i < n; i++) printf("%s\n", buf[i]);
}

int systemctl_main_internal(SystemdToolsState *s, int argc, char **argv)
{
    if (!s) return -1;
    int pr = sysctl_parse_arguments(s, argc, argv);
    if (pr != 0) return pr > 0 ? 0 : pr;

    /* Find first non-option (the command) */
    const char *cmd = NULL;
    int first_arg = -1;
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') {
            cmd = argv[i];
            first_arg = i + 1;
            break;
        }
    }
    if (!cmd) {
        /* default: list-units */
        int n = 0;
        sysctl_list_units(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }

    if (strcmp(cmd, "list-units") == 0 || strcmp(cmd, "list") == 0) {
        int n = 0;
        sysctl_list_units(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-unit-files") == 0) {
        int n = 0;
        sysctl_list_unit_files(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-jobs") == 0) {
        int n = 0;
        sysctl_list_jobs(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-sockets") == 0) {
        int n = 0;
        sysctl_list_sockets(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-timers") == 0) {
        int n = 0;
        sysctl_list_timers(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-machines") == 0) {
        int n = 0;
        sysctl_list_machines(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-mounts") == 0) {
        int n = 0;
        sysctl_list_mounts(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-automounts") == 0) {
        int n = 0;
        sysctl_list_automounts(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-swaps") == 0) {
        int n = 0;
        sysctl_list_swaps(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-dependencies") == 0) {
        if (first_arg < 0 || first_arg >= argc) return -1;
        int n = 0;
        sysctl_list_dependencies(s, argv[first_arg],
                                 s_main_scratch, &n, s->systemctl_reversed);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "start") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_start_unit(s, argv[first_arg], UNIT_START_REPLACE);
    if (strcmp(cmd, "stop") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_stop_unit(s, argv[first_arg]);
    if (strcmp(cmd, "restart") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_restart_unit(s, argv[first_arg], 0);
    if (strcmp(cmd, "try-restart") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_restart_unit(s, argv[first_arg], 1);
    if (strcmp(cmd, "reload") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_reload_unit(s, argv[first_arg]);
    if (strcmp(cmd, "isolate") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_isolate_unit(s, argv[first_arg]);
    if (strcmp(cmd, "status") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_status_unit(s, argv[first_arg]);
    if (strcmp(cmd, "show") == 0 && first_arg >= 0 && first_arg < argc) {
        int n = 0;
        sysctl_show_unit(s, argv[first_arg], s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "kill") == 0 && first_arg >= 0 && first_arg < argc) {
        int sig = SIGTERM;
        const char *who = "all";
        /* parse --signal= / --kill-who= if present */
        for (int i = first_arg; i < argc; i++) {
            if (starts_with(argv[i], "--signal=")) sig = (int)strtol(argv[i] + 9, NULL, 10);
            else if (starts_with(argv[i], "--kill-who=")) who = argv[i] + 11;
        }
        /* first non-option after the command is the unit */
        for (int i = first_arg; i < argc; i++)
            if (argv[i][0] != '-') return sysctl_kill_unit(s, argv[i], sig, who);
        return -1;
    }
    if (strcmp(cmd, "reset-failed") == 0) {
        if (first_arg >= 0 && first_arg < argc)
            return sysctl_reset_failed_unit(s, argv[first_arg]);
        return sysctl_reset_failed_unit(s, "*");
    }
    if (strcmp(cmd, "enable") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_enable_unit(s, argv[first_arg], s->systemctl_now);
    if (strcmp(cmd, "disable") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_disable_unit(s, argv[first_arg], s->systemctl_now);
    if (strcmp(cmd, "mask") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_mask_unit(s, argv[first_arg], s->systemctl_runtime);
    if (strcmp(cmd, "unmask") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_unmask_unit(s, argv[first_arg], s->systemctl_runtime);
    if (strcmp(cmd, "link") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_link_unit(s, argv[first_arg], s->systemctl_runtime);
    if (strcmp(cmd, "preset") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_preset(s, argv[first_arg]);
    if (strcmp(cmd, "preset-all") == 0)
        return sysctl_preset_all(s);
    if (strcmp(cmd, "get-default") == 0) {
        char buf[SYSCTL_MAX_NAME_LEN];
        if (sysctl_get_default(s, buf) == 0) { printf("%s\n", buf); return 0; }
        return -1;
    }
    if (strcmp(cmd, "set-default") == 0 && first_arg >= 0 && first_arg < argc)
        return sysctl_set_default(s, argv[first_arg]);
    if (strcmp(cmd, "daemon-reload") == 0)
        return sysctl_daemon_reload(s);
    if (strcmp(cmd, "daemon-reexec") == 0)
        return sysctl_reexecute(s);
    if (strcmp(cmd, "poweroff") == 0) return sysctl_poweroff(s);
    if (strcmp(cmd, "reboot") == 0)   return sysctl_reboot(s);
    if (strcmp(cmd, "halt") == 0)     return sysctl_halt(s);
    if (strcmp(cmd, "suspend") == 0) return sysctl_suspend(s);
    if (strcmp(cmd, "hibernate") == 0)        return sysctl_hibernate(s);
    if (strcmp(cmd, "hybrid-sleep") == 0)     return sysctl_hybrid_sleep(s);
    if (strcmp(cmd, "kexec") == 0)            return sysctl_kexec(s);
    if (strcmp(cmd, "switch-root") == 0 && first_arg >= 0 && first_arg < argc) {
        const char *init = (first_arg + 1 < argc) ? argv[first_arg + 1] : NULL;
        return sysctl_switch_root(s, argv[first_arg], init);
    }
    if (strcmp(cmd, "is-system-running") == 0)
        return sysctl_is_system_running(s);

    fprintf(stderr, "systemctl: unknown command '%s'\n", cmd);
    systemctl_print_help();
    return -1;
}

int journalctl_main_internal(SystemdToolsState *s, int argc, char **argv)
{
    if (!s) return -1;
    JournalctlOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.lines = 10;
    opts.priority_min = 0;
    opts.priority_max = 7;
    int pr = journalctl_parse_arguments(s, argc, argv, &opts);
    if (pr != 0) return pr > 0 ? 0 : pr;
    journalctl_load(s);
    return journalctl_dump(s, &opts);
}

int loginctl_main_internal(SystemdToolsState *s, int argc, char **argv)
{
    if (!s) return -1;
    int pr = loginctl_parse_arguments(s, argc, argv);
    if (pr != 0) return pr > 0 ? 0 : pr;
    const char *cmd = NULL;
    int first_arg = -1;
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') {
            cmd = argv[i];
            first_arg = i + 1;
            break;
        }
    }
    if (!cmd) {
        int n = 0;
        loginctl_list_sessions(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-sessions") == 0) {
        int n = 0;
        loginctl_list_sessions(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-users") == 0) {
        int n = 0;
        loginctl_list_users(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-seats") == 0) {
        int n = 0;
        loginctl_list_seats(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "list-machines") == 0) {
        int n = 0;
        loginctl_list_machines(s, s_main_scratch, &n);
        print_lines(s_main_scratch, n);
        return 0;
    }
    if (strcmp(cmd, "session-status") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_session_status(s, argv[first_arg]);
    if (strcmp(cmd, "user-status") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_user_status(s, argv[first_arg]);
    if (strcmp(cmd, "seat-status") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_seat_status(s, argv[first_arg]);
    if (strcmp(cmd, "machine-status") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_machine_status(s, argv[first_arg]);
    if (strcmp(cmd, "activate") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_activate(s, argv[first_arg]);
    if (strcmp(cmd, "lock-session") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_lock_session(s, argv[first_arg]);
    if (strcmp(cmd, "unlock-session") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_unlock_session(s, argv[first_arg]);
    if (strcmp(cmd, "terminate-session") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_terminate_session(s, argv[first_arg], SIGTERM);
    if (strcmp(cmd, "kill-session") == 0 && first_arg >= 0 && first_arg < argc) {
        int sig = SIGTERM;
        for (int i = first_arg; i < argc; i++)
            if (starts_with(argv[i], "--signal=")) sig = (int)strtol(argv[i] + 9, NULL, 10);
        for (int i = first_arg; i < argc; i++)
            if (argv[i][0] != '-') return loginctl_kill_session(s, argv[i], sig, "all");
        return -1;
    }
    if (strcmp(cmd, "enable-linger") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_enable_linger(s, argv[first_arg]);
    if (strcmp(cmd, "disable-linger") == 0 && first_arg >= 0 && first_arg < argc)
        return loginctl_disable_linger(s, argv[first_arg]);

    fprintf(stderr, "loginctl: unknown command '%s'\n", cmd);
    loginctl_print_help();
    return -1;
}

#ifndef KENUXK_NO_MAIN_SYSTEMD_TOOLS
int main(int argc, char **argv)
{
    /* SystemdToolsState is enormous (it embeds Unit units[SYSCTL_MAX_UNITS]
     * and Job jobs[SYSCTL_MAX_JOB_ID]) and cannot live on the stack.
     * Use static storage so the BSS section is demand-paged by the kernel. */
    static SystemdToolsState s;
    systemd_tools_init(&s);
    int rc;
    /* Decide which sub-tool to dispatch based on argv[0] basename. */
    const char *bn = strrchr(argv[0], '/');
    bn = bn ? bn + 1 : argv[0];
#ifdef _WIN32
    const char *bnw = strrchr(argv[0], '\\');
    if (bnw && bnw + 1 > bn) bn = bnw + 1;
#endif
    const char *dot = strrchr(bn, '.');
    char base[64];
    if (dot) {
        size_t n = (size_t)(dot - bn);
        if (n >= sizeof(base)) n = sizeof(base) - 1;
        memcpy(base, bn, n);
        base[n] = '\0';
    } else {
        strncpy(base, bn, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
    }
    if (strstr(base, "journalctl"))
        rc = journalctl_main_internal(&s, argc, argv);
    else if (strstr(base, "loginctl"))
        rc = loginctl_main_internal(&s, argc, argv);
    else
        rc = systemctl_main_internal(&s, argc, argv);
    systemd_tools_cleanup(&s);
    return rc;
}
#endif
