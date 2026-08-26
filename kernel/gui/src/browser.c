/* ============================================================
 * browser.c - Kenux Browser Demo Application
 *
 * A demo web browser UI with navigation bar, address bar,
 * bookmark bar, and preset local pages (kenux:// URLs).
 * No real network connection; all pages are rendered locally.
 * ============================================================ */

#include "browser.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"

/* ---------- Constants ---------- */

#define BROWSER_HISTORY_MAX 16
#define BROWSER_URL_MAX     64

/* Layout (relative to content rect) */
#define BR_NAV_BAR_H        32
#define BR_BOOKMARK_H       22
#define BR_BOOKMARK_Y       34
#define BR_PAGE_Y           (BR_NAV_BAR_H + BR_BOOKMARK_H + 2)

/* Address bar geometry (relative to content rect) */
#define BR_ADDR_X           120
#define BR_ADDR_Y           6
#define BR_ADDR_W           360
#define BR_ADDR_H           20

/* Colors */
#define BR_NAV_BG           RGB(0x2B, 0x2B, 0x2B)
#define BR_BOOKMARK_BG      RGB(0x33, 0x33, 0x33)
#define BR_PAGE_BG          RGB(0xFF, 0xFF, 0xFF)
#define BR_LINK_COLOR       RGB(0x00, 0x78, 0xD4)
#define BR_TEXT_COLOR       RGB(0x20, 0x20, 0x20)
#define BR_TITLE_COLOR      RGB(0x1A, 0x1A, 0x1A)
#define BR_GRAY_TEXT        RGB(0x66, 0x66, 0x66)
#define BR_ADDR_BG          RGB(0x3C, 0x3C, 0x3C)
#define BR_ADDR_FG          RGB(0xE0, 0xE0, 0xE0)
#define BR_ADDR_BORDER      RGB(0x60, 0x60, 0x60)
#define BR_SEPARATOR        RGB(0x55, 0x55, 0x55)
#define BR_TABLE_HEADER     RGB(0xF0, 0xF0, 0xF0)
#define BR_TABLE_ALT        RGB(0xF8, 0xF8, 0xF8)

/* ---------- State ---------- */

typedef struct {
    char current_url[BROWSER_URL_MAX];
    char history[BROWSER_HISTORY_MAX][BROWSER_URL_MAX];
    int32_t history_pos;
    int32_t history_count;
    bool loading;
} browser_state_t;

static window_t* browser_win = NULL;
static browser_state_t br_state;

/* ---------- Forward declarations ---------- */

static void br_draw_content(void);

/* ---------- String helpers ---------- */

static void br_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void br_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static bool br_str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static uint32_t br_str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static void br_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ---------- Text drawing helpers ---------- */

/* Draw text in bold by rendering twice with 1px horizontal offset */
static void br_draw_bold(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    font_draw_text(x, y, str, color);
    font_draw_text(x + 1, y, str, color);
}

/* Draw a hyperlink: blue text with underline */
static void br_draw_link(uint32_t x, uint32_t y, const char* str) {
    font_draw_text(x, y, str, BR_LINK_COLOR);
    uint32_t w = font_text_width(str);
    gfx_draw_hline(x, y + FONT_HEIGHT - 1, w, BR_LINK_COLOR);
}

/* ---------- URL validation ---------- */

static bool br_is_valid_url(const char* url) {
    return br_str_equal(url, "kenux://home") ||
           br_str_equal(url, "kenux://about") ||
           br_str_equal(url, "kenux://docs") ||
           br_str_equal(url, "kenux://system") ||
           br_str_equal(url, "kenux://files");
}

/* ---------- History management ---------- */

