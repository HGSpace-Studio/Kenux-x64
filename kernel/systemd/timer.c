#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>
#include <process.h>

static int sscanf(const char* str, const char* format, ...)
{
    (void)str; (void)format;
    return 0;
}

#define TIMER_PERSIST_FILE "/run/systemd/timers/timer_state"

typedef struct {
    int year;
    int month;
    int day;
    int weekday;
    int hour;
    int minute;
    int second;
    
    bool year_set;
    bool month_set;
    bool day_set;
    bool weekday_set;
    bool hour_set;
    bool minute_set;
    bool second_set;
    
    bool year_wildcard;
    bool month_wildcard;
    bool day_wildcard;
    bool weekday_wildcard;
    bool hour_wildcard;
    bool minute_wildcard;
    bool second_wildcard;
    
    int year_min;
    int year_max;
    int month_min;
    int month_max;
    int day_min;
    int day_max;
    int hour_min;
    int hour_max;
    int minute_min;
    int minute_max;
    int second_min;
    int second_max;
    int weekday_min;
    int weekday_max;
    
    int year_step;
    int month_step;
    int day_step;
    int hour_step;
    int minute_step;
    int second_step;
} calendar_spec_t;

static int timer_find_next_activation(timer_t* timer, uint64_t now);
static bool parse_calendar_spec(const char* calendar, calendar_spec_t* spec);
static int timer_calculate_next_elapse(timer_t* timer);

static int timer_write_persist(void)
{
    vfs_mkdir("/run/systemd/timers", 0755);
    
    int fd = vfs_open(TIMER_PERSIST_FILE, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    for (int i = 0; i < systemd.timer_count; i++) {
        timer_t* timer = &systemd.timers[i];
        
        char line[512];
        sprintf(line, "%s:%llu:%llu:%llu:%d:%d:%d:%d\n",
                timer->base.name,
                timer->last_trigger_time,
                timer->next_elapse_time,
                timer->wake_time,
                timer->triggers_left,
                timer->total_triggers,
                timer->persistent,
                timer->accuracy_usec);
        
        vfs_write(fd, line, strlen(line));
    }
    
    vfs_close(fd);
    return 0;
}

static int timer_read_persist(void)
{
    int fd = vfs_open(TIMER_PERSIST_FILE, FS_O_RDONLY, 0);
    if (fd < 0) return -1;
    
    char content[8192];
    int len = vfs_read(fd, content, sizeof(content) - 1);
    vfs_close(fd);
    
    if (len <= 0) return -1;
    content[len] = '\0';
    
    char* line = content;
    char* next;
    
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        
        char name[SYSTEMD_MAX_NAME];
        uint64_t last_trigger, next_elapse, wake_time;
        int triggers_left, total_triggers, persistent, accuracy_usec;
        
        if (sscanf(line, "%[^:]:%llu:%llu:%llu:%d:%d:%d:%d",
                   name, &last_trigger, &next_elapse, &wake_time,
                   &triggers_left, &total_triggers, &persistent, &accuracy_usec) == 8) {
            
            for (int i = 0; i < systemd.timer_count; i++) {
                timer_t* timer = &systemd.timers[i];
                if (strcmp(timer->base.name, name) == 0) {
                    timer->last_trigger_time = last_trigger;
                    timer->next_elapse_time = next_elapse;
                    timer->wake_time = wake_time;
                    timer->triggers_left = triggers_left;
                    timer->total_triggers = total_triggers;
                    timer->persistent = persistent;
                    timer->accuracy_usec = accuracy_usec;
                    break;
                }
            }
        }
        
        line = next + 1;
    }
    
    return 0;
}

