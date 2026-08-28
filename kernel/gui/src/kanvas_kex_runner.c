#include "kanvas_kex_runner.h"
#include "kanvas_animator.h"
#include "kapi.h"
#include <string.h>

static uint32_t kex_col32(kui_color_t c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static void fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x;
    if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t* p = (uint32_t*)((uint8_t*)fb + row * stride);
        for (int col = x; col < x + w; col++) p[col] = color;
    }
}

static void fill_rounded_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int r, uint32_t color)
{
    if (r <= 0) { fill_rect(fb, stride, fw, fh, x, y, w, h, color); return; }
    fill_rect(fb, stride, fw, fh, x + r, y, w - 2 * r, h, color);
    fill_rect(fb, stride, fw, fh, x, y + r, r, h - 2 * r, color);
    fill_rect(fb, stride, fw, fh, x + w - r, y + r, r, h - 2 * r, color);
    for (int dy = 0; dy < r; dy++) {
        int dx = (int)__builtin_sqrtf((float)(r * r - dy * dy));
        int cx1 = x + r - dx, cx2 = x + w - r + dx;
        int ry1 = y + r - dy - 1, ry2 = y - r + h + dy;
        fill_rect(fb, stride, fw, fh, cx1, ry1, cx2 - cx1, 1, color);
        fill_rect(fb, stride, fw, fh, cx1, ry2, cx2 - cx1, 1, color);
    }
}

kanvas_kex_runner_t* kanvas_kex_runner_create(void)
{
    kanvas_kex_runner_t* r = (kanvas_kex_runner_t*)kapi_malloc(sizeof(kanvas_kex_runner_t));
    if (!r) return NULL;
    memset(r, 0, sizeof(kanvas_kex_runner_t));
    r->current.state = KEX_STATE_IDLE;
    r->current.format = KEX_FORMAT_UNKNOWN;
    r->window_visible = false;
    r->window_x = 100; r->window_y = 100;
    r->window_w = 480; r->window_h = 360;
    r->bg_color = (kui_color_t){30, 30, 30, 255};
    r->fg_color = (kui_color_t){230, 230, 230, 255};
    r->accent_color = (kui_color_t){98, 0, 238, 255};
    r->success_color = (kui_color_t){64, 180, 64, 255};
    r->error_color = (kui_color_t){224, 64, 64, 255};
    return r;
}

void kanvas_kex_runner_destroy(kanvas_kex_runner_t* runner)
{
    if (!runner) return;
    kapi_free(runner);
}

