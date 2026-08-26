/* ============================================================
 * filemgr.c - File Explorer Application
 *
 * Provides a file manager with sidebar navigation, address bar
 * with back/forward/up buttons, file/folder listing using the
 * kernel VFS API, and a status bar showing item count and size.
 *
 * Uses real VFS when available (vfs_root), with a simulated
 * fallback filesystem for environments where VFS is not
 * initialised.
 * ============================================================ */

#include "filemgr.h"
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

/* ---- Layout constants ---- */
#define FM_SIDEBAR_W      160
#define FM_MENU_BAR_H      32
#define FM_ADDR_BAR_H      44
#define FM_BTN_H           34
#define FM_BTN_W           92
#define FM_MAX_ENTRIES     32
#define FM_MAX_PATH        128
#define FM_HISTORY_SIZE    16
#define FM_NAME_LEN        32
#define FM_ROW_H           30

/* ---- File entry ---- */
typedef struct {
    char     name[FM_NAME_LEN];
    int      is_dir;
    uint32_t size;
} fm_entry_t;

/* ---- Sidebar quick-access item ---- */
typedef struct {
    const char* name;
    icon_id_t   icon;
    const char* path;
} fm_sidebar_item_t;

/* ---- Application state ---- */
static window_t* filemgr_win = NULL;
static char      fm_cwd[FM_MAX_PATH] = "/";
static fm_entry_t fm_entries[FM_MAX_ENTRIES];
static uint32_t   fm_entry_count = 0;
static int32_t    fm_selected = -1;

/* Navigation history (for back/forward) */
static char   fm_history[FM_HISTORY_SIZE][FM_MAX_PATH];
static int32_t fm_hist_pos = 0;    /* current position in history */
static int32_t fm_hist_count = 0;  /* total entries in history   */

/* Sidebar items */
static fm_sidebar_item_t fm_sidebar[] = {
    { "Home",      ICON_HOME,     "/" },
    { "System",    ICON_SETTINGS, "/System" },
    { "Documents", ICON_FOLDER,   "/Documents" },
    { "Network",   ICON_NETWORK,  "/Network" },
};
#define FM_SIDEBAR_COUNT (sizeof(fm_sidebar) / sizeof(fm_sidebar[0]))

/* ============================================================
 * String helper functions (freestanding, no stdlib)
 * ============================================================ */

