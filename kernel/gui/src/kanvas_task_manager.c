#include "kanvas_task_manager.h"
#include "kapi.h"
#include <string.h>

static uint32_t tm_col32(kui_color_t c) { return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

static void tm_fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x; if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++) { uint32_t* p = (uint32_t*)((uint8_t*)fb + r * stride); for (int c = x; c < x + w; c++) p[c] = color; }
}

kanvas_task_manager_t* kanvas_task_manager_create(void)
{
    kanvas_task_manager_t* tm = (kanvas_task_manager_t*)kapi_kmalloc(sizeof(kanvas_task_manager_t));
    if (!tm) return NULL;
    memset(tm, 0, sizeof(kanvas_task_manager_t));
    tm->visible = false;
    tm->w = KTM_WIN_W; tm->h = KTM_WIN_H;
    tm->active_tab = KTM_TAB_PROCS;
    tm->sort_column = 0; tm->sort_desc = false;
    tm->bg_color = (kui_color_t){30, 30, 30, 255};
    tm->fg_color = (kui_color_t){230, 230, 230, 255};
    tm->accent_color = (kui_color_t){98, 0, 238, 255};
    tm->header_bg = (kui_color_t){40, 40, 40, 255};
    tm->hover_color = (kui_color_t){50, 50, 50, 255};
    tm->selected_color = (kui_color_t){98, 0, 238, 128};
    tm->graph_color = (kui_color_t){98, 0, 238, 255};
    tm->perf.cpu_count = 4;
    tm->perf.cpu_freq_mhz = 3200;
    tm->perf.mem_total = 8ULL * 1024 * 1024 * 1024;
    return tm;
}

void kanvas_task_manager_destroy(kanvas_task_manager_t* tm) { if (tm) kapi_kfree(tm); }

