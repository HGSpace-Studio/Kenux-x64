#include "kanvas_file_manager.h"
#include "kapi.h"
#include <string.h>

static uint32_t fm_col32(kui_color_t c) { return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

static void fm_fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x; if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++) { uint32_t* p = (uint32_t*)((uint8_t*)fb + r * stride); for (int c = x; c < x + w; c++) p[c] = color; }
}

kanvas_file_manager_t* kanvas_file_manager_create(void)
{
    kanvas_file_manager_t* fm = (kanvas_file_manager_t*)kapi_kmalloc(sizeof(kanvas_file_manager_t));
    if (!fm) return NULL;
    memset(fm, 0, sizeof(kanvas_file_manager_t));
    fm->visible = false;
    fm->w = KFM_WIN_W; fm->h = KFM_WIN_H;
    fm->dual_pane = false;
    fm->active_pane = 0;
    fm->left.scroll_offset = 0; fm->left.hovered_index = -1;
    fm->right.scroll_offset = 0; fm->right.hovered_index = -1;
    memcpy(fm->left.path, "/home", 5);
    memcpy(fm->right.path, "/home", 5);
    fm->left.sort_mode = KFM_SORT_NAME; fm->right.sort_mode = KFM_SORT_NAME;
    fm->left.view_mode = KFM_VIEW_LIST; fm->right.view_mode = KFM_VIEW_LIST;
    fm->bg_color = (kui_color_t){30, 30, 30, 255};
    fm->fg_color = (kui_color_t){230, 230, 230, 255};
    fm->accent_color = (kui_color_t){98, 0, 238, 255};
    fm->sidebar_bg = (kui_color_t){25, 25, 25, 255};
    fm->hover_color = (kui_color_t){50, 50, 50, 255};
    fm->selected_color = (kui_color_t){98, 0, 238, 128};
    return fm;
}

void kanvas_file_manager_destroy(kanvas_file_manager_t* fm)
{
    if (!fm) return;
    if (fm->left.entries) kapi_kfree(fm->left.entries);
    if (fm->right.entries) kapi_kfree(fm->right.entries);
    kapi_kfree(fm);
}

void kanvas_file_manager_paint(kanvas_file_manager_t* fm, uint32_t* fb, int stride, int fw, int fh)
{
    if (!fm || !fm->visible || !fb) return;
    uint32_t bg = fm_col32(fm->bg_color);
    uint32_t fg = fm_col32(fm->fg_color);
    uint32_t sbg = fm_col32(fm->sidebar_bg);
    uint32_t hover = fm_col32(fm->hover_color);
    uint32_t accent = fm_col32(fm->accent_color);
    fm_fill_rect(fb, stride, fw, fh, fm->x, fm->y, fm->w, fm->h, bg);
    fm_fill_rect(fb, stride, fw, fh, fm->x, fm->y, KFM_SIDEBAR_W, fm->h, sbg);
    const char* sidebar_items[] = {"Home", "Documents", "Downloads", "Pictures", "Music", "Videos", "Trash"};
    int sy = fm->y + KFM_TOOLBAR_H + 8;
    for (int i = 0; i < 7; i++) {
        kui_draw_text(fb, stride, fw, fh, fm->x + 16, sy, sidebar_items[i], i == 0 ? accent : fg, 13, 0);
        sy += 28;
    }
    int tb_y = fm->y;
    fm_fill_rect(fb, stride, fw, fh, fm->x + KFM_SIDEBAR_W, tb_y, fm->w - KFM_SIDEBAR_W, KFM_TOOLBAR_H, sbg);
    kui_draw_text(fb, stride, fw, fh, fm->x + KFM_SIDEBAR_W + 12, tb_y + 12, fm->left.path, fg, 13, 0);
    int content_x = fm->x + KFM_SIDEBAR_W;
    int content_y = fm->y + KFM_TOOLBAR_H;
    int content_h = fm->h - KFM_TOOLBAR_H - KFM_STATUSBAR_H;
    kfm_pane_t* pane = fm->active_pane == 0 ? &fm->left : &fm->right;
    int vis_count = content_h / KFM_ITEM_H;
    if (vis_count > KFM_MAX_VISIBLE) vis_count = KFM_MAX_VISIBLE;
    for (int i = 0; i < pane->entry_count && i < vis_count; i++) {
        int idx = i + pane->scroll_offset;
        if (idx >= pane->entry_count) break;
        kfm_entry_t* e = &pane->entries[idx];
        int row_y = content_y + i * KFM_ITEM_H;
        if (e->selected) fm_fill_rect(fb, stride, fw, fh, content_x, row_y, fm->w - KFM_SIDEBAR_W, KFM_ITEM_H, fm_col32(fm->selected_color));
        else if (i == pane->hovered_index) fm_fill_rect(fb, stride, fw, fh, content_x, row_y, fm->w - KFM_SIDEBAR_W, KFM_ITEM_H, hover);
        const char* type_icon = e->type == KFM_TYPE_DIR ? "[D]" : (e->type == KFM_TYPE_KEX ? "[K]" : "   ");
        kui_draw_text(fb, stride, fw, fh, content_x + 8, row_y + 4, type_icon, accent, 12, 0);
        kui_draw_text(fb, stride, fw, fh, content_x + 36, row_y + 4, e->name, fg, 13, 0);
    }
    int sb_y = fm->y + fm->h - KFM_STATUSBAR_H;
    fm_fill_rect(fb, stride, fw, fh, fm->x, sb_y, fm->w, KFM_STATUSBAR_H, sbg);
    kui_draw_text(fb, stride, fw, fh, fm->x + 12, sb_y + 6, fm->status_text, 0x808080FF, 12, 0);
}

void kanvas_file_manager_navigate(kanvas_file_manager_t* fm, int pane, const char* path)
{
    if (!fm || !path) return;
    kfm_pane_t* p = pane == 0 ? &fm->left : &fm->right;
    size_t len = kapi_strlen(path);
    if (len >= KFM_PATH_MAX) len = KFM_PATH_MAX - 1;
    memcpy(p->path, path, len);
    p->path[len] = '\0';
    p->scroll_offset = 0;
    p->hovered_index = -1;
}

void kanvas_file_manager_handle_mouse(kanvas_file_manager_t* fm, int mx, int my, bool left, bool right) { (void)fm; (void)mx; (void)my; (void)left; (void)right; }
void kanvas_file_manager_handle_key(kanvas_file_manager_t* fm, int key, bool down) { (void)fm; (void)key; (void)down; }
void kanvas_file_manager_refresh(kanvas_file_manager_t* fm, int pane) { (void)fm; (void)pane; }
void kanvas_file_manager_open(kanvas_file_manager_t* fm, int pane, int index) { (void)fm; (void)pane; (void)index; }
void kanvas_file_manager_delete_selected(kanvas_file_manager_t* fm, int pane) { (void)fm; (void)pane; }
void kanvas_file_manager_copy_selected(kanvas_file_manager_t* fm, int pane) { (void)fm; (void)pane; }
void kanvas_file_manager_paste(kanvas_file_manager_t* fm, int pane) { (void)fm; (void)pane; }
void kanvas_file_manager_toggle_dual_pane(kanvas_file_manager_t* fm) { if (fm) fm->dual_pane = !fm->dual_pane; }
void kanvas_file_manager_sort(kanvas_file_manager_t* fm, int pane, kfm_sort_t mode) { (void)fm; (void)pane; (void)mode; }
void kanvas_file_manager_set_view(kanvas_file_manager_t* fm, kfm_view_t view) { if (fm) { fm->left.view_mode = view; fm->right.view_mode = view; } }