static void br_push_history(const char* url) {
    /* Truncate forward history if we navigated back before */
    if (br_state.history_pos < br_state.history_count - 1) {
        br_state.history_count = br_state.history_pos + 1;
    }

    /* If history is full, shift everything left by one */
    if (br_state.history_count >= BROWSER_HISTORY_MAX) {
        for (int32_t i = 0; i < BROWSER_HISTORY_MAX - 1; i++) {
            br_str_copy(br_state.history[i], br_state.history[i + 1]);
        }
        br_state.history_count = BROWSER_HISTORY_MAX - 1;
    }

    /* Append the new URL */
    br_str_copy(br_state.history[br_state.history_count], url);
    br_state.history_count++;
    br_state.history_pos = br_state.history_count - 1;
}

/* ---------- Navigation ---------- */

void browser_navigate(const char* url) {
    if (!br_is_valid_url(url)) return;
    if (!browser_win) return;

    br_str_copy(br_state.current_url, url);
    br_push_history(url);
    br_state.loading = false;

    /* Update status bar */
    char status[64];
    br_str_copy(status, "Done  |  ");
    br_str_cat(status, url);
    window_set_statusbar(browser_win, status);

    if (browser_win->visible) {
        br_draw_content();
        wm_paint();
    }
}

static void br_go_back(void) {
    if (br_state.history_pos <= 0) return;

    br_state.history_pos--;
    br_str_copy(br_state.current_url, br_state.history[br_state.history_pos]);

    char status[64];
    br_str_copy(status, "Done  |  ");
    br_str_cat(status, br_state.current_url);
    window_set_statusbar(browser_win, status);

    if (browser_win && browser_win->visible) {
        br_draw_content();
        wm_paint();
    }
}

static void br_go_forward(void) {
    if (br_state.history_pos >= br_state.history_count - 1) return;

    br_state.history_pos++;
    br_str_copy(br_state.current_url, br_state.history[br_state.history_pos]);

    char status[64];
    br_str_copy(status, "Done  |  ");
    br_str_cat(status, br_state.current_url);
    window_set_statusbar(browser_win, status);

    if (browser_win && browser_win->visible) {
        br_draw_content();
        wm_paint();
    }
}

static void br_refresh(void) {
    if (browser_win && browser_win->visible) {
        char status[64];
        br_str_copy(status, "Done  |  ");
        br_str_cat(status, br_state.current_url);
        window_set_statusbar(browser_win, status);

        br_draw_content();
        wm_paint();
    }
}

static void br_go_home(void) {
    browser_navigate("kenux://home");
}

/* ---------- Widget callbacks ---------- */

static void br_back_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    br_go_back();
}

static void br_forward_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    br_go_forward();
}

static void br_refresh_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    br_refresh();
}

static void br_home_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    br_go_home();
}

static void br_bookmark_click(widget_t* wgt, void* user_data) {
    (void)wgt;
    if (user_data) {
        browser_navigate((const char*)user_data);
    }
}

/* ---------- Navigation bar drawing ---------- */

static void br_draw_nav_bar(uint32_t cx, uint32_t cy, uint32_t cw) {
    /* Dark background for the navigation bar */
    fb_fill_rect(cx, cy, cw, BR_NAV_BAR_H, BR_NAV_BG);

    /* Separator below nav bar */
    gfx_draw_hline(cx, cy + BR_NAV_BAR_H, cw, BR_SEPARATOR);

    /* Address bar background */
    fb_fill_rect(cx + BR_ADDR_X, cy + BR_ADDR_Y,
                 BR_ADDR_W, BR_ADDR_H, BR_ADDR_BG);
    gfx_draw_rect(cx + BR_ADDR_X, cy + BR_ADDR_Y,
                  BR_ADDR_W, BR_ADDR_H, BR_ADDR_BORDER);

    /* Address bar text (current URL) */
    font_draw_text(cx + BR_ADDR_X + 6, cy + BR_ADDR_Y + 3,
                   br_state.current_url, BR_ADDR_FG);

    /* Loading indicator */
    if (br_state.loading) {
        font_draw_text(cx + BR_ADDR_X + BR_ADDR_W - 50,
                       cy + BR_ADDR_Y + 3, "Loading...",
                       RGB(0xFF, 0xD7, 0x00));
    }
}