static void fm_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void fm_str_copy_n(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void fm_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static uint32_t fm_str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static int fm_str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void fm_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* Format byte count to human-readable string */
static void fm_format_size(uint32_t bytes, char* buf) {
    if (bytes == 0) {
        buf[0] = '-';
        buf[1] = '\0';
        return;
    }
    if (bytes >= 1048576) {
        uint32_t mb = bytes / 1048576;
        uint32_t mb_frac = (bytes % 1048576) * 10 / 1048576;
        fm_int_to_str(mb, buf);
        char frac[4];
        frac[0] = '.';
        frac[1] = '0' + mb_frac;
        frac[2] = '\0';
        fm_str_cat(buf, frac);
        fm_str_cat(buf, " MB");
    } else if (bytes >= 1024) {
        uint32_t kb = bytes / 1024;
        fm_int_to_str(kb, buf);
        fm_str_cat(buf, " KB");
    } else {
        fm_int_to_str(bytes, buf);
        fm_str_cat(buf, " B");
    }
}

/* ============================================================
 * VFS integration
 * ============================================================ */

/* Navigate the real VFS tree from vfs_root following the given
 * slash-separated path (e.g. "/System/bin"). Returns NULL if
 * the path cannot be resolved or VFS is not initialised. */
static vfs_node_t* fm_vfs_lookup(const char* path) {
    if (!vfs_root || !path) return NULL;

    /* Empty or root path */
    if (path[0] == '/' && path[1] == '\0') return vfs_root;
    if (path[0] != '/') return NULL;

    vfs_node_t* node = vfs_root;
    uint32_t i = 1;  /* skip leading '/' */

    while (path[i] && node) {
        /* Extract one path component */
        char comp[FS_MAX_NAME];
        uint32_t ci = 0;
        while (path[i] && path[i] != '/' && ci < FS_MAX_NAME - 1) {
            comp[ci++] = path[i++];
        }
        comp[ci] = '\0';
        if (path[i] == '/') i++;

        if (ci == 0) continue;

        /* Use finddir to look up the child */
        if (node->finddir) {
            node = node->finddir(node, comp);
        } else {
            /* Fallback: traverse children linked list */
            vfs_node_t* child = node->children;
            node = NULL;
            while (child) {
                if (fm_str_cmp(child->name, comp) == 0) {
                    node = child;
                    break;
                }
                child = child->next;
            }
        }
    }

    return node;
}

/* Load directory entries from the real VFS. Returns 1 on
 * success (at least one entry found), 0 on failure. */
static int fm_load_vfs_entries(const char* path) {
    if (!vfs_root) return 0;

    vfs_node_t* dir = fm_vfs_lookup(path);
    if (!dir) return 0;
    if (dir->type != FS_TYPE_DIRECTORY) return 0;

    fm_entry_count = 0;

    /* Traverse children linked list */
    vfs_node_t* child = dir->children;
    while (child && fm_entry_count < FM_MAX_ENTRIES) {
        fm_str_copy_n(fm_entries[fm_entry_count].name, child->name, FM_NAME_LEN);
        fm_entries[fm_entry_count].is_dir = (child->type == FS_TYPE_DIRECTORY) ? 1 : 0;
        fm_entries[fm_entry_count].size = (uint32_t)child->size;
        fm_entry_count++;
        child = child->next;
    }

    /* If children list is empty, try readdir */
    if (fm_entry_count == 0 && dir->readdir) {
        int idx = 0;
        char name[FS_MAX_NAME];
        while (fm_entry_count < FM_MAX_ENTRIES) {
            int ret = dir->readdir(dir, idx, name, FS_MAX_NAME);
            if (ret != 0) break;
            fm_str_copy_n(fm_entries[fm_entry_count].name, name, FM_NAME_LEN);
            fm_entries[fm_entry_count].is_dir = 0;
            fm_entries[fm_entry_count].size = 0;
            fm_entry_count++;
            idx++;
        }
    }

    return (fm_entry_count > 0) ? 1 : 0;
}

static void fm_load_entries(const char* path) {
    fm_selected = -1;
    if (!fm_load_vfs_entries(path)) fm_entry_count = 0;
}

/* ============================================================
 * Navigation
 * ============================================================ */

/* Push the current path onto the history stack. */
static void fm_history_push(const char* path) {
    if (fm_hist_pos >= FM_HISTORY_SIZE - 1) {
        /* Shift history left to make room */
        for (int32_t i = 0; i < FM_HISTORY_SIZE - 1; i++) {
            fm_str_copy(fm_history[i], fm_history[i + 1]);
        }
        fm_hist_pos = FM_HISTORY_SIZE - 1;
    } else {
        fm_hist_pos++;
    }
    fm_str_copy_n(fm_history[fm_hist_pos], path, FM_MAX_PATH);
    fm_hist_count = fm_hist_pos + 1;
}

/* Navigate to an absolute path, recording history. */
static void fm_navigate_to(const char* path) {
    fm_history_push(fm_cwd);
    fm_str_copy_n(fm_cwd, path, FM_MAX_PATH);
    fm_load_entries(fm_cwd);

    /* Update status bar */
    char status[64];
    fm_str_copy(status, "");
    fm_int_to_str(fm_entry_count, status);
    fm_str_cat(status, " items");
    window_set_statusbar(filemgr_win, status);
}

/* Navigate back in history. */
static void fm_navigate_back(void) {
    if (fm_hist_pos <= 0) return;
    fm_hist_pos--;
    fm_str_copy_n(fm_cwd, fm_history[fm_hist_pos], FM_MAX_PATH);
    fm_load_entries(fm_cwd);
}

/* Navigate forward in history. */
static void fm_navigate_forward(void) {
    if (fm_hist_pos >= fm_hist_count - 1) return;
    fm_hist_pos++;
    fm_str_copy_n(fm_cwd, fm_history[fm_hist_pos], FM_MAX_PATH);
    fm_load_entries(fm_cwd);
}

/* Navigate to the parent directory. */
static void fm_navigate_up(void) {
    /* Already at root */
    if (fm_str_cmp(fm_cwd, "/") == 0) return;

    /* Find last '/' and truncate */
    uint32_t len = fm_str_len(fm_cwd);
    /* Remove trailing '/' if present (but keep root) */
    while (len > 1 && fm_cwd[len - 1] == '/') {
        fm_cwd[--len] = '\0';
    }
    /* Find the last '/' */
    while (len > 1 && fm_cwd[len - 1] != '/') {
        len--;
    }
    if (len <= 1) {
        fm_cwd[0] = '/';
        fm_cwd[1] = '\0';
    } else {
        fm_cwd[len - 1] = '\0';  /* truncate at the '/' */
        if (fm_cwd[0] == '\0') {
            fm_cwd[0] = '/';
            fm_cwd[1] = '\0';
        }
    }

    fm_history_push(fm_cwd);
    fm_load_entries(fm_cwd);
}

/* ============================================================
 * Content drawing
 * ============================================================ */

static void fm_draw_content(void) {
    if (!filemgr_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(filemgr_win, &cx, &cy, &cw, &ch);

    /* Kenux Explorer: simple menu bar, toolbar, tree pane and details list. */
    fb_fill_rect(cx, cy, cw, ch, RGB(0xF7, 0xFA, 0xFF));

    uint32_t menu_y = cy;
    fb_fill_rect(cx, menu_y, cw, FM_MENU_BAR_H, RGB(0xF7, 0xFA, 0xFF));
    gfx_draw_hline(cx, menu_y + FM_MENU_BAR_H - 1, cw, RGB(0xD7, 0xE1, 0xF0));
    font_draw_text(cx + 12,  menu_y + 6, "File",    RGB(0x17, 0x20, 0x33));
    font_draw_text(cx + 76, menu_y + 6, "View",    RGB(0x17, 0x20, 0x33));
    font_draw_text(cx + 140, menu_y + 6, "Edit",   RGB(0x17, 0x20, 0x33));
    font_draw_text(cx + 204, menu_y + 6, "Tools", RGB(0x17, 0x20, 0x33));

    /* ---- Toolbar / address bar ---- */
    uint32_t addr_y = cy + FM_MENU_BAR_H;
    fb_fill_rect(cx, addr_y, cw, FM_ADDR_BAR_H, RGB(0xEA, 0xF0, 0xFA));
    gfx_draw_hline(cx, addr_y + FM_ADDR_BAR_H, cw, RGB(0xD7, 0xE1, 0xF0));
    fb_fill_rounded_rect(cx + 10, addr_y + 6, 54, 32, 16, RGB(0xF8, 0xFA, 0xFF));
    font_draw_text(cx + 24, addr_y + 10, "Up", RGB(0x17, 0x20, 0x33));
    fb_fill_rounded_rect(cx + 72, addr_y + 6, 76, 32, 16, RGB(0xF8, 0xFA, 0xFF));
    font_draw_text(cx + 88, addr_y + 10, "Open", RGB(0x17, 0x20, 0x33));
    fb_fill_rounded_rect(cx + 158, addr_y + 6, 104, 32, 16, RGB(0x25, 0x8D, 0xFF));
    font_draw_text(cx + 176, addr_y + 10, "Refresh", RGB(0xFF, 0xFF, 0xFF));

    /* Path display (centre of address bar) */
    uint32_t path_y = addr_y + 10;
    uint32_t path_x = cx + 276;
    uint32_t path_w = cw > 292 ? cw - 292 : 72;
    fb_fill_rounded_rect(path_x, addr_y + 6, path_w, 32, 16, RGB(0xFF, 0xFF, 0xFF));
    font_draw_text(path_x + 12, path_y, fm_cwd, RGB(0x17, 0x20, 0x33));

    /* ---- Sidebar ---- */
    uint32_t sb_y = addr_y + FM_ADDR_BAR_H;
    uint32_t sb_h = ch - FM_ADDR_BAR_H - FM_BTN_H - 4;
    if (sb_h > FM_MENU_BAR_H) sb_h -= FM_MENU_BAR_H;
    fb_fill_rounded_rect(cx + 8, sb_y + 8, FM_SIDEBAR_W - 16, sb_h - 16, 18, RGB(0xFF, 0xFF, 0xFF));
    gfx_draw_vline(cx + FM_SIDEBAR_W, sb_y, sb_h, RGB(0xD7, 0xE1, 0xF0));

    /* Sidebar header */
    font_draw_text(cx + 18, sb_y + 18, "Folders", RGB(0x17, 0x20, 0x33));

    /* Sidebar items */
    for (uint32_t i = 0; i < FM_SIDEBAR_COUNT; i++) {
        uint32_t iy = sb_y + 54 + i * 36;
        if (iy + 32 > sb_y + sb_h) break;

        int is_current = (fm_str_cmp(fm_sidebar[i].path, fm_cwd) == 0);

        if (is_current) {
            fb_fill_rounded_rect(cx + 14, iy, FM_SIDEBAR_W - 28, 32, 16, RGB(0x25, 0x8D, 0xFF));
        }

        /* Icon */
        KENUX_Render_DrawIcon(cx + 20, iy + 6, fm_sidebar[i].icon, 22);

        /* Label */
        uint32_t text_color = is_current ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x17, 0x20, 0x33);
        font_draw_text(cx + 52, iy + 5, fm_sidebar[i].name, text_color);
    }

    /* ---- Main file list area ---- */
    uint32_t mx = cx + FM_SIDEBAR_W + 4;
    uint32_t mw = cw - FM_SIDEBAR_W - 8;
    uint32_t my = sb_y;
    uint32_t mh = sb_h;

    /* Column header */
    fb_fill_rounded_rect(mx, my + 8, mw, mh - 12, 18, RGB(0xFF, 0xFF, 0xFF));
    fb_fill_rect(mx + 1, my + 8, mw - 2, 34, RGB(0xEA, 0xF0, 0xFA));
    font_draw_text(mx + 10, my + 14, "Type", RGB(0x17, 0x20, 0x33));
    font_draw_text(mx + 88, my + 14, "Name", RGB(0x17, 0x20, 0x33));
    font_draw_text(mx + mw - 108, my + 14, "Size", RGB(0x17, 0x20, 0x33));

    /* File entries */
    uint32_t list_y = my + 42;
    for (uint32_t i = 0; i < fm_entry_count; i++) {
        uint32_t ry = list_y + i * FM_ROW_H;
        if (ry + FM_ROW_H > my + mh) break;

        /* Row background (alternating) */
        uint32_t row_bg;
        if ((int32_t)i == fm_selected) {
            row_bg = RGB(0x25, 0x8D, 0xFF);
        } else if (i % 2 == 0) {
            row_bg = RGB(0xFF, 0xFF, 0xFF);
        } else {
            row_bg = RGB(0xF4, 0xF8, 0xFF);
        }
        fb_fill_rect(mx + 1, ry, mw - 2, FM_ROW_H, row_bg);

        /* Icon: folder or file */
        icon_id_t entry_icon = fm_entries[i].is_dir ? ICON_FOLDER : ICON_FILE;
        KENUX_Render_DrawIcon(mx + 10, ry + 4, entry_icon, 22);

        /* Name */
        uint32_t name_color = ((int32_t)i == fm_selected) ? RGB(0xFF, 0xFF, 0xFF) :
            (fm_entries[i].is_dir ? RGB(0x0A, 0x4B, 0x9A) : RGB(0x17, 0x20, 0x33));
        font_draw_text(mx + 42, ry + 4, fm_entries[i].is_dir ? "DIR" : "APP", name_color);
        font_draw_text(mx + 88, ry + 4, fm_entries[i].name, name_color);

        /* Size (files only) */
        if (!fm_entries[i].is_dir) {
            char size_str[24];
            fm_format_size(fm_entries[i].size, size_str);
            font_draw_text(mx + mw - 108, ry + 4, size_str, ((int32_t)i == fm_selected) ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x50, 0x50, 0x50));
        } else {
            font_draw_text(mx + mw - 108, ry + 4, "<DIR>", ((int32_t)i == fm_selected) ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x70, 0x70, 0x70));
        }
    }

    /* If no entries, show a placeholder message */
    if (fm_entry_count == 0) {
        font_draw_text(mx + 8, list_y + 4, "This folder is empty",
                       RGB(0x70, 0x70, 0x70));
    }

    /* ---- Button row separator (buttons are painted by widgets) ---- */
    uint32_t btn_row_y = cy + ch - FM_BTN_H - 4;
    gfx_draw_hline(cx, btn_row_y, cw, RGB(0xB8, 0xB8, 0xB8));

    /* ---- Status bar text ---- */
    {
        uint32_t total_size = 0;
        uint32_t file_count = 0;
        uint32_t dir_count = 0;
        for (uint32_t i = 0; i < fm_entry_count; i++) {
            if (fm_entries[i].is_dir) {
                dir_count++;
            } else {
                file_count++;
                total_size += fm_entries[i].size;
            }
        }
        char num[16];
        char buf[80];
        char size_str[24];
        fm_str_copy(buf, "");
        fm_int_to_str(fm_entry_count, num);
        fm_str_cat(buf, num);
        fm_str_cat(buf, " items | ");
        fm_int_to_str(file_count, num);
        fm_str_cat(buf, num);
        fm_str_cat(buf, " files, ");
        fm_int_to_str(dir_count, num);
        fm_str_cat(buf, num);
        fm_str_cat(buf, " folders | Total: ");
        fm_format_size(total_size, size_str);
        fm_str_cat(buf, size_str);
        window_set_statusbar(filemgr_win, buf);
    }
    window_paint_widgets(filemgr_win);
}