static bool is_leap_year(int year)
{
    if (year % 4 != 0) return false;
    if (year % 100 != 0) return true;
    return (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

static int parse_range(const char* str, int* min_val, int* max_val, int* step_val)
{
    *min_val = *max_val = *step_val = 0;
    
    char* dash = strchr(str, '-');
    char* slash = strchr(str, '/');
    
    if (dash) {
        *dash = '\0';
        *min_val = atoi(str);
        *max_val = atoi(dash + 1);
        if (slash) {
            *slash = '\0';
            *step_val = atoi(slash + 1);
        }
    } else if (slash) {
        *slash = '\0';
        *min_val = atoi(str);
        *max_val = *min_val;
        *step_val = atoi(slash + 1);
    } else {
        *min_val = *max_val = atoi(str);
        *step_val = 1;
    }
    
    if (*step_val <= 0) *step_val = 1;
    return 0;
}

static bool parse_calendar_spec(const char* calendar, calendar_spec_t* spec)
{
    if (!calendar || !spec) return false;
    
    memset(spec, 0, sizeof(calendar_spec_t));
    
    spec->year_step = spec->month_step = spec->day_step = 
    spec->hour_step = spec->minute_step = spec->second_step = 1;
    
    spec->year_min = spec->year_max = -1;
    spec->month_min = spec->month_max = -1;
    spec->day_min = spec->day_max = -1;
    spec->hour_min = spec->hour_max = -1;
    spec->minute_min = spec->minute_max = -1;
    spec->second_min = spec->second_max = -1;
    
    if (strcmp(calendar, "hourly") == 0) {
        spec->minute_wildcard = true;
        spec->minute_set = true;
        spec->second_wildcard = true;
        spec->second_set = true;
        spec->minute_min = 0; spec->minute_max = 59;
        spec->second_min = 0; spec->second_max = 59;
        return true;
    }
    
    if (strcmp(calendar, "daily") == 0) {
        spec->hour_wildcard = true;
        spec->hour_set = true;
        spec->minute_wildcard = true;
        spec->minute_set = true;
        spec->second_wildcard = true;
        spec->second_set = true;
        spec->hour_min = 0; spec->hour_max = 23;
        spec->minute_min = 0; spec->minute_max = 59;
        spec->second_min = 0; spec->second_max = 59;
        return true;
    }
    
    if (strcmp(calendar, "weekly") == 0) {
        spec->weekday_wildcard = true;
        spec->weekday_set = true;
        spec->hour_wildcard = true;
        spec->hour_set = true;
        spec->minute_wildcard = true;
        spec->minute_set = true;
        spec->second_wildcard = true;
        spec->second_set = true;
        spec->weekday_min = 0; spec->weekday_max = 6;
        spec->hour_min = 0; spec->hour_max = 23;
        spec->minute_min = 0; spec->minute_max = 59;
        spec->second_min = 0; spec->second_max = 59;
        return true;
    }
    
    if (strcmp(calendar, "monthly") == 0) {
        spec->day_wildcard = true;
        spec->day_set = true;
        spec->hour_wildcard = true;
        spec->hour_set = true;
        spec->minute_wildcard = true;
        spec->minute_set = true;
        spec->second_wildcard = true;
        spec->second_set = true;
        spec->day_min = 1; spec->day_max = 31;
        spec->hour_min = 0; spec->hour_max = 23;
        spec->minute_min = 0; spec->minute_max = 59;
        spec->second_min = 0; spec->second_max = 59;
        return true;
    }
    
    if (strcmp(calendar, "yearly") == 0 || strcmp(calendar, "annually") == 0) {
        spec->month_wildcard = true;
        spec->month_set = true;
        spec->day_wildcard = true;
        spec->day_set = true;
        spec->hour_wildcard = true;
        spec->hour_set = true;
        spec->minute_wildcard = true;
        spec->minute_set = true;
        spec->second_wildcard = true;
        spec->second_set = true;
        spec->month_min = 1; spec->month_max = 12;
        spec->day_min = 1; spec->day_max = 31;
        spec->hour_min = 0; spec->hour_max = 23;
        spec->minute_min = 0; spec->minute_max = 59;
        spec->second_min = 0; spec->second_max = 59;
        return true;
    }
    
    char copy[256];
    strncpy(copy, calendar, sizeof(copy) - 1);
    
    char* date_part = copy;
    char* time_part = strchr(copy, ' ');
    
    if (time_part) {
        *time_part = '\0';
        time_part++;
    }
    
    if (date_part[0]) {
        char* part = date_part;
        char* comma;
        
        while ((comma = strchr(part, ',')) != NULL) {
            *comma = '\0';
            part = comma + 1;
        }
        
        if (strchr(date_part, '-')) {
            char* year_str = date_part;
            char* month_str = strchr(date_part, '-');
            char* day_str = strrchr(date_part, '-');
            
            if (month_str) *month_str++ = '\0';
            if (day_str && day_str != date_part) *day_str++ = '\0';
            
            if (year_str && *year_str) {
                spec->year_set = true;
                if (strcmp(year_str, "*") == 0) {
                    spec->year_wildcard = true;
                    spec->year_min = 1970; spec->year_max = 2100;
                } else {
                    int min, max, step;
                    parse_range(year_str, &min, &max, &step);
                    spec->year_min = min; spec->year_max = max; spec->year_step = step;
                }
            }
            
            if (month_str && *month_str) {
                spec->month_set = true;
                if (strcmp(month_str, "*") == 0) {
                    spec->month_wildcard = true;
                    spec->month_min = 1; spec->month_max = 12;
                } else {
                    int min, max, step;
                    parse_range(month_str, &min, &max, &step);
                    spec->month_min = min; spec->month_max = max; spec->month_step = step;
                }
            }
            
            if (day_str && *day_str) {
                spec->day_set = true;
                if (strcmp(day_str, "*") == 0) {
                    spec->day_wildcard = true;
                    spec->day_min = 1; spec->day_max = 31;
                } else {
                    int min, max, step;
                    parse_range(day_str, &min, &max, &step);
                    spec->day_min = min; spec->day_max = max; spec->day_step = step;
                }
            }
        }
        
        char* wday_str = strstr(date_part, "Mon");
        if (!wday_str) wday_str = strstr(date_part, "Tue");
        if (!wday_str) wday_str = strstr(date_part, "Wed");
        if (!wday_str) wday_str = strstr(date_part, "Thu");
        if (!wday_str) wday_str = strstr(date_part, "Fri");
        if (!wday_str) wday_str = strstr(date_part, "Sat");
        if (!wday_str) wday_str = strstr(date_part, "Sun");
        
        if (wday_str) {
            spec->weekday_set = true;
            static const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            for (int i = 0; i < 7; i++) {
                if (strncmp(wday_str, days[i], 3) == 0) {
                    spec->weekday = i;
                    spec->weekday_min = i;
                    spec->weekday_max = i;
                    break;
                }
            }
        }
    }
    
    if (time_part && *time_part) {
        char* hour_str = time_part;
        char* minute_str = strchr(time_part, ':');
        char* second_str = strrchr(time_part, ':');
        
        if (minute_str) *minute_str++ = '\0';
        if (second_str && second_str != time_part) *second_str++ = '\0';
        
        if (hour_str && *hour_str) {
            spec->hour_set = true;
            if (strcmp(hour_str, "*") == 0) {
                spec->hour_wildcard = true;
                spec->hour_min = 0; spec->hour_max = 23;
            } else {
                int min, max, step;
                parse_range(hour_str, &min, &max, &step);
                spec->hour_min = min; spec->hour_max = max; spec->hour_step = step;
            }
        }
        
        if (minute_str && *minute_str) {
            spec->minute_set = true;
            if (strcmp(minute_str, "*") == 0) {
                spec->minute_wildcard = true;
                spec->minute_min = 0; spec->minute_max = 59;
            } else {
                int min, max, step;
                parse_range(minute_str, &min, &max, &step);
                spec->minute_min = min; spec->minute_max = max; spec->minute_step = step;
            }
        }
        
        if (second_str && *second_str) {
            spec->second_set = true;
            if (strcmp(second_str, "*") == 0) {
                spec->second_wildcard = true;
                spec->second_min = 0; spec->second_max = 59;
            } else {
                int min, max, step;
                parse_range(second_str, &min, &max, &step);
                spec->second_min = min; spec->second_max = max; spec->second_step = step;
            }
        }
    }
    
    return true;
}

static int timer_calculate_calendar_next(timer_t* timer, uint64_t now)
{
    calendar_spec_t spec;
    if (!parse_calendar_spec(timer->calendar, &spec)) {
        journal_log(timer->base.name, "Invalid calendar spec", LOG_ERR);
        return -1;
    }
    
    time_t current = now;
    int year = 1970 + current / (365 * 24 * 3600);
    int remaining = current % (365 * 24 * 3600);
    int month = 1;
    
    for (int m = 1; m <= 12; m++) {
        int dim = days_in_month(year, m);
        if (remaining < dim * 24 * 3600) {
            month = m;
            break;
        }
        remaining -= dim * 24 * 3600;
    }
    
    int day = 1 + remaining / (24 * 3600);
    remaining %= (24 * 3600);
    int hour = remaining / 3600;
    int minute = (remaining % 3600) / 60;
    int second = remaining % 60;
    
    int target_year = year;
    int target_month = month;
    int target_day = day;
    int target_hour = hour;
    int target_minute = minute;
    int target_second = second;
    
    bool found = false;
    
    for (int y = target_year; y <= (spec.year_max > 0 ? spec.year_max : target_year + 10); y += spec.year_step) {
        for (int m = (y == target_year ? target_month : 1); m <= 12; m += spec.month_step) {
            int dim = days_in_month(y, m);
            for (int d = (y == target_year && m == target_month ? target_day : 1); d <= dim; d += spec.day_step) {
                for (int h = (y == target_year && m == target_month && d == target_day ? target_hour : 0); 
                     h <= 23; h += spec.hour_step) {
                    for (int min = (y == target_year && m == target_month && d == target_day && h == target_hour ? target_minute : 0);
                         min <= 59; min += spec.minute_step) {
                        for (int sec = (y == target_year && m == target_month && d == target_day && h == target_hour && min == target_minute ? target_second : 0);
                             sec <= 59; sec += spec.second_step) {
                            
                            if (spec.year_set && !spec.year_wildcard && (y < spec.year_min || y > spec.year_max)) continue;
                            if (spec.month_set && !spec.month_wildcard && (m < spec.month_min || m > spec.month_max)) continue;
                            if (spec.day_set && !spec.day_wildcard && (d < spec.day_min || d > spec.day_max)) continue;
                            if (spec.hour_set && !spec.hour_wildcard && (h < spec.hour_min || h > spec.hour_max)) continue;
                            if (spec.minute_set && !spec.minute_wildcard && (min < spec.minute_min || min > spec.minute_max)) continue;
                            if (spec.second_set && !spec.second_wildcard && (sec < spec.second_min || sec > spec.second_max)) continue;
                            
                            if (spec.weekday_set) {
                                int wday = 0;
                                int a = (14 - m) / 12;
                                int y0 = y - a;
                                int m0 = m + 12 * a - 2;
                                wday = (d + y0 + y0 / 4 - y0 / 100 + y0 / 400 + (31 * m0) / 12) % 7;
                                if (wday != spec.weekday) continue;
                            }
                            
                            int seconds = sec + min * 60 + h * 3600 + (d - 1) * 24 * 3600;
                            for (int i = 1; i < m; i++) {
                                seconds += days_in_month(y, i) * 24 * 3600;
                            }
                            
                            for (int i = 1970; i < y; i++) {
                                seconds += (is_leap_year(i) ? 366 : 365) * 24 * 3600;
                            }
                            
                            if (seconds > current) {
                                timer->next_elapse_time = seconds;
                                found = true;
                                goto done;
                            }
                        }
                    }
                }
            }
        }
    }
    
done:
    if (!found) {
        timer->next_elapse_time = current + timer->interval_sec;
    }
    
    return 0;
}

static int timer_calculate_next_elapse(timer_t* timer)
{
    if (!timer) return -1;
    
    uint64_t now = time_get_timestamp();
    
    if (timer->calendar && strlen(timer->calendar) > 0) {
        return timer_calculate_calendar_next(timer, now);
    }
    
    if (timer->interval_sec > 0) {
        if (timer->last_trigger_time > 0) {
            timer->next_elapse_time = timer->last_trigger_time + timer->interval_sec;
        } else {
            timer->next_elapse_time = now + timer->interval_sec;
        }
        
        if (timer->next_elapse_time <= now) {
            uint64_t elapsed = now - timer->next_elapse_time;
            uint64_t cycles = elapsed / timer->interval_sec;
            timer->next_elapse_time += (cycles + 1) * timer->interval_sec;
        }
    }
    
    if (timer->accuracy_usec > 0) {
        uint64_t rand_offset = (rand() % timer->accuracy_usec) / 1000000;
        timer->next_elapse_time += rand_offset;
    }
    
    return 0;
}

static void timer_trigger(timer_t* timer)
{
    if (!timer) return;
    
    journal_log(timer->base.name, "Timer triggered", LOG_INFO);
    
    timer->last_trigger_time = time_get_timestamp();
    timer->trigger_count++;
    
    if (timer->triggers_left > 0) {
        timer->triggers_left--;
    }
    
    timer_write_persist();
    
    if (timer->unit) {
        systemd_start_unit(timer->unit);
    }
    
    timer_calculate_next_elapse(timer);
}

static void timer_monitor_thread(void* arg)
{
    timer_t* timer = (timer_t*)arg;
    if (!timer) return;
    
    while (timer->base.state == UNIT_STATE_ACTIVE) {
        uint64_t now = time_get_timestamp();
        
        if (timer->next_elapse_time > 0 && now >= timer->next_elapse_time) {
            timer_trigger(timer);
        }
        
        msleep(100);
    }
}

int timer_load(const char* path)
{
    if (systemd.timer_count >= SYSTEMD_MAX_TIMERS) return -1;
    
    timer_t* timer = &systemd.timers[systemd.timer_count];
    memset(timer, 0, sizeof(timer_t));
    INIT_LIST_HEAD(&timer->base.unit_list);
    
    strncpy(timer->base.source_path, path, SYSTEMD_MAX_PATH_LEN - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(timer->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    timer->base.type = UNIT_TYPE_TIMER;
    timer->base.state = UNIT_STATE_DEAD;
    timer->accuracy_usec = 100000;
    timer->persistent = false;
    timer->wake_system = false;
    timer->triggers_left = -1;
    
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret > 0) {
        char section[64] = "";
        char key[128], value[1024];
        char* line = content;
        char* next;
        
        while ((next = strchr(line, '\n')) != NULL) {
            *next = '\0';
            trim(line);
            if (*line == '\0' || *line == '#') {
                line = next + 1;
                continue;
            }
            if (line[0] == '[' && parse_ini_section(line, section, sizeof(section)) == 0) {
                line = next + 1;
                continue;
            }
            if (parse_ini_keyvalue(line, key, sizeof(key), value, sizeof(value)) != 0) {
                line = next + 1;
                continue;
            }
            
            if (strcmp(section, "Unit") == 0) {
                parse_unit_section(&timer->base, key, value);
            } else if (strcmp(section, "Timer") == 0) {
                if (strcmp(key, "OnActiveSec") == 0) {
                    timer->interval_sec = atoi(value);
                } else if (strcmp(key, "OnBootSec") == 0) {
                    timer->interval_sec = atoi(value);
                } else if (strcmp(key, "OnStartupSec") == 0) {
                    timer->interval_sec = atoi(value);
                } else if (strcmp(key, "OnUnitActiveSec") == 0) {
                    timer->interval_sec = atoi(value);
                } else if (strcmp(key, "OnUnitInactiveSec") == 0) {
                    timer->interval_sec = atoi(value);
                } else if (strcmp(key, "OnCalendar") == 0) {
                    strncpy(timer->calendar, value, SYSTEMD_MAX_PATH_LEN - 1);
                } else if (strcmp(key, "AccuracySec") == 0) {
                    timer->accuracy_usec = (uint64_t)atoi(value) * 1000000;
                } else if (strcmp(key, "RandomizedDelaySec") == 0) {
                    timer->accuracy_usec = (uint64_t)atoi(value) * 1000000;
                } else if (strcmp(key, "Persistent") == 0) {
                    timer->persistent = (strcmp(value, "yes") == 0);
                } else if (strcmp(key, "WakeSystem") == 0) {
                    timer->wake_system = (strcmp(value, "yes") == 0);
                } else if (strcmp(key, "Unit") == 0) {
                    strncpy(timer->unit, value, SYSTEMD_MAX_NAME - 1);
                } else if (strcmp(key, "RemainAfterElapse") == 0) {
                    timer->remain_after_elapse = (strcmp(value, "yes") == 0);
                } else if (strcmp(key, "TriggersLeft") == 0) {
                    timer->triggers_left = atoi(value);
                }
            } else if (strcmp(section, "Install") == 0) {
                parse_install_section(&timer->base, key, value);
            }
            
            line = next + 1;
        }
    }
    
    timer->base.load_time = time_get_timestamp();
    
    list_add_tail(&timer->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &timer->base;
    systemd.timer_count++;
    
    journal_log(timer->base.name, "Timer loaded", LOG_INFO);
    return 0;
}

int timer_start(timer_t* timer)
{
    if (!timer) return -1;
    
    spinlock_lock(&systemd.lock);
    
    if (timer->base.state == UNIT_STATE_ACTIVE) {
        spinlock_unlock(&systemd.lock);
        return -1;
    }
    
    timer->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(timer->base.name, "Starting timer", LOG_INFO);
    
    timer_calculate_next_elapse(timer);
    
    spinlock_lock(&systemd.lock);
    timer->base.state = UNIT_STATE_ACTIVE;
    timer->base.active_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    thread_create(timer_monitor_thread, timer);
    
    char msg[256];
    sprintf(msg, "Timer active, next elapse at %llu", timer->next_elapse_time);
    journal_log(timer->base.name, msg, LOG_INFO);
    
    return 0;
}

int timer_stop(timer_t* timer)
{
    if (!timer) return -1;
    
    spinlock_lock(&systemd.lock);
    
    if (timer->base.state != UNIT_STATE_ACTIVE) {
        spinlock_unlock(&systemd.lock);
        return -1;
    }
    
    timer->base.state = UNIT_STATE_DEACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(timer->base.name, "Stopping timer", LOG_INFO);
    
    spinlock_lock(&systemd.lock);
    timer->base.state = UNIT_STATE_EXITED;
    timer->base.inactive_time = time_get_timestamp();
    spinlock_unlock(&systemd.lock);
    
    journal_log(timer->base.name, "Timer stopped", LOG_INFO);
    
    return 0;
}

void timer_reload_all(void)
{
    timer_read_persist();
    
    for (int i = 0; i < systemd.timer_count; i++) {
        timer_t* timer = &systemd.timers[i];
        if (timer->base.state == UNIT_STATE_ACTIVE) {
            timer_calculate_next_elapse(timer);
        }
    }
}

int timer_find_next_activation(timer_t* timer, uint64_t now)
{
    if (!timer) return -1;
    
    if (timer->calendar && strlen(timer->calendar) > 0) {
        return timer_calculate_calendar_next(timer, now);
    }
    
    return timer_calculate_next_elapse(timer);
}

void timer_monitor(timer_t* timer)
{
    if (!timer) return;
    
    if (timer->base.state != UNIT_STATE_ACTIVE) return;
    
    uint64_t now = time_get_timestamp();
    
    if (timer->next_elapse_time > 0 && now >= timer->next_elapse_time) {
        timer_trigger(timer);
    }
}