/* ---------- Bookmark bar drawing ---------- */

static void br_draw_bookmark_bar(uint32_t cx, uint32_t cy, uint32_t cw) {
    uint32_t by = cy + BR_BOOKMARK_Y;

    /* Bookmark bar background */
    fb_fill_rect(cx, by, cw, BR_BOOKMARK_H, BR_BOOKMARK_BG);
    gfx_draw_hline(cx, by + BR_BOOKMARK_H, cw, BR_SEPARATOR);

    /* Draw bookmark labels (the widget buttons sit on top) */
    const char* labels[] = { "Home", "About", "Docs", "System", "Files" };
    uint32_t count = 5;
    uint32_t btn_w = 110;
    uint32_t gap = 4;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t bx = cx + gap + i * (btn_w + gap);
        if (bx + btn_w > cx + cw) break;

        /* Highlight active bookmark */
        const char* urls[] = {
            "kenux://home", "kenux://about", "kenux://docs",
            "kenux://system", "kenux://files"
        };
        if (br_str_equal(br_state.current_url, urls[i])) {
            fb_fill_rect(bx, by + 1, btn_w, BR_BOOKMARK_H - 2,
                         RGB(0x00, 0x78, 0xD4));
            font_draw_text(bx + 6, by + 3, labels[i],
                           RGB(0xFF, 0xFF, 0xFF));
        } else {
            font_draw_text(bx + 6, by + 3, labels[i],
                           RGB(0xCC, 0xCC, 0xCC));
        }
    }
}

/* ---------- Page content drawing ---------- */

static void br_draw_page_home(uint32_t px, uint32_t py, uint32_t pw, uint32_t ph) {
    (void)ph;
    uint32_t y = py + 16;
    uint32_t x = px + 20;
    uint32_t max_x = px + pw - 20;

    /* Title (bold/large effect) */
    br_draw_bold(x, y, "Welcome to Kenux", BR_TITLE_COLOR);
    y += 24;

    /* Subtitle */
    font_draw_text(x, y, "Kenux Browser Demo v1.0", BR_GRAY_TEXT);
    y += 24;

    /* Separator */
    gfx_draw_hline(x, y, max_x - x, RGB(0xDD, 0xDD, 0xDD));
    y += 12;

    /* Section heading */
    br_draw_bold(x, y, "Quick Links", BR_TEXT_COLOR);
    y += 22;

    /* Link list */
    const char* links[] = {
        "kenux://about",
        "kenux://docs",
        "kenux://system",
        "kenux://files"
    };
    const char* descs[] = {
        "About Kenux - System and OS information",
        "API Documentation - KAPI function reference",
        "System Status - CPU and memory usage",
        "File Browser - Virtual filesystem listing"
    };
    uint32_t link_count = 4;
    for (uint32_t i = 0; i < link_count; i++) {
        if (y + 16 > py + ph - 8) break;
        br_draw_link(x, y, links[i]);
        font_draw_text(x + 160, y, descs[i], BR_GRAY_TEXT);
        y += 20;
    }

    y += 12;
    gfx_draw_hline(x, y, max_x - x, RGB(0xDD, 0xDD, 0xDD));
    y += 12;

    /* Help text */
    font_draw_text(x, y,
                   "Use the bookmark bar or navigation buttons to explore.",
                   BR_GRAY_TEXT);
    y += 16;
    font_draw_text(x, y,
                   "Back / Forward buttons navigate history.",
                   BR_GRAY_TEXT);
}

