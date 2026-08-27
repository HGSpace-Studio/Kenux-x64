#ifndef KANVAS_FILE_MANAGER_H
#define KANVAS_FILE_MANAGER_H

#include "kapi_kanvasui.h"
#include "kapi_graphics2d.h"

#define KFM_WIN_W              700
#define KFM_WIN_H              500
#define KFM_SIDEBAR_W          180
#define KFM_TOOLBAR_H          40
#define KFM_STATUSBAR_H        28
#define KFM_ITEM_H             28
#define KFM_ICON_SZ            20
#define KFM_PATH_MAX           512
#define KFM_NAME_MAX           256
#define KFM_MAX_ITEMS          1024
#define KFM_MAX_VISIBLE        32
#define KFM_RADIUS             10

typedef enum {
    KFM_SORT_NAME = 0,
    KFM_SORT_SIZE,
    KFM_SORT_TYPE,
    KFM_SORT_DATE
} kfm_sort_t;

typedef enum {
    KFM_VIEW_LIST = 0,
    KFM_VIEW_GRID,
    KFM_VIEW_ICON
} kfm_view_t;

typedef enum {
    KFM_TYPE_FILE = 0,
    KFM_TYPE_DIR,
    KFM_TYPE_SYMLINK,
    KFM_TYPE_KEX,
    KFM_TYPE_KXP,
    KFM_TYPE_ELF,
    KFM_TYPE_ARCHIVE,
    KFM_TYPE_IMAGE,
    KFM_TYPE_AUDIO,
    KFM_TYPE_VIDEO,
    KFM_TYPE_TEXT,
    KFM_TYPE_UNKNOWN
} kfm_file_type_t;

typedef struct {
    char name[KFM_NAME_MAX];
    kfm_file_type_t type;
    uint64_t size;
    uint64_t modified;
    bool selected;
    bool hidden;
    int icon_id;
} kfm_entry_t;

typedef struct {
    char path[KFM_PATH_MAX];
    kfm_entry_t* entries;
    int entry_count;
    int selected_count;
    int scroll_offset;
    int hovered_index;
    kfm_sort_t sort_mode;
    kfm_view_t view_mode;
    bool show_hidden;
    char search_text[64];
    kfm_entry_t clipboard[KFM_MAX_ITEMS];
    int clipboard_count;
    bool clipboard_cut;
} kfm_pane_t;

typedef struct {
    bool visible;
    int x, y, w, h;
    kfm_pane_t left;
    kfm_pane_t right;
    bool dual_pane;
    int active_pane;
    char status_text[256];
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t sidebar_bg;
    kui_color_t hover_color;
    kui_color_t selected_color;
} kanvas_file_manager_t;

kanvas_file_manager_t* kanvas_file_manager_create(void);
void kanvas_file_manager_destroy(kanvas_file_manager_t* fm);
void kanvas_file_manager_paint(kanvas_file_manager_t* fm, uint32_t* fb, int stride, int fw, int fh);
void kanvas_file_manager_navigate(kanvas_file_manager_t* fm, int pane, const char* path);
void kanvas_file_manager_handle_mouse(kanvas_file_manager_t* fm, int mx, int my, bool left, bool right);
void kanvas_file_manager_handle_key(kanvas_file_manager_t* fm, int key, bool down);
void kanvas_file_manager_refresh(kanvas_file_manager_t* fm, int pane);
void kanvas_file_manager_open(kanvas_file_manager_t* fm, int pane, int index);
void kanvas_file_manager_delete_selected(kanvas_file_manager_t* fm, int pane);
void kanvas_file_manager_copy_selected(kanvas_file_manager_t* fm, int pane);
void kanvas_file_manager_paste(kanvas_file_manager_t* fm, int pane);
void kanvas_file_manager_toggle_dual_pane(kanvas_file_manager_t* fm);
void kanvas_file_manager_sort(kanvas_file_manager_t* fm, int pane, kfm_sort_t mode);
void kanvas_file_manager_set_view(kanvas_file_manager_t* fm, kfm_view_t view);

#endif