#ifndef KANVAS_TASK_MANAGER_H
#define KANVAS_TASK_MANAGER_H

#include "kapi_kanvasui.h"
#include "kapi_graphics2d.h"

#define KTM_WIN_W              600
#define KTM_WIN_H              450
#define KTM_TAB_H              36
#define KTM_HEADER_H           28
#define KTM_ROW_H              24
#define KTM_MAX_PROCS          512
#define KTM_MAX_VISIBLE        20
#define KTM_NAME_MAX           64
#define KTM_RADIUS             10

typedef enum {
    KTM_TAB_PROCS = 0,
    KTM_TAB_PERF,
    KTM_TAB_NET,
    KTM_TAB_DISK,
    KTM_TAB_COUNT
} ktm_tab_t;

typedef enum {
    KTM_STATE_RUNNING = 0,
    KTM_STATE_SLEEPING,
    KTM_STATE_STOPPED,
    KTM_STATE_ZOMBIE
} ktm_proc_state_t;

typedef struct {
    int pid;
    int ppid;
    char name[KTM_NAME_MAX];
    char user[32];
    ktm_proc_state_t state;
    uint64_t cpu_usage;
    uint64_t mem_usage;
    uint64_t mem_bytes;
    uint64_t disk_read;
    uint64_t disk_write;
    uint64_t start_time;
    int priority;
    int threads;
    bool selected;
} ktm_process_t;

typedef struct {
    uint64_t cpu_total;
    uint64_t cpu_user;
    uint64_t cpu_system;
    uint64_t cpu_idle;
    uint64_t mem_total;
    uint64_t mem_used;
    uint64_t mem_cached;
    uint64_t mem_buffers;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    uint64_t net_rx_packets;
    uint64_t net_tx_packets;
    uint64_t disk_total;
    uint64_t disk_used;
    uint64_t disk_read_rate;
    uint64_t disk_write_rate;
    uint64_t uptime_ms;
    int cpu_count;
    int cpu_freq_mhz;
    float cpu_temps[16];
} ktm_perf_t;

typedef struct {
    bool visible;
    int x, y, w, h;
    ktm_tab_t active_tab;
    ktm_process_t processes[KTM_MAX_PROCS];
    int proc_count;
    int selected_pid;
    int scroll_offset;
    int sort_column;
    bool sort_desc;
    ktm_perf_t perf;
    ktm_perf_t perf_history[60];
    int perf_history_idx;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t header_bg;
    kui_color_t hover_color;
    kui_color_t selected_color;
    kui_color_t graph_color;
} kanvas_task_manager_t;

kanvas_task_manager_t* kanvas_task_manager_create(void);
void kanvas_task_manager_destroy(kanvas_task_manager_t* tm);
void kanvas_task_manager_paint(kanvas_task_manager_t* tm, uint32_t* fb, int stride, int fw, int fh);
void kanvas_task_manager_update(kanvas_task_manager_t* tm, uint64_t now_ms);
void kanvas_task_manager_handle_mouse(kanvas_task_manager_t* tm, int mx, int my, bool left, bool right);
void kanvas_task_manager_handle_key(kanvas_task_manager_t* tm, int key, bool down);
void kanvas_task_manager_kill_process(kanvas_task_manager_t* tm, int pid);
void kanvas_task_manager_sort_by(kanvas_task_manager_t* tm, int column);
void kanvas_task_manager_refresh(kanvas_task_manager_t* tm);

#endif