static void br_draw_page_about(uint32_t px, uint32_t py, uint32_t pw, uint32_t ph) {
    (void)ph;
    uint32_t y = py + 16;
    uint32_t x = px + 20;

    /* Title */
    br_draw_bold(x, y, "About Kenux", BR_TITLE_COLOR);
    y += 28;

    /* OS Version via KAPI */
    font_draw_text(x, y, "OS Version:", BR_GRAY_TEXT);
    {
        char buf[64];
        KAPI_System_GetOSVersion(buf, sizeof(buf));
        font_draw_text(x + 100, y, buf, BR_TEXT_COLOR);
    }
    y += 20;

    /* Kernel version */
    font_draw_text(x, y, "Kernel:", BR_GRAY_TEXT);
    font_draw_text(x + 100, y, "KenuxK 4.0 x86_64", BR_TEXT_COLOR);
    y += 20;

    /* Browser version */
    font_draw_text(x, y, "Browser:", BR_GRAY_TEXT);
    font_draw_text(x + 100, y, "Kenux Browser 1.0", BR_TEXT_COLOR);
    y += 20;

    /* CPU model via KAPI */
    font_draw_text(x, y, "CPU:", BR_GRAY_TEXT);
    {
        char buf[64];
        KAPI_System_GetCPUModel(buf, sizeof(buf));
        font_draw_text(x + 100, y, buf, BR_TEXT_COLOR);
    }
    y += 20;

    /* Display */
    font_draw_text(x, y, "Display:", BR_GRAY_TEXT);
    {
        char buf[32];
        br_str_copy(buf, "");
        br_int_to_str(fb.width, buf);
        br_str_cat(buf, "x");
        char h[8];
        br_int_to_str(fb.height, h);
        br_str_cat(buf, h);
        br_str_cat(buf, " GOP");
        font_draw_text(x + 100, y, buf, BR_TEXT_COLOR);
    }
    y += 28;

    /* Separator */
    gfx_draw_hline(x, y, pw - 40, RGB(0xDD, 0xDD, 0xDD));
    y += 12;

    /* Description */
    br_draw_bold(x, y, "Overview", BR_TEXT_COLOR);
    y += 20;
    font_draw_text(x, y,
                   "Kenux is a 64-bit kernel OS with GUI rendered",
                   BR_TEXT_COLOR);
    y += 16;
    font_draw_text(x, y,
                   "directly in the kernel via UEFI GOP framebuffer.",
                   BR_TEXT_COLOR);
    y += 24;

    /* Features */
    br_draw_bold(x, y, "Features:", BR_TEXT_COLOR);
    y += 20;
    const char* features[] = {
        "- x86_64 long mode with 4-level paging",
        "- GUI with window manager and widgets",
        "- TCP/IP network stack",
        "- Ext2/Ext4 filesystem support",
        "- PS/2 keyboard and mouse drivers",
        "- 12 themes, 400+ colors, HSL support",
        "- KAL abstraction layer (46 modules)"
    };
    uint32_t feat_count = 7;
    for (uint32_t i = 0; i < feat_count; i++) {
        if (y + 16 > py + ph - 8) break;
        font_draw_text(x + 4, y, features[i], BR_GRAY_TEXT);
        y += 16;
    }
}

static void br_draw_page_docs(uint32_t px, uint32_t py, uint32_t pw, uint32_t ph) {
    (void)ph;
    uint32_t y = py + 16;
    uint32_t x = px + 20;

    /* Title */
    br_draw_bold(x, y, "API Documentation", BR_TITLE_COLOR);
    y += 24;
    font_draw_text(x, y, "KAPI Function Reference", BR_GRAY_TEXT);
    y += 24;

    /* Separator */
    gfx_draw_hline(x, y, pw - 40, RGB(0xDD, 0xDD, 0xDD));
    y += 12;

    /* API entries */
    struct {
        const char* sig;
        const char* desc;
    } apis[] = {
        { "KAPI_System_GetCPUUsage()", "Returns CPU usage (0-100%)" },
        { "KAPI_System_GetMemUsage()", "Returns memory usage (0-100%)" },
        { "KAPI_System_GetMemTotal()", "Returns total memory in KB" },
        { "KAPI_System_GetMemFree()",  "Returns free memory in KB" },
        { "KAPI_System_GetOSVersion()", "Returns OS version string" },
        { "KAPI_System_GetCPUModel()",  "Returns CPU model string" },
        { "KAPI_System_Uptime()",       "Returns uptime in seconds" },
        { "KAPI_Process_GetCount()",    "Returns process count" },
        { "KAPI_Process_GetList()",     "Returns list of processes" },
        { "KAPI_VFS_GetFileIcon()",     "Returns file icon for path" }
    };
    uint32_t api_count = 10;
    for (uint32_t i = 0; i < api_count; i++) {
        if (y + 32 > py + ph - 8) break;

        /* Function signature (monospace look via bold) */
        br_draw_bold(x, y, apis[i].sig, BR_LINK_COLOR);
        y += 16;
        /* Description */
        font_draw_text(x + 12, y, apis[i].desc, BR_GRAY_TEXT);
        y += 20;
    }
}