/* ============================================================
 * Button callbacks
 * ============================================================ */

static void fm_back_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_navigate_back();
    window_paint(filemgr_win);
    fm_draw_content();
}

static void fm_forward_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_navigate_forward();
    window_paint(filemgr_win);
    fm_draw_content();
}

static void fm_up_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_navigate_up();
    window_paint(filemgr_win);
    fm_draw_content();
}

static void fm_home_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_navigate_to("/");
    window_paint(filemgr_win);
    fm_draw_content();
}

static void fm_refresh_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_load_entries(fm_cwd);
    window_paint(filemgr_win);
    fm_draw_content();
}

/* ============================================================
 * Content click handler — makes sidebar, file list, and toolbar
 * interactive instead of just being drawn pixels.
 * x, y are relative to the window content rect.
 * ============================================================ */
static void fm_content_click(window_t* win, uint32_t x, uint32_t y, void* user_data) {
    (void)user_data;
    if (!filemgr_win || !filemgr_win->visible) return;

    /* Content rect dimensions — match fm_draw_content layout */
    uint32_t cx, cy, cw, ch;
    window_get_content_rect(filemgr_win, &cx, &cy, &cw, &ch);

    /* Menu bar area (y < FM_MENU_BAR_H): not interactive yet */
    if (y < FM_MENU_BAR_H) return;

    /* Address bar / toolbar area */
    uint32_t addr_y = FM_MENU_BAR_H;
    if (y >= addr_y && y < addr_y + FM_ADDR_BAR_H) {
        /* "Up" button: x 10..64 */
        if (x >= 10 && x < 64) {
            fm_navigate_up();
            window_paint(filemgr_win);
            fm_draw_content();
            return;
        }
        /* "Open" button: x 72..148 */
        if (x >= 72 && x < 148) {
            if (fm_selected >= 0 && fm_selected < (int32_t)fm_entry_count) {
                if (fm_entries[fm_selected].is_dir) {
                    char path[FM_MAX_PATH];
                    fm_str_copy(path, fm_cwd);
                    if (fm_str_cmp(fm_cwd, "/") != 0) fm_str_cat(path, "/");
                    fm_str_cat(path, fm_entries[fm_selected].name);
                    fm_navigate_to(path);
                }
            }
            window_paint(filemgr_win);
            fm_draw_content();
            return;
        }
        /* "Refresh" button: x 158..262 */
        if (x >= 158 && x < 262) {
            fm_load_entries(fm_cwd);
            window_paint(filemgr_win);
            fm_draw_content();
            return;
        }
        return;
    }

    /* Below address bar: sidebar + file list */
    uint32_t sb_y = addr_y + FM_ADDR_BAR_H;

    /* Sidebar area (x < FM_SIDEBAR_W) */
    if (x < FM_SIDEBAR_W && y >= sb_y) {
        uint32_t item_y = sb_y + 54;
        for (uint32_t i = 0; i < FM_SIDEBAR_COUNT; i++) {
            uint32_t iy = item_y + i * 36;
            if (y >= iy && y < iy + 32) {
                fm_navigate_to(fm_sidebar[i].path);
                window_paint(filemgr_win);
                fm_draw_content();
                return;
            }
        }
        return;
    }

    /* File list area (x >= FM_SIDEBAR_W + 4) */
    uint32_t mx = FM_SIDEBAR_W + 4;
    if (x >= mx && y >= sb_y) {
        uint32_t list_y = sb_y + 42;
        uint32_t row_h = FM_ROW_H;
        if (y >= list_y) {
            int32_t idx = (int32_t)((y - list_y) / row_h);
            if (idx >= 0 && idx < (int32_t)fm_entry_count) {
                fm_selected = idx;
                /* Double-duty: if it's a directory, navigate into it */
                if (fm_entries[idx].is_dir) {
                    char path[FM_MAX_PATH];
                    fm_str_copy(path, fm_cwd);
                    if (fm_str_cmp(fm_cwd, "/") != 0) fm_str_cat(path, "/");
                    fm_str_cat(path, fm_entries[idx].name);
                    fm_navigate_to(path);
                }
                window_paint(filemgr_win);
                fm_draw_content();
                return;
            }
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

window_t* filemgr_create(void) {
    if (filemgr_win) return filemgr_win;

    filemgr_win = window_create(60, 40, 620, 420, "Kenux Explorer");
    filemgr_win->titlebar_color = RGB(0x0A, 0x4B, 0x9A);
    filemgr_win->visible = false;
    filemgr_win->on_content_click = fm_content_click;
    window_set_statusbar(filemgr_win, "0 items");

    /* Navigation buttons (bottom row) */
    uint32_t btn_y = 420 - 2 * BORDER_WIDTH - TITLEBAR_HEIGHT - STATUSBAR_HEIGHT - FM_BTN_H - 4;
    widget_t* btn_back = widget_create_button(8, btn_y, FM_BTN_W, FM_BTN_H,
        "Back", fm_back_click, NULL);
    window_add_widget(filemgr_win, btn_back);

    widget_t* btn_fwd = widget_create_button(8 + (FM_BTN_W + 4), btn_y, FM_BTN_W, FM_BTN_H,
        "Forward", fm_forward_click, NULL);
    window_add_widget(filemgr_win, btn_fwd);

    widget_t* btn_up = widget_create_button(8 + (FM_BTN_W + 4) * 2, btn_y, FM_BTN_W, FM_BTN_H,
        "Up", fm_up_click, NULL);
    window_add_widget(filemgr_win, btn_up);

    widget_t* btn_home = widget_create_button(8 + (FM_BTN_W + 4) * 3, btn_y, FM_BTN_W, FM_BTN_H,
        "Home", fm_home_click, NULL);
    window_add_widget(filemgr_win, btn_home);

    widget_t* btn_refresh = widget_create_button(8 + (FM_BTN_W + 4) * 4, btn_y, FM_BTN_W, FM_BTN_H,
        "Refresh", fm_refresh_click, NULL);
    window_add_widget(filemgr_win, btn_refresh);

    /* Initialise state */
    fm_str_copy(fm_cwd, "/");
    fm_hist_pos = -1;
    fm_hist_count = 0;
    fm_history_push("/");
    fm_load_entries(fm_cwd);

    wm_add_window(filemgr_win);
    return filemgr_win;
}

void filemgr_refresh(void) {
    if (!filemgr_win || !filemgr_win->visible) return;
    if (filemgr_win->state.minimized) return;
    fm_draw_content();
}

void filemgr_on_show(void) {
    if (!filemgr_win || !filemgr_win->visible) return;
    fm_load_entries(fm_cwd);
    window_paint(filemgr_win);
    fm_draw_content();
}
