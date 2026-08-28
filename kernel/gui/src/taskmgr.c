/* ============================================================
 * taskmgr.c - Task Manager Application
 *
 * Displays process list with CPU/memory usage, dashboard cards
 * for system resources, and live-updating curve graphs.
 * ============================================================ */

#include "taskmgr.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"
#include <timer.h>
#include <arch/process.h>

static window_t* taskmgr_win = NULL;
static widget_t* tm_cpu_label = NULL;
static widget_t* tm_mem_label = NULL;
static widget_t* tm_proc_count_label = NULL;
static uint64_t tm_last_refresh_ms = 0;
static int32_t tm_selected_proc = -1;
static kapi_process_info_t tm_processes[PROCESS_MAX];

/* Simple integer to string */
static void tm_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void tm_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void tm_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

/* Refresh button callback */
static void tm_refresh_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    taskmgr_refresh();
}

/* End task button callback */
static void tm_endtask_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (tm_selected_proc < 0) return;
    int32_t proc_count = KAPI_Process_GetList(tm_processes, PROCESS_MAX);
    if (proc_count > 0 && tm_selected_proc < proc_count) {
        KAPI_Process_Kill(tm_processes[tm_selected_proc].pid);
    }
    tm_selected_proc = -1;
    taskmgr_refresh();
}