static void br_draw_page_system(uint32_t px, uint32_t py, uint32_t pw, uint32_t ph) {
    (void)ph;
    uint32_t y = py + 16;
    uint32_t x = px + 20;
    uint32_t bar_w = pw - 120;

    /* Title */
    br_draw_bold(x, y, "System Status", BR_TITLE_COLOR);
    y += 28;

    /* CPU Usage */
    {
        uint32_t cpu = KAPI_System_GetCPUUsage();
        char buf[16];
        br_str_copy(buf, "CPU Usage: ");
        char num[8];
        br_int_to_str(cpu, num);
        br_str_cat(buf, num);
        br_str_cat(buf, "%");
        font_draw_text(x, y, buf, BR_TEXT_COLOR);
        y += 18;

        /* Usage bar */
        fb_fill_rect(x, y, bar_w, 10, RGB(0xE0, 0xE0, 0xE0));
        uint32_t fill = (bar_w * cpu) / 100;
        uint32_t bar_color = RGB(0x00, 0xCC, 0x66);
        if (cpu > 75) bar_color = RGB(0xFF, 0x6B, 0x6B);
        else if (cpu > 50) bar_color = RGB(0xFF, 0xD7, 0x00);
        if (fill > 0) {
            fb_fill_rect(x, y, fill, 10, bar_color);
        }
        y += 22;
    }

    /* Memory Usage */
    {
        uint32_t mem = KAPI_System_GetMemUsage();
        char buf[16];
        br_str_copy(buf, "Mem Usage: ");
        char num[8];
        br_int_to_str(mem, num);
        br_str_cat(buf, num);
        br_str_cat(buf, "%");
        font_draw_text(x, y, buf, BR_TEXT_COLOR);
        y += 18;

        /* Usage bar */
        fb_fill_rect(x, y, bar_w, 10, RGB(0xE0, 0xE0, 0xE0));
        uint32_t fill = (bar_w * mem) / 100;
        uint32_t bar_color = RGB(0x00, 0x78, 0xD4);
        if (mem > 75) bar_color = RGB(0xFF, 0x6B, 0x6B);
        else if (mem > 50) bar_color = RGB(0xFF, 0xD7, 0x00);
        if (fill > 0) {
            fb_fill_rect(x, y, fill, 10, bar_color);
        }
        y += 22;
    }

    /* Separator */
    gfx_draw_hline(x, y, pw - 40, RGB(0xDD, 0xDD, 0xDD));
    y += 12;

    /* Memory details */
    {
        uint32_t total_kb = KAPI_System_GetMemTotal();
        uint32_t free_kb = KAPI_System_GetMemFree();
        uint32_t used_kb = total_kb - free_kb;

        font_draw_text(x, y, "Memory Total:", BR_GRAY_TEXT);
        char buf[32];
        br_int_to_str(total_kb / 1024, buf);
        br_str_cat(buf, " MB");
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
        y += 18;

        font_draw_text(x, y, "Memory Used:", BR_GRAY_TEXT);
        br_str_copy(buf, "");
        br_int_to_str(used_kb / 1024, buf);
        br_str_cat(buf, " MB");
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
        y += 18;

        font_draw_text(x, y, "Memory Free:", BR_GRAY_TEXT);
        br_str_copy(buf, "");
        br_int_to_str(free_kb / 1024, buf);
        br_str_cat(buf, " MB");
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
        y += 24;
    }

    /* Uptime */
    {
        uint32_t up = KAPI_System_Uptime();
        font_draw_text(x, y, "Uptime:", BR_GRAY_TEXT);
        char buf[32];
        br_int_to_str(up, buf);
        br_str_cat(buf, " seconds");
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
        y += 24;
    }

    /* Process count */
    {
        uint32_t proc_count = KAPI_Process_GetCount();
        font_draw_text(x, y, "Processes:", BR_GRAY_TEXT);
        char buf[16];
        br_int_to_str(proc_count, buf);
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
        y += 24;
    }

    /* Disk usage */
    {
        uint32_t disk = KAPI_System_GetDiskUsage();
        font_draw_text(x, y, "Disk Usage:", BR_GRAY_TEXT);
        char buf[16];
        br_int_to_str(disk, buf);
        br_str_cat(buf, "%");
        font_draw_text(x + 120, y, buf, BR_TEXT_COLOR);
    }
}