void kanvas_kex_runner_paint(kanvas_kex_runner_t* runner, uint32_t* fb, int stride, int fw, int fh)
{
    if (!runner || !fb || !runner->window_visible) return;

    int wx = runner->window_x, wy = runner->window_y;
    int ww = runner->window_w, wh = runner->window_h;
    uint32_t bg = kex_col32(runner->bg_color);
    uint32_t fg = kex_col32(runner->fg_color);
    uint32_t accent = kex_col32(runner->accent_color);

    fill_rounded_rect(fb, stride, fw, fh, wx, wy, ww, wh, 12, bg);

    fill_rounded_rect(fb, stride, fw, fh, wx, wy, ww, 36, 12, 0x2D2D2DFF);
    kui_draw_text(fb, stride, fw, fh, wx + 12, wy + 10, "Kex Package Runner", fg, 14, 0);

    int close_x = wx + ww - 28, close_y = wy + 8;
    fill_rounded_rect(fb, stride, fw, fh, close_x, close_y, 20, 20, 4, 0xE04040FF);

    int content_y = wy + 48;

    switch (runner->current.state) {
    case KEX_STATE_IDLE:
        kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Drop .kex / .elf file here", 0x808080FF, 14, 0);
        kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 24, "or click Browse to select", 0x606060FF, 12, 0);
        {
            int btn_x = wx + 16, btn_y = content_y + 56;
            fill_rounded_rect(fb, stride, fw, fh, btn_x, btn_y, 100, 32, 6, accent);
            kui_draw_text(fb, stride, fw, fh, btn_x + 20, btn_y + 8, "Browse", 0xFFFFFFFF, 13, 0);
        }
        break;
    case KEX_STATE_VALIDATING:
        kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Validating package...", fg, 14, 0);
        break;
    case KEX_STATE_INSTALLING:
        {
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Installing:", fg, 14, 0);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 20, runner->current.path, accent, 12, 0);
            int bar_x = wx + 16, bar_y = content_y + 48;
            int bar_w = ww - 32;
            fill_rounded_rect(fb, stride, fw, fh, bar_x, bar_y, bar_w, 6, 3, 0x404040FF);
            int fill_w = (int)((float)bar_w * runner->current.progress);
            if (fill_w > 0) fill_rounded_rect(fb, stride, fw, fh, bar_x, bar_y, fill_w, 6, 3, accent);
        }
        break;
    case KEX_STATE_RUNNING:
        {
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Running:", fg, 14, 0);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 20, runner->current.path, accent, 12, 0);
            char pid_text[32];
            pid_text[0] = 'P'; pid_text[1] = 'I'; pid_text[2] = 'D'; pid_text[3] = ':'; pid_text[4] = ' ';
            int pid = (int)runner->current.pid;
            int pos = 5;
            if (pid == 0) { pid_text[pos++] = '0'; }
            else {
                char tmp[16]; int ti = 0;
                while (pid > 0) { tmp[ti++] = '0' + (pid % 10); pid /= 10; }
                while (ti > 0) pid_text[pos++] = tmp[--ti];
            }
            pid_text[pos] = '\0';
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 40, pid_text, 0x808080FF, 12, 0);
            if (runner->current.output_len > 0) {
                int out_y = content_y + 64;
                int max_lines = (wh - (out_y - wy) - 16) / 16;
                int line = 0;
                int start = 0;
                for (int i = 0; i < runner->current.output_len && line < max_lines; i++) {
                    if (runner->current.output[i] == '\n' || i == runner->current.output_len - 1) {
                        int len = i - start;
                        if (len > 0 && len < 128) {
                            char buf[128];
                            memcpy(buf, &runner->current.output[start], len);
                            buf[len] = '\0';
                            kui_draw_text(fb, stride, fw, fh, wx + 16, out_y + line * 16, buf, 0xA0A0A0FF, 11, 0);
                        }
                        start = i + 1;
                        line++;
                    }
                }
            }
        }
        break;
    case KEX_STATE_COMPLETED:
        {
            uint32_t succ = kex_col32(runner->success_color);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Completed successfully", succ, 14, 0);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 24, runner->current.path, 0x808080FF, 12, 0);
            int btn_x = wx + 16, btn_y = content_y + 56;
            fill_rounded_rect(fb, stride, fw, fh, btn_x, btn_y, 80, 32, 6, accent);
            kui_draw_text(fb, stride, fw, fh, btn_x + 16, btn_y + 8, "Run Again", 0xFFFFFFFF, 12, 0);
        }
        break;
    case KEX_STATE_ERROR:
        {
            uint32_t err = kex_col32(runner->error_color);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y, "Error occurred", err, 14, 0);
            kui_draw_text(fb, stride, fw, fh, wx + 16, content_y + 24, runner->current.output, 0xA0A0A0FF, 12, 0);
        }
        break;
    default:
        break;
    }
}

void kanvas_kex_runner_handle_mouse(kanvas_kex_runner_t* runner, int mx, int my, bool left_down, bool left_up)
{
    if (!runner || !runner->window_visible) return;
    (void)mx; (void)my; (void)left_down; (void)left_up;
}

void kanvas_kex_runner_update(kanvas_kex_runner_t* runner, uint64_t now_ms)
{
    if (!runner) return;
    (void)now_ms;
    if (runner->current.state == KEX_STATE_INSTALLING) {
        runner->current.progress += 0.02f;
        if (runner->current.progress >= 1.0f) {
            runner->current.progress = 1.0f;
            runner->current.state = KEX_STATE_COMPLETED;
        }
    }
}

kex_format_t kanvas_kex_detect_format(const char* path)
{
    if (!path) return KEX_FORMAT_UNKNOWN;
    size_t len = strlen(path);
    if (len >= 4) {
        const char* ext = path + len - 4;
        if (ext[0] == '.' && ext[1] == 'k' && ext[2] == 'e' && ext[3] == 'x') return KEX_FORMAT_KEX;
        if (ext[0] == '.' && ext[1] == 'k' && ext[2] == 'x' && ext[3] == 'p') return KEX_FORMAT_KXP;
        if (ext[0] == '.' && ext[1] == 'e' && ext[2] == 'l' && ext[3] == 'f') return KEX_FORMAT_ELF;
    }
    return KEX_FORMAT_UNKNOWN;
}