/* Draw the process list and dashboard inside the window content area */
static void tm_draw_content(void) {
    if (!taskmgr_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(taskmgr_win, &cx, &cy, &cw, &ch);

    /* Dashboard cards row */
    uint32_t card_w = (cw - 20) / 4;
    uint32_t card_h = 50;
    uint32_t card_y = cy + 2;

    /* Update history for curves */
    kenux_render_update_history();

    /* CPU card */
    uint32_t cpu_val = KAPI_System_GetCPUUsage();
    KENUX_Render_DrawDashboardCard(cx + 4, card_y, card_w, card_h,
                                   KENUX_RESOURCE_CPU, cpu_val,
                                   CURVE_ICON_PKG_DEFAULT);

    /* Memory card */
    uint32_t mem_val = KAPI_System_GetMemUsage();
    KENUX_Render_DrawDashboardCard(cx + 4 + card_w + 4, card_y, card_w, card_h,
                                   KENUX_RESOURCE_MEM, mem_val,
                                   CURVE_ICON_PKG_DEFAULT);

    /* Disk card */
    uint32_t disk_val = KAPI_System_GetDiskUsage();
    KENUX_Render_DrawDashboardCard(cx + 4 + (card_w + 4) * 2, card_y, card_w, card_h,
                                   KENUX_RESOURCE_DISK, disk_val,
                                   CURVE_ICON_PKG_DEFAULT);

    /* Network card */
    uint32_t net_val = KAPI_System_GetNetUsage();
    KENUX_Render_DrawDashboardCard(cx + 4 + (card_w + 4) * 3, card_y, card_w, card_h,
                                   KENUX_RESOURCE_NET, net_val,
                                   CURVE_ICON_PKG_DEFAULT);

    /* CPU curve graph */
    uint32_t curve_y = card_y + card_h + 8;
    uint32_t curve_h = 40;
    uint32_t curve_w = cw - 8;

    const curve_history_t* cpu_hist = kenux_render_get_history(KENUX_RESOURCE_CPU);
    if (cpu_hist && cpu_hist->count > 1) {
        KENUX_Render_DrawCurve(cx + 4, curve_y, curve_w, curve_h,
                               cpu_hist, RGB(0x00, 0xD4, 0xFF),
                               KENUX_RESOURCE_CPU);
    } else {
        fb_fill_rect(cx + 4, curve_y, curve_w, curve_h, RGB(0x1A, 0x1A, 0x1A));
        font_draw_text(cx + 4, curve_y + 4, "CPU Graph (collecting...)",
                       RGB(0x80, 0x80, 0x80));
    }

    /* Process list header */
    uint32_t list_y = curve_y + curve_h + 8;
    fb_fill_rect(cx + 4, list_y, cw - 8, 16, RGB(0x2B, 0x2B, 0x2B));
    font_draw_text(cx + 8, list_y + 2, "PID   Name           CPU%   Mem(KB)",
                   RGB(0xFF, 0xFF, 0xFF));

    /* Process list */
    int32_t proc_count = KAPI_Process_GetList(tm_processes, PROCESS_MAX);
    if (proc_count > 0) {
        for (int32_t i = 0; i < proc_count; i++) {
            uint32_t row_y = list_y + 16 + (uint32_t)i * 14;
            if (row_y + 14 > cy + ch) break;

            /* Row background (alternating) */
            if (i % 2 == 0) {
                fb_fill_rect(cx + 4, row_y, cw - 8, 14, RGB(0x1E, 0x1E, 0x1E));
            } else {
                fb_fill_rect(cx + 4, row_y, cw - 8, 14, RGB(0x24, 0x24, 0x24));
            }

            /* PID */
            char pid_str[8];
            tm_int_to_str(tm_processes[i].pid, pid_str);
            font_draw_text(cx + 8, row_y + 2, pid_str, RGB(0xAE, 0xAE, 0xB0));

            /* Name */
            font_draw_text(cx + 48, row_y + 2, tm_processes[i].name, RGB(0xFF, 0xFF, 0xFF));

            /* CPU% */
            char cpu_str[8];
            tm_int_to_str(tm_processes[i].cpu_usage, cpu_str);
            tm_str_cat(cpu_str, "%");
            uint32_t cpu_color = RGB(0x00, 0xFF, 0x88);
            if (tm_processes[i].cpu_usage > 50) cpu_color = RGB(0xFF, 0x6B, 0x6B);
            else if (tm_processes[i].cpu_usage > 25) cpu_color = RGB(0xFF, 0xD7, 0x00);
            font_draw_text(cx + 190, row_y + 2, cpu_str, cpu_color);

            /* Memory */
            char mem_str[16];
            tm_int_to_str(tm_processes[i].mem_usage, mem_str);
            font_draw_text(cx + 240, row_y + 2, mem_str, RGB(0xAE, 0xAE, 0xB0));
        }
    }

    /* Update status labels */
    if (tm_cpu_label) {
        char buf[32];
        tm_str_copy(buf, "CPU: ");
        char num[8];
        tm_int_to_str(cpu_val, num);
        tm_str_cat(buf, num);
        tm_str_cat(buf, "%");
        widget_set_text(tm_cpu_label, buf);
    }
    if (tm_mem_label) {
        char buf[32];
        tm_str_copy(buf, "MEM: ");
        char num[8];
        tm_int_to_str(mem_val, num);
        tm_str_cat(buf, num);
        tm_str_cat(buf, "%");
        widget_set_text(tm_mem_label, buf);
    }
    if (tm_proc_count_label) {
        char buf[32];
        tm_str_copy(buf, "Processes: ");
        char num[8];
        tm_int_to_str(KAPI_Process_GetCount(), num);
        tm_str_cat(buf, num);
        widget_set_text(tm_proc_count_label, buf);
    }

    window_paint_widgets(taskmgr_win);
}

/* Content click handler — click on process list to select */
static void tm_content_click(window_t* win, uint32_t x, uint32_t y, void* user_data) {
    (void)win; (void)x; (void)user_data;
    if (!taskmgr_win || !taskmgr_win->visible) return;

    /* Process list starts after dashboard cards (52px) + curve (48px) + header (16px) */
    uint32_t list_start_y = 52 + 8 + 40 + 8 + 16;
    if (y < list_start_y) return;

    int32_t idx = (int32_t)((y - list_start_y) / 14);
    if (idx >= 0 && idx < 10) {
        tm_selected_proc = idx;
        tm_draw_content();
    }
}

window_t* taskmgr_create(void) {
    if (taskmgr_win) return taskmgr_win;

    taskmgr_win = window_create(120, 50, 420, 320, "Task Manager");
    taskmgr_win->titlebar_color = msf_settings.titlebar_active;
    taskmgr_win->visible = false;
    taskmgr_win->on_content_click = tm_content_click;
    window_set_statusbar(taskmgr_win, "10 processes");

    /* Status labels */
    tm_cpu_label = widget_create_label(10, 0, 100, 14, "CPU: --%", RGB(0x00, 0xD4, 0xFF));
    window_add_widget(taskmgr_win, tm_cpu_label);

    tm_mem_label = widget_create_label(110, 0, 100, 14, "MEM: --%", RGB(0x00, 0xFF, 0x88));
    window_add_widget(taskmgr_win, tm_mem_label);

    tm_proc_count_label = widget_create_label(220, 0, 120, 14, "Processes: 10", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(taskmgr_win, tm_proc_count_label);

    /* Buttons */
    widget_t* btn_refresh = widget_create_button(10, 270, 80, 24,
        "Refresh", tm_refresh_click, NULL);
    window_add_widget(taskmgr_win, btn_refresh);

    widget_t* btn_end = widget_create_button(100, 270, 80, 24,
        "End Task", tm_endtask_click, NULL);
    window_add_widget(taskmgr_win, btn_end);

    wm_add_window(taskmgr_win);
    return taskmgr_win;
}

void taskmgr_refresh(void) {
    if (!taskmgr_win || !taskmgr_win->visible) return;
    window_paint(taskmgr_win);
    tm_draw_content();
}

void taskmgr_paint_content(void) {
    if (!taskmgr_win || !taskmgr_win->visible) return;
    if (taskmgr_win->state.minimized) return;
    tm_draw_content();
}

void taskmgr_update(void) {
    if (!taskmgr_win || !taskmgr_win->visible) return;
    uint64_t now_ms = timer_jiffies_to_ms(timer_get_jiffies());
    /* 平稳刷新：约 2 秒更新一次，避免持续重绘曲线图拖慢桌面。 */
    if (tm_last_refresh_ms == 0 || now_ms - tm_last_refresh_ms >= 2000) {
        tm_last_refresh_ms = now_ms;
        taskmgr_paint_content();
    }
}