static void br_draw_page_files(uint32_t px, uint32_t py, uint32_t pw, uint32_t ph) {
    (void)ph;
    uint32_t y = py + 16;
    uint32_t x = px + 20;

    /* Title */
    br_draw_bold(x, y, "File Browser", BR_TITLE_COLOR);
    y += 24;
    font_draw_text(x, y, "Virtual filesystem root (/)", BR_GRAY_TEXT);
    y += 24;

    /* Table header */
    fb_fill_rect(px + 16, y, pw - 32, 18, BR_TABLE_HEADER);
    gfx_draw_rect(px + 16, y, pw - 32, 18, RGB(0xCC, 0xCC, 0xCC));
    font_draw_text(x, y + 2, "Name", BR_GRAY_TEXT);
    font_draw_text(x + 200, y + 2, "Type", BR_GRAY_TEXT);
    font_draw_text(x + 300, y + 2, "Size", BR_GRAY_TEXT);
    y += 20;

    /* File entries */
    struct {
        const char* name;
        const char* type;
        const char* size;
    } entries[] = {
        { "/kernel",   "Directory", "---" },
        { "/gui",      "Directory", "---" },
        { "/drivers",  "Directory", "---" },
        { "/lib",      "Directory", "---" },
        { "/apps",     "Directory", "---" },
        { "/README.txt","File",      "2 KB" },
        { "/Makefile", "File",      "4 KB" },
        { "/kernel.c", "File",      "16 KB" },
        { "/linker.ld","File",      "1 KB" },
        { "/config.h", "File",      "2 KB" }
    };
    uint32_t entry_count = 10;
    for (uint32_t i = 0; i < entry_count; i++) {
        if (y + 16 > py + ph - 8) break;

        /* Alternating row background */
        if (i % 2 == 0) {
            fb_fill_rect(px + 16, y, pw - 32, 16, BR_PAGE_BG);
        } else {
            fb_fill_rect(px + 16, y, pw - 32, 16, BR_TABLE_ALT);
        }

        /* Folder vs file color */
        uint32_t name_color = BR_LINK_COLOR;
        if (br_str_equal(entries[i].type, "File")) {
            name_color = BR_TEXT_COLOR;
        }

        font_draw_text(x, y + 1, entries[i].name, name_color);
        font_draw_text(x + 200, y + 1, entries[i].type, BR_GRAY_TEXT);
        font_draw_text(x + 300, y + 1, entries[i].size, BR_GRAY_TEXT);
        y += 16;
    }
}

/* ---------- Main content draw ---------- */