bool kanvas_kex_validate_package(const char* path, kex_package_info_t* info)
{
    if (!path || !info) return false;
    memset(info, 0, sizeof(kex_package_info_t));
    size_t len = strlen(path);
    if (len >= KEX_RUNNER_MAX_PATH) len = KEX_RUNNER_MAX_PATH - 1;
    memcpy(info->path, path, len);
    info->format = kanvas_kex_detect_format(path);
    info->executable = (info->format != KEX_FORMAT_UNKNOWN);
    return info->executable;
}

int kanvas_kex_install(kanvas_kex_runner_t* runner, const char* path)
{
    if (!runner || !path) return -1;
    size_t len = strlen(path);
    if (len >= KEX_RUNNER_MAX_PATH) len = KEX_RUNNER_MAX_PATH - 1;
    memcpy(runner->current.path, path, len);
    runner->current.path[len] = '\0';
    runner->current.format = kanvas_kex_detect_format(path);
    runner->current.state = KEX_STATE_INSTALLING;
    runner->current.progress = 0.0f;
    return 0;
}

int kanvas_kex_run(kanvas_kex_runner_t* runner, const char* path, bool use_gfx, int gfx_w, int gfx_h)
{
    if (!runner || !path) return -1;
    size_t len = strlen(path);
    if (len >= KEX_RUNNER_MAX_PATH) len = KEX_RUNNER_MAX_PATH - 1;
    memcpy(runner->current.path, path, len);
    runner->current.path[len] = '\0';
    runner->current.format = kanvas_kex_detect_format(path);
    runner->current.state = KEX_STATE_RUNNING;
    runner->current.pid = (uint32_t)(runner + 1);
    runner->current.use_gfx = use_gfx;
    runner->current.gfx_w = gfx_w;
    runner->current.gfx_h = gfx_h;
    runner->current.output_len = 0;
    return 0;
}

int kanvas_kex_run_with_args(kanvas_kex_runner_t* runner, const char* path, const char** args, int argc)
{
    int ret = kanvas_kex_run(runner, path, false, 0, 0);
    if (ret < 0) return ret;
    runner->current.arg_count = argc < KEX_RUNNER_MAX_ARGS ? argc : KEX_RUNNER_MAX_ARGS;
    for (int i = 0; i < runner->current.arg_count; i++) {
        size_t alen = strlen(args[i]);
        if (alen >= 64) alen = 63;
        memcpy(runner->current.args[i], args[i], alen);
        runner->current.args[i][alen] = '\0';
    }
    return 0;
}

void kanvas_kex_stop(kanvas_kex_runner_t* runner)
{
    if (!runner) return;
    runner->current.state = KEX_STATE_COMPLETED;
    runner->current.exit_code = 0;
}

void kanvas_kex_uninstall(kanvas_kex_runner_t* runner, const char* name)
{
    (void)runner; (void)name;
}

void kanvas_kex_show_window(kanvas_kex_runner_t* runner)
{
    if (!runner) return;
    runner->window_visible = true;
    runner->anim_id = kanvas_animator_window_open(runner, runner->window_x, runner->window_y, runner->window_w, runner->window_h);
}

void kanvas_kex_hide_window(kanvas_kex_runner_t* runner)
{
    if (!runner) return;
    runner->anim_id = kanvas_animator_window_close(runner);
}

const char* kanvas_kex_format_name(kex_format_t format)
{
    switch (format) {
    case KEX_FORMAT_KEX:   return "Kex";
    case KEX_FORMAT_KXP:   return "Kxp (Shared)";
    case KEX_FORMAT_ELF:   return "ELF";
    default:               return "Unknown";
    }
}

const char* kanvas_kex_state_name(kex_runner_state_t state)
{
    switch (state) {
    case KEX_STATE_IDLE:       return "Idle";
    case KEX_STATE_VALIDATING: return "Validating";
    case KEX_STATE_INSTALLING: return "Installing";
    case KEX_STATE_RUNNING:    return "Running";
    case KEX_STATE_COMPLETED:  return "Completed";
    case KEX_STATE_ERROR:      return "Error";
    default:                   return "Unknown";
    }
}