void kanvas_task_manager_paint(kanvas_task_manager_t* tm, uint32_t* fb, int stride, int fw, int fh)
{
    if (!tm || !tm->visible || !fb) return;
    uint32_t bg = tm_col32(tm->bg_color);
    uint32_t fg = tm_col32(tm->fg_color);
    uint32_t hbg = tm_col32(tm->header_bg);
    uint32_t accent = tm_col32(tm->accent_color);
    uint32_t graph = tm_col32(tm->graph_color);
    tm_fill_rect(fb, stride, fw, fh, tm->x, tm->y, tm->w, tm->h, bg);
    const char* tab_names[] = {"Processes", "Performance", "Network", "Disk"};
    int tab_x = tm->x + 8;
    for (int i = 0; i < KTM_TAB_COUNT; i++) {
        int tw = 80;
        uint32_t tcol = i == (int)tm->active_tab ? accent : 0x606060FF;
        kui_draw_text(fb, stride, fw, fh, tab_x, tm->y + 10, tab_names[i], tcol, 13, 0);
        tab_x += tw + 16;
    }
    tm_fill_rect(fb, stride, fw, fh, tm->x, tm->y + KTM_TAB_H, tm->w, 1, 0x404040FF);
    int content_y = tm->y + KTM_TAB_H + 4;
    switch (tm->active_tab) {
    case KTM_TAB_PROCS:
        {
            tm_fill_rect(fb, stride, fw, fh, tm->x, content_y, tm->w, KTM_HEADER_H, hbg);
            const char* headers[] = {"PID", "Name", "CPU%", "Mem%", "State"};
            int col_x[] = {tm->x + 8, tm->x + 60, tm->x + 250, tm->x + 320, tm->x + 390};
            for (int i = 0; i < 5; i++) kui_draw_text(fb, stride, fw, fh, col_x[i], content_y + 6, headers[i], fg, 12, 0);
            content_y += KTM_HEADER_H;
            for (int i = 0; i < tm->proc_count && i < KTM_MAX_VISIBLE; i++) {
                ktm_process_t* p = &tm->processes[i];
                int row_y = content_y + i * KTM_ROW_H;
                if (p->pid == tm->selected_pid) tm_fill_rect(fb, stride, fw, fh, tm->x, row_y, tm->w, KTM_ROW_H, tm_col32(tm->selected_color));
                char pid_s[16]; int v = p->pid, pos = 0;
                if (v == 0) pid_s[pos++] = '0';
                else { char tmp[8]; int ti = 0; while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; } while (ti > 0) pid_s[pos++] = tmp[--ti]; }
                pid_s[pos] = '\0';
                kui_draw_text(fb, stride, fw, fh, col_x[0], row_y + 4, pid_s, fg, 12, 0);
                kui_draw_text(fb, stride, fw, fh, col_x[1], row_y + 4, p->name, fg, 12, 0);
                char cpu_s[16] = ""; int cpu = (int)p->cpu_usage;
                { int ci = 0; if (cpu == 0) cpu_s[ci++] = '0'; else { char tmp[8]; int ti = 0; while (cpu > 0) { tmp[ti++] = '0' + (cpu % 10); cpu /= 10; } while (ti > 0) cpu_s[ci++] = tmp[--ti]; } cpu_s[ci++] = '%'; cpu_s[ci] = '\0'; }
                kui_draw_text(fb, stride, fw, fh, col_x[2], row_y + 4, cpu_s, fg, 12, 0);
            }
        }
        break;
    case KTM_TAB_PERF:
        {
            kui_draw_text(fb, stride, fw, fh, tm->x + 12, content_y, "CPU Usage", fg, 14, 0);
            int graph_x = tm->x + 12, graph_y = content_y + 24, graph_w = tm->w - 24, graph_h = 100;
            tm_fill_rect(fb, stride, fw, fh, graph_x, graph_y, graph_w, graph_h, 0x1A1A1AFF);
            for (int i = 0; i < 59; i++) {
                int idx = (tm->perf_history_idx + i) % 60;
                int next = (idx + 1) % 60;
                int y1 = graph_y + graph_h - (int)((float)tm->perf_history[idx].cpu_user / (float)tm->perf_history[idx].cpu_total * graph_h);
                int y2 = graph_y + graph_h - (int)((float)tm->perf_history[next].cpu_user / (float)tm->perf_history[next].cpu_total * graph_h);
                int x1 = graph_x + i * graph_w / 59;
                int x2 = graph_x + (i + 1) * graph_w / 59;
                tm_fill_rect(fb, stride, fw, fh, x1, y1, x2 - x1 + 1, 2, graph);
                (void)y2;
            }
            content_y = graph_y + graph_h + 16;
            kui_draw_text(fb, stride, fw, fh, tm->x + 12, content_y, "Memory", fg, 14, 0);
            int bar_y = content_y + 24, bar_h = 20;
            tm_fill_rect(fb, stride, fw, fh, graph_x, bar_y, graph_w, bar_h, 0x1A1A1AFF);
            int fill_w = (int)((float)graph_w * (float)tm->perf.mem_used / (float)tm->perf.mem_total);
            if (fill_w > 0) tm_fill_rect(fb, stride, fw, fh, graph_x, bar_y, fill_w, bar_h, graph);
        }
        break;
    case KTM_TAB_NET:
        kui_draw_text(fb, stride, fw, fh, tm->x + 12, content_y, "Network Statistics", fg, 14, 0);
        break;
    case KTM_TAB_DISK:
        kui_draw_text(fb, stride, fw, fh, tm->x + 12, content_y, "Disk Usage", fg, 14, 0);
        break;
    }
}

void kanvas_task_manager_update(kanvas_task_manager_t* tm, uint64_t now_ms)
{
    if (!tm) return;
    (void)now_ms;
    tm->perf_history[tm->perf_history_idx] = tm->perf;
    tm->perf_history_idx = (tm->perf_history_idx + 1) % 60;
}

void kanvas_task_manager_handle_mouse(kanvas_task_manager_t* tm, int mx, int my, bool left, bool right) { (void)tm; (void)mx; (void)my; (void)left; (void)right; }
void kanvas_task_manager_handle_key(kanvas_task_manager_t* tm, int key, bool down) { (void)tm; (void)key; (void)down; }
void kanvas_task_manager_kill_process(kanvas_task_manager_t* tm, int pid) { (void)tm; (void)pid; }
void kanvas_task_manager_sort_by(kanvas_task_manager_t* tm, int column) { if (tm) { if (tm->sort_column == column) tm->sort_desc = !tm->sort_desc; else { tm->sort_column = column; tm->sort_desc = false; } } }
void kanvas_task_manager_refresh(kanvas_task_manager_t* tm) { (void)tm; }