static void br_draw_content(void) {
    if (!browser_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(browser_win, &cx, &cy, &cw, &ch);

    /* Navigation bar */
    br_draw_nav_bar(cx, cy, cw);

    /* Bookmark bar */
    br_draw_bookmark_bar(cx, cy, cw);

    /* Page content area */
    uint32_t px = cx;
    uint32_t py = cy + BR_PAGE_Y;
    uint32_t pw = cw;
    uint32_t ph = ch - BR_PAGE_Y;

    /* White page background */
    fb_fill_rect(px, py, pw, ph, BR_PAGE_BG);

    /* Render the current page */
    if (br_str_equal(br_state.current_url, "kenux://home")) {
        br_draw_page_home(px, py, pw, ph);
    } else if (br_str_equal(br_state.current_url, "kenux://about")) {
        br_draw_page_about(px, py, pw, ph);
    } else if (br_str_equal(br_state.current_url, "kenux://docs")) {
        br_draw_page_docs(px, py, pw, ph);
    } else if (br_str_equal(br_state.current_url, "kenux://system")) {
        br_draw_page_system(px, py, pw, ph);
    } else if (br_str_equal(br_state.current_url, "kenux://files")) {
        br_draw_page_files(px, py, pw, ph);
    } else {
        /* Unknown URL: show error page */
        font_draw_text(px + 20, py + 20, "Page not found", BR_TITLE_COLOR);
        font_draw_text(px + 20, py + 40, br_state.current_url, BR_GRAY_TEXT);
    }
}

/* ---------- Public API ---------- */

window_t* browser_create(void) {
    if (browser_win) return browser_win;

    /* Initialize state */
    br_state.history_pos = -1;
    br_state.history_count = 0;
    br_state.loading = false;
    br_str_copy(br_state.current_url, "kenux://home");

    /* Create window 600x400 */
    browser_win = window_create(160, 40, 600, 400, "Kenux Browser");
    browser_win->titlebar_color = msf_settings.titlebar_active;
    browser_win->visible = false;
    window_set_statusbar(browser_win, "Ready");

    /* Navigation buttons (positioned relative to content area) */
    widget_t* btn_back = widget_create_button(4, 4, 50, 24,
        "Back", br_back_click, NULL);
    window_add_widget(browser_win, btn_back);

    widget_t* btn_fwd = widget_create_button(58, 4, 60, 24,
        "Forward", br_forward_click, NULL);
    window_add_widget(browser_win, btn_fwd);

    widget_t* btn_refresh = widget_create_button(486, 4, 56, 24,
        "Refresh", br_refresh_click, NULL);
    window_add_widget(browser_win, btn_refresh);

    widget_t* btn_home = widget_create_button(546, 4, 48, 24,
        "Home", br_home_click, NULL);
    window_add_widget(browser_win, btn_home);

    /* Bookmark bar buttons (quick links to preset pages) */
    widget_t* bm_home = widget_create_button(4, BR_BOOKMARK_Y, 110, 20,
        "Home", br_bookmark_click, (void*)"kenux://home");
    window_add_widget(browser_win, bm_home);

    widget_t* bm_about = widget_create_button(118, BR_BOOKMARK_Y, 110, 20,
        "About", br_bookmark_click, (void*)"kenux://about");
    window_add_widget(browser_win, bm_about);

    widget_t* bm_docs = widget_create_button(232, BR_BOOKMARK_Y, 110, 20,
        "Docs", br_bookmark_click, (void*)"kenux://docs");
    window_add_widget(browser_win, bm_docs);

    widget_t* bm_system = widget_create_button(346, BR_BOOKMARK_Y, 110, 20,
        "System", br_bookmark_click, (void*)"kenux://system");
    window_add_widget(browser_win, bm_system);

    widget_t* bm_files = widget_create_button(460, BR_BOOKMARK_Y, 110, 20,
        "Files", br_bookmark_click, (void*)"kenux://files");
    window_add_widget(browser_win, bm_files);

    wm_add_window(browser_win);

    /* Push initial page to history */
    br_push_history("kenux://home");

    return browser_win;
}

void browser_on_show(void) {
    if (browser_win && browser_win->visible) {
        /* Update status bar */
        char status[64];
        br_str_copy(status, "Done  |  ");
        br_str_cat(status, br_state.current_url);
        window_set_statusbar(browser_win, status);

        br_draw_content();
    }
}
