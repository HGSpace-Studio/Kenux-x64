/* ============================================================
 * terminal.c - Integrated Terminal Application
 *
 * Provides an interactive command-line shell running inside a
 * GUI window. Supports built-in commands that call real kernel
 * APIs for process listing, memory info, CPU model, uptime,
 * VFS directory listing, and more.
 *
 * Visual style: black background (#0C0C0C) with green prompt
 * and white/light-gray output text. Blinking block cursor.
 * ============================================================ */

#include "terminal.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"
#include <arch/fs.h>

/* ---- Terminal configuration ---- */
#define TERM_MAX_LINES     24
#define TERM_LINE_WIDTH    72
#define TERM_LINE_HEIGHT   14
#define TERM_TITLE_H       18
#define TERM_INPUT_H       16
#define TERM_BLINK_TICKS   6    /* cursor blink period (~300ms at 50ms GUI tick) */

/* ---- Terminal colours ---- */
#define TERM_BG_COLOR      RGB(0x0C, 0x0C, 0x0C)
#define TERM_TITLE_BG      RGB(0x1A, 0x1A, 0x1A)
#define TERM_GREEN         RGB(0x00, 0xFF, 0x00)
#define TERM_WHITE         RGB(0xFF, 0xFF, 0xFF)
#define TERM_GRAY          RGB(0xC0, 0xC0, 0xC0)
#define TERM_DIM           RGB(0x60, 0x60, 0x60)
#define TERM_BORDER        RGB(0x33, 0x33, 0x33)

#define TERM_PROMPT        "kenux@localhost:~$ "

/* ---- Terminal state ---- */
typedef struct {
    char     lines[TERM_MAX_LINES][TERM_LINE_WIDTH];
    uint32_t line_count;
    char     input_buf[TERM_LINE_WIDTH];
    uint32_t input_len;
    uint32_t cursor_pos;
    char     cwd[64];
} terminal_state_t;

static window_t*        terminal_win = NULL;
static terminal_state_t term_state;
static uint32_t         term_blink_counter = 0;
static int              term_cursor_visible = 1;

/* ============================================================
 * String helper functions (freestanding, no stdlib)
 * ============================================================ */

static void term_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void term_str_copy_n(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void term_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static uint32_t term_str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static int term_str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void term_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* Pad or truncate a string to exactly `width` characters. */
static void term_pad_str(char* buf, const char* str, uint32_t width) {
    uint32_t i = 0;
    while (str[i] && i < width) { buf[i] = str[i]; i++; }
    while (i < width) { buf[i] = ' '; i++; }
    buf[i] = '\0';
}

/* ============================================================
 * Terminal output buffer management
 * ============================================================ */

/* Append a line to the output buffer. If the buffer is full,
 * shift all lines up by one to make room. */
static void term_append_line(const char* text) {
    if (term_state.line_count >= TERM_MAX_LINES) {
        for (uint32_t i = 0; i < TERM_MAX_LINES - 1; i++) {
            term_str_copy(term_state.lines[i], term_state.lines[i + 1]);
        }
        term_state.line_count = TERM_MAX_LINES - 1;
    }
    term_str_copy_n(term_state.lines[term_state.line_count], text, TERM_LINE_WIDTH);
    term_state.line_count++;
}

/* Clear all output lines. */
static void term_clear_output(void) {
    term_state.line_count = 0;
}

/* ============================================================
 * Command implementations
 * ============================================================ */

static void term_cmd_help(void) {
    term_append_line("Available commands:");
    term_append_line("  help     - Show this help message");
    term_append_line("  ls       - List files in current directory");
    term_append_line("  ps       - Show running processes");
    term_append_line("  mem      - Show memory information");
    term_append_line("  cpu      - Show CPU information");
    term_append_line("  uptime   - Show system uptime");
    term_append_line("  clear    - Clear the terminal screen");
    term_append_line("  echo     - Echo text to output");
    term_append_line("  ver      - Show system version");
    term_append_line("  date     - Show current time");
    term_append_line("  whoami   - Show current user");
}

static void term_cmd_ls(void) {
    /* Try real VFS first */
    if (vfs_root) {
        vfs_node_t* child = vfs_root->children;
        uint32_t count = 0;
        while (child) {
            char line[TERM_LINE_WIDTH];
            if (child->type == FS_TYPE_DIRECTORY) {
                term_str_copy(line, "[DIR]  ");
            } else {
                term_str_copy(line, "[FILE] ");
            }
            term_str_cat(line, child->name);
            term_append_line(line);
            child = child->next;
            count++;
        }
        if (count > 0) return;
    }

    /* Fallback: simulated root directory listing */
    term_append_line("[DIR]  System");
    term_append_line("[DIR]  Documents");
    term_append_line("[DIR]  Downloads");
    term_append_line("[FILE] config.sys");
    term_append_line("[FILE] kernel.log");
}

static void term_cmd_ps(void) {
    term_append_line("PID   Name             CPU%  Mem(KB)");
    term_append_line("---   ----             ----  -------");

    kapi_process_info_t procs[10];
    uint32_t proc_count = 10;
    int32_t ret = KAPI_Process_GetList(procs, proc_count);
    if (ret == KAPI_OK) {
        for (uint32_t i = 0; i < proc_count; i++) {
            char line[TERM_LINE_WIDTH];
            char num[16];
            char field[32];

            term_str_copy(line, "");

            /* PID (5 chars wide) */
            term_int_to_str(procs[i].pid, num);
            term_pad_str(field, num, 5);
            term_str_cat(line, field);
            term_str_cat(line, " ");

            /* Name (16 chars wide) */
            term_pad_str(field, procs[i].name, 16);
            term_str_cat(line, field);
            term_str_cat(line, " ");

            /* CPU usage (5 chars wide) */
            term_int_to_str(procs[i].cpu_usage, num);
            term_str_cat(num, "%");
            term_pad_str(field, num, 4);
            term_str_cat(line, field);
            term_str_cat(line, "  ");

            /* Memory */
            term_int_to_str(procs[i].mem_usage, num);
            term_str_cat(line, num);

            term_append_line(line);
        }
    } else {
        term_append_line("  (unable to retrieve process list)");
    }
}

static void term_cmd_mem(void) {
    char line[TERM_LINE_WIDTH];
    char num[16];

    uint32_t total = KAPI_System_GetMemTotal();
    uint32_t free_mem = KAPI_System_GetMemFree();
    uint32_t used = total - free_mem;

    term_str_copy(line, "Memory Total: ");
    term_int_to_str(total, num);
    term_str_cat(line, num);
    term_str_cat(line, " KB (");
    term_int_to_str(total / 1024, num);
    term_str_cat(line, num);
    term_str_cat(line, " MB)");
    term_append_line(line);

    term_str_copy(line, "Memory Used:  ");
    term_int_to_str(used, num);
    term_str_cat(line, num);
    term_str_cat(line, " KB (");
    term_int_to_str(used / 1024, num);
    term_str_cat(line, num);
    term_str_cat(line, " MB)");
    term_append_line(line);

    term_str_copy(line, "Memory Free:  ");
    term_int_to_str(free_mem, num);
    term_str_cat(line, num);
    term_str_cat(line, " KB (");
    term_int_to_str(free_mem / 1024, num);
    term_str_cat(line, num);
    term_str_cat(line, " MB)");
    term_append_line(line);
}

static void term_cmd_cpu(void) {
    char line[TERM_LINE_WIDTH];
    char model[48];
    char num[16];

    KAPI_System_GetCPUModel(model, sizeof(model));

    term_str_copy(line, "CPU Model: ");
    term_str_cat(line, model);
    term_append_line(line);

    uint32_t usage = KAPI_System_GetCPUUsage();
    term_str_copy(line, "CPU Usage: ");
    term_int_to_str(usage, num);
    term_str_cat(line, num);
    term_str_cat(line, "%");
    term_append_line(line);
}

static void term_cmd_uptime(void) {
    uint32_t up = KAPI_System_Uptime();
    char line[TERM_LINE_WIDTH];
    char num[16];

    uint32_t days = up / 86400;
    uint32_t hours = (up % 86400) / 3600;
    uint32_t mins = (up % 3600) / 60;
    uint32_t secs = up % 60;

    term_str_copy(line, "Uptime: ");
    if (days > 0) {
        term_int_to_str(days, num);
        term_str_cat(line, num);
        term_str_cat(line, "d ");
    }
    term_int_to_str(hours, num);
    term_str_cat(line, num);
    term_str_cat(line, "h ");
    term_int_to_str(mins, num);
    term_str_cat(line, num);
    term_str_cat(line, "m ");
    term_int_to_str(secs, num);
    term_str_cat(line, num);
    term_str_cat(line, "s");
    term_append_line(line);

    term_str_copy(line, "Total seconds: ");
    term_int_to_str(up, num);
    term_str_cat(line, num);
    term_append_line(line);
}

static void term_cmd_clear(void) {
    term_clear_output();
}

static void term_cmd_echo(const char* args) {
    if (args[0] == '\0') {
        term_append_line("");
    } else {
        term_append_line(args);
    }
}

static void term_cmd_ver(void) {
    char line[TERM_LINE_WIDTH];
    char ver[48];

    KAPI_System_GetOSVersion(ver, sizeof(ver));

    term_str_copy(line, "KenuxOS ");
    term_str_cat(line, ver);
    term_append_line(line);

    term_append_line("Kernel: KenuxK 4.0 x86_64");
    term_append_line("GUI: UEFI GOP framebuffer");
}

static void term_cmd_date(void) {
    /* No RTC available; derive time from kernel uptime.
     * The system clock starts at 12:00 (matching the GUI taskbar). */
    uint32_t up = KAPI_System_Uptime();
    uint32_t total_sec = 12 * 3600 + up;
    uint32_t hour = (total_sec / 3600) % 24;
    uint32_t min = (total_sec / 60) % 60;
    uint32_t sec = total_sec % 60;

    char line[TERM_LINE_WIDTH];
    char num[16];

    term_str_copy(line, "Current time: ");
    if (hour < 10) term_str_cat(line, "0");
    term_int_to_str(hour, num);
    term_str_cat(line, num);
    term_str_cat(line, ":");
    if (min < 10) term_str_cat(line, "0");
    term_int_to_str(min, num);
    term_str_cat(line, num);
    term_str_cat(line, ":");
    if (sec < 10) term_str_cat(line, "0");
    term_int_to_str(sec, num);
    term_str_cat(line, num);
    term_append_line(line);
}

static void term_cmd_whoami(void) {
    term_append_line("root");
}

/* ============================================================
 * Command parsing and execution
 * ============================================================ */

static void term_execute_command(void) {
    /* Echo the prompt + command to output history */
    char echo[TERM_LINE_WIDTH];
    term_str_copy(echo, TERM_PROMPT);
    term_str_cat(echo, term_state.input_buf);
    term_append_line(echo);

    /* Extract command name (up to first space) */
    char cmd[32];
    uint32_t i = 0;
    while (term_state.input_buf[i] &&
           term_state.input_buf[i] != ' ' &&
           i < sizeof(cmd) - 1) {
        cmd[i] = term_state.input_buf[i];
        i++;
    }
    cmd[i] = '\0';

    /* Skip spaces to find arguments */
    while (term_state.input_buf[i] == ' ') i++;
    const char* args = &term_state.input_buf[i];

    /* Dispatch to command handler */
    if (term_str_cmp(cmd, "help") == 0) {
        term_cmd_help();
    } else if (term_str_cmp(cmd, "ls") == 0) {
        term_cmd_ls();
    } else if (term_str_cmp(cmd, "ps") == 0) {
        term_cmd_ps();
    } else if (term_str_cmp(cmd, "mem") == 0) {
        term_cmd_mem();
    } else if (term_str_cmp(cmd, "cpu") == 0) {
        term_cmd_cpu();
    } else if (term_str_cmp(cmd, "uptime") == 0) {
        term_cmd_uptime();
    } else if (term_str_cmp(cmd, "clear") == 0) {
        term_cmd_clear();
    } else if (term_str_cmp(cmd, "echo") == 0) {
        term_cmd_echo(args);
    } else if (term_str_cmp(cmd, "ver") == 0) {
        term_cmd_ver();
    } else if (term_str_cmp(cmd, "date") == 0) {
        term_cmd_date();
    } else if (term_str_cmp(cmd, "whoami") == 0) {
        term_cmd_whoami();
    } else if (cmd[0] == '\0') {
        /* Empty command — do nothing */
    } else {
        char msg[TERM_LINE_WIDTH];
        term_str_copy(msg, "Unknown command: ");
        term_str_cat(msg, cmd);
        term_str_cat(msg, " (type 'help' for commands)");
        term_append_line(msg);
    }

    /* Clear the input buffer */
    term_state.input_len = 0;
    term_state.input_buf[0] = '\0';
}

/* ============================================================
 * Terminal content drawing
 * ============================================================ */

static void term_draw_content(void) {
    if (!terminal_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(terminal_win, &cx, &cy, &cw, &ch);

    /* Fill entire content area with terminal background */
    fb_fill_rect(cx, cy, cw, ch, TERM_BG_COLOR);

    /* ---- Title bar ---- */
    fb_fill_rect(cx, cy, cw, TERM_TITLE_H, TERM_TITLE_BG);
    gfx_draw_hline(cx, cy + TERM_TITLE_H, cw, TERM_BORDER);
    font_draw_text(cx + 4, cy + 2, "Kenux Terminal - /", TERM_GREEN);

    /* ---- Output area ---- */
    uint32_t out_y = cy + TERM_TITLE_H + 2;
    uint32_t out_h = ch - TERM_TITLE_H - 2 - TERM_INPUT_H;
    uint32_t max_visible = out_h / TERM_LINE_HEIGHT;
    if (max_visible == 0) max_visible = 1;

    /* Determine which lines to display (show most recent) */
    uint32_t start = 0;
    if (term_state.line_count > max_visible) {
        start = term_state.line_count - max_visible;
    }

    for (uint32_t i = start; i < term_state.line_count; i++) {
        uint32_t ly = out_y + (i - start) * TERM_LINE_HEIGHT;
        if (ly + TERM_LINE_HEIGHT > cy + ch - TERM_INPUT_H) break;
        font_draw_text(cx + 4, ly, term_state.lines[i], TERM_GRAY);
    }

    /* ---- Input line ---- */
    uint32_t in_y = cy + ch - TERM_INPUT_H;
    fb_fill_rect(cx, in_y, cw, TERM_INPUT_H, TERM_BG_COLOR);
    gfx_draw_hline(cx, in_y, cw, TERM_BORDER);

    /* Prompt (green) */
    font_draw_text(cx + 4, in_y + 1, TERM_PROMPT, TERM_GREEN);

    /* Input buffer (white) */
    uint32_t prompt_w = term_str_len(TERM_PROMPT) * FONT_WIDTH;
    font_draw_text(cx + 4 + prompt_w, in_y + 1,
                   term_state.input_buf, TERM_WHITE);

    /* Blinking cursor (block at end of input) */
    if (term_cursor_visible) {
        uint32_t cursor_x = cx + 4 + prompt_w +
                            term_state.input_len * FONT_WIDTH;
        fb_fill_rect(cursor_x, in_y + 1, FONT_WIDTH, FONT_HEIGHT - 3,
                     TERM_GREEN);
    }
    gui_fb_flush_rect(cx, cy, cw, ch);
    wm_invalidate_cursor_cache();
}

/* ============================================================
 * Terminal initialization
 * ============================================================ */

static void term_init_state(void) {
    term_state.line_count = 0;
    term_state.input_len = 0;
    term_state.input_buf[0] = '\0';
    term_state.cursor_pos = 0;
    term_str_copy(term_state.cwd, "~");

    /* Welcome banner */
    term_append_line("Kenux Terminal v1.0");
    term_append_line("Kernel: KenuxK 4.0 x86_64");
    term_append_line("Type 'help' for available commands.");
    term_append_line("");
}

/* ============================================================
 * Public API
 * ============================================================ */

window_t* terminal_create(void) {
    if (terminal_win) return terminal_win;

    terminal_win = window_create(100, 60, 500, 340, "Terminal");
    terminal_win->titlebar_color = RGB(0x1A, 0x1A, 0x1A);
    terminal_win->bg_color = TERM_BG_COLOR;
    terminal_win->visible = false;
    window_set_statusbar(terminal_win, "kenux@localhost:~$");

    term_init_state();

    wm_add_window(terminal_win);
    return terminal_win;
}

void terminal_on_show(void) {
    if (!terminal_win || !terminal_win->visible) return;
    term_draw_content();
}

void terminal_handle_key(uint16_t key_code, uint16_t key_char) {
    if (!terminal_win || !terminal_win->visible) return;
    if (terminal_win->state.minimized) return;

    /* Reset cursor blink on any key press */
    term_cursor_visible = 1;
    term_blink_counter = 0;

    if (key_code == 0x0D || key_code == 0x0A) {
        /* Enter — execute command */
        term_execute_command();
        term_draw_content();
    } else if (key_code == 0x08) {
        /* Backspace — remove last character */
        if (term_state.input_len > 0) {
            term_state.input_len--;
            term_state.input_buf[term_state.input_len] = '\0';
        }
        term_draw_content();
    } else if (key_char >= 32 && key_char <= 126) {
        /* Printable character — append to input buffer */
        if (term_state.input_len < TERM_LINE_WIDTH - 1) {
            term_state.input_buf[term_state.input_len] = (char)key_char;
            term_state.input_len++;
            term_state.input_buf[term_state.input_len] = '\0';
        }
        term_draw_content();
    }
}

void terminal_refresh(void) {
    if (!terminal_win || !terminal_win->visible) return;
    if (terminal_win->state.minimized) return;
    term_draw_content();
}

void terminal_tick(void) {
    if (!terminal_win || !terminal_win->visible) return;
    if (terminal_win->state.minimized) return;

    term_blink_counter++;
    if (term_blink_counter >= TERM_BLINK_TICKS) {
        term_blink_counter = 0;
        term_cursor_visible = !term_cursor_visible;
        term_draw_content();
    }
}
