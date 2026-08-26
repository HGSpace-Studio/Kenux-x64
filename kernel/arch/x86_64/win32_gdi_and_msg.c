#include <arch/win32.h>
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "window.h"
#include "widget.h"
#include "window_manager.h"
#include <arch/slab.h>
#include <string.h>
#include <arch/keyboard.h>
#include <arch/mouse.h>

#define WINDOWS_MAX 256
#define DC_POOL_SIZE 64
#define GDI_OBJECT_POOL_SIZE 512
#define MSG_QUEUE_MAX 256

#define GDI_OBJ_SIG_BRUSH 0x42525553
#define GDI_OBJ_SIG_PEN   0x50454E
#define GDI_OBJ_SIG_FONT  0x464F4E54

#define WND_SIG 0x574E44
#define HDC_SIG 0x484443

typedef struct tagCREATESTRUCTA {
    void*    lpCreateParams;
    HINSTANCE hInstance;
    HMENU    hMenu;
    HWND     hwndParent;
    int      cy;
    int      cx;
    int      y;
    int      x;
    LONG     style;
    const char* lpszName;
    const char* lpszClass;
    DWORD    dwExStyle;
} CREATESTRUCTA;

typedef struct {
    uint32_t signature;
    HWND hwnd;
    int offset_x;
    int offset_y;
    COLORREF text_color;
    COLORREF bk_color;
    int bk_mode;
    HFONT current_font;
    HPEN current_pen;
    HBRUSH current_brush;
    int map_mode;
    int viewport_x;
    int viewport_y;
    int window_x;
    int window_y;
    int viewport_ext_x;
    int viewport_ext_y;
    int window_ext_x;
    int window_ext_y;
    RECT clip;
    bool owns_buffer;
    int pen_pos_x;
    int pen_pos_y;
} gdi_dc_t;

typedef struct {
    uint32_t sig;
    COLORREF color;
    int style;
    int width;
    int weight;
    char face[64];
    int height;
    int escapement;
    int orientation;
    DWORD italic;
    DWORD underline;
    DWORD strikeout;
    DWORD charset;
    DWORD out_precision;
    DWORD clip_precision;
    DWORD quality;
    DWORD pitch_and_family;
} gdi_object_t;

typedef struct {
    uint32_t sig;
    HWND handle;
    HWND parent;
    char class_name[64];
    char window_name[256];
    uint32_t dwStyle;
    uint32_t dwExStyle;
    RECT rect;
    RECT window_rect;
    BOOL visible;
    BOOL enabled;
    BOOL dirty;
    RECT dirty_rect;
    HINSTANCE hinstance;
    WNDPROC wndproc;
    void* user_data;
    HMENU menu;
    HICON hIcon;
    HCURSOR hCursor;
    uint32_t window_bytes_extra;
    uint8_t extra_bytes[128];
    uint32_t child_count;
    HWND children[64];
} win32_window_t;

typedef struct {
    MSG messages[MSG_QUEUE_MAX];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    BOOL quit_posted;
    int quit_code;
} msg_queue_t;

static gdi_dc_t g_dc_table[DC_POOL_SIZE];
static gdi_object_t g_gdi_objects[GDI_OBJECT_POOL_SIZE];
static win32_window_t g_windows[WINDOWS_MAX];
static msg_queue_t g_msg_queue;
static uint64_t g_next_handle = 2;
static HWND g_focus_window = NULL;
static HWND g_foreground_window = NULL;
static int32_t g_mouse_global_x = 0;
static int32_t g_mouse_global_y = 0;

static LRESULT default_button_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT default_edit_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT default_static_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT default_listbox_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT default_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static COLORREF g_sys_colors[25] = {
    0xC0C0C0,
    0x002244,
    0x0078D4,
    0xB7B7B7,
    0xF9F9F9,
    0xFFFFFF,
    0x000000,
    0x000000,
    0x000000,
    0x000000,
    0x0078D4,
    0xB7B7B7,
    0xABABAB,
    0x0078D4,
    0xFFFFFF,
    0xECECEC,
    0xA0A0A0,
    0xA0A0A0,
    0x000000,
    0x000000,
    0xFFFFFF,
    0x696969,
    0xFFFFFF,
    0x000000,
    0xFFFFE1
};

static void win32_init_pools(void) __attribute__((constructor));
static void win32_init_pools(void) {
    uint32_t i;
    for (i = 0; i < (uint32_t)DC_POOL_SIZE; i++) {
        g_dc_table[i].signature = 0;
    }
    for (i = 0; i < (uint32_t)GDI_OBJECT_POOL_SIZE; i++) {
        g_gdi_objects[i].sig = 0;
    }
    for (i = 0; i < (uint32_t)WINDOWS_MAX; i++) {
        g_windows[i].sig = 0;
    }
    g_msg_queue.head = 0;
    g_msg_queue.tail = 0;
    g_msg_queue.count = 0;
    g_msg_queue.quit_posted = FALSE;
    g_msg_queue.quit_code = 0;
}

static HDC alloc_dc(HWND hwnd) {
    int32_t i;
    for (i = 0; i < DC_POOL_SIZE; i++) {
        if (g_dc_table[i].signature == 0) {
            memset(&g_dc_table[i], 0, sizeof(gdi_dc_t));
            g_dc_table[i].signature = HDC_SIG;
            g_dc_table[i].hwnd = hwnd;
            g_dc_table[i].text_color = RGB(0, 0, 0);
            g_dc_table[i].bk_color = RGB(255, 255, 255);
            g_dc_table[i].bk_mode = OPAQUE;
            g_dc_table[i].map_mode = MM_TEXT;
            g_dc_table[i].offset_x = 0;
            g_dc_table[i].offset_y = 0;
            g_dc_table[i].pen_pos_x = 0;
            g_dc_table[i].pen_pos_y = 0;
            g_dc_table[i].clip.left = 0;
            g_dc_table[i].clip.top = 0;
            g_dc_table[i].clip.right = (LONG)fb.width;
            g_dc_table[i].clip.bottom = (LONG)fb.height;
            return (HDC)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

static gdi_dc_t* dc_from_handle(HDC hdc) {
    int32_t idx;
    if (hdc == NULL) return NULL;
    idx = (int32_t)((uintptr_t)hdc - 1);
    if (idx < 0 || idx >= DC_POOL_SIZE) return NULL;
    if (g_dc_table[idx].signature != HDC_SIG) return NULL;
    return &g_dc_table[idx];
}

static void free_dc(HDC hdc) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc != NULL) {
        dc->signature = 0;
    }
}

static uint32_t gdi_obj_find_slot(void) {
    uint32_t i;
    for (i = 0; i < (uint32_t)GDI_OBJECT_POOL_SIZE; i++) {
        if (g_gdi_objects[i].sig == 0) {
            return i + 1;
        }
    }
    return 0;
}

static gdi_object_t* gdi_obj_from_handle(HGDIOBJ obj) {
    uint32_t idx;
    if (obj == NULL) return NULL;
    idx = (uint32_t)((uintptr_t)obj - 1);
    if (idx == 0 || idx > (uint32_t)GDI_OBJECT_POOL_SIZE) return NULL;
    return &g_gdi_objects[idx - 1];
}

static win32_window_t* win_from_handle(HWND hwnd) {
    int32_t i;
    if (hwnd == NULL) return NULL;
    if ((uintptr_t)hwnd == 1) {
        return &g_windows[0];
    }
    for (i = 0; i < (int32_t)WINDOWS_MAX; i++) {
        if (g_windows[i].sig == WND_SIG && g_windows[i].handle == hwnd) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static win32_window_t* win_alloc_slot(void) {
    int32_t i;
    for (i = 0; i < (int32_t)WINDOWS_MAX; i++) {
        if (g_windows[i].sig == 0) {
            memset(&g_windows[i], 0, sizeof(win32_window_t));
            g_windows[i].sig = WND_SIG;
            g_windows[i].handle = (HWND)(g_next_handle++);
            return &g_windows[i];
        }
    }
    return NULL;
}

static void add_child_to_parent(win32_window_t* parent, HWND child) {
    if (parent == NULL || child == NULL) return;
    if (parent->child_count < 64) {
        parent->children[parent->child_count++] = child;
    }
}

static void remove_child_from_parent(win32_window_t* parent, HWND child) {
    uint32_t i, j;
    if (parent == NULL || child == NULL) return;
    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            for (j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            break;
        }
    }
}

HDC BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint) {
    HDC hdc;
    win32_window_t* win;
    if (lpPaint == NULL) return NULL;
    hdc = GetDC(hWnd);
    lpPaint->hdc = hdc;
    lpPaint->fErase = TRUE;
    win = win_from_handle(hWnd);
    if (win != NULL) {
        lpPaint->rcPaint = win->dirty_rect;
        if (lpPaint->rcPaint.left == 0 && lpPaint->rcPaint.top == 0 &&
            lpPaint->rcPaint.right == 0 && lpPaint->rcPaint.bottom == 0) {
            lpPaint->rcPaint = win->rect;
        }
    }
    return hdc;
}

BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint) {
    if (lpPaint != NULL && lpPaint->hdc != NULL) {
        ReleaseDC(hWnd, lpPaint->hdc);
    }
    return TRUE;
}

HDC GetDC(HWND hWnd) {
    HDC hdc;
    gdi_dc_t* dc;
    win32_window_t* win;
    hdc = alloc_dc(hWnd);
    if (hdc == NULL) return NULL;
    dc = dc_from_handle(hdc);
    if (dc != NULL) {
        win = win_from_handle(hWnd);
        if (win != NULL) {
            dc->offset_x = (int)win->window_rect.left;
            dc->offset_y = (int)win->window_rect.top;
            dc->clip = win->rect;
        }
    }
    return hdc;
}

int ReleaseDC(HWND hWnd, HDC hDC) {
    (void)hWnd;
    free_dc(hDC);
    return TRUE;
}

HDC GetWindowDC(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    gdi_dc_t* dc = dc_from_handle(hdc);
    win32_window_t* win = win_from_handle(hWnd);
    if (dc != NULL && win != NULL) {
        dc->clip = win->window_rect;
    }
    return hdc;
}

int GetSystemMetrics(int nIndex) {
    switch (nIndex) {
        case SM_CXSCREEN: return (int)fb.width;
        case SM_CYSCREEN: return (int)fb.height;
        case SM_CXFULLSCREEN: return (int)fb.width;
        case SM_CYFULLSCREEN: return (int)fb.height;
        case SM_CXMIN: return 112;
        case SM_CYMIN: return 27;
        case SM_CXSIZE: return 32;
        case SM_CYSIZE: return 26;
        case SM_CXBORDER: return 1;
        case SM_CYBORDER: return 1;
        case SM_CXDLGFRAME: return 7;
        case SM_CYDLGFRAME: return 7;
        case SM_CXICON: return 32;
        case SM_CYICON: return 32;
        case SM_CXCURSOR: return 32;
        case SM_CYCURSOR: return 32;
        case SM_CYMENU: return 20;
        case SM_CYCAPTION: return 26;
        case SM_CXSMICON: return 16;
        case SM_CYSMICON: return 16;
        case SM_DBCSENABLED: return 0;
        case SM_MENUDROPALIGNMENT: return 0;
        case SM_DEBUG: return 0;
        case SM_MOUSEPRESENT: return 1;
        case SM_SWAPBUTTON: return 0;
        case SM_CXDOUBLECLK: return 4;
        case SM_CYDOUBLECLK: return 4;
        case SM_CXFRAME: return 48;
        case SM_CYFRAME: return 48;
        case SM_CXMINTRACK: return 112;
        case SM_CYMINTRACK: return 27;
        case SM_CXTHUMB: return 16;
        case SM_CYVTHUMB: return 16;
        default: return 0;
    }
}

int GetDpiForWindow(HWND hwnd) {
    (void)hwnd;
    return 96;
}

int GetDpiForSystem(void) {
    return 96;
}

HBRUSH GetSysColorBrush(int nIndex) {
    COLORREF color = GetSysColor(nIndex);
    return CreateSolidBrush(color);
}

DWORD GetSysColor(int nIndex) {
    if (nIndex < 0 || nIndex >= 25) return 0;
    return (DWORD)g_sys_colors[nIndex];
}

BOOL SetSysColors(int cElements, const int* lpaElements, const COLORREF* lpaRgbValues) {
    int i;
    if (cElements <= 0 || lpaElements == NULL || lpaRgbValues == NULL) return FALSE;
    for (i = 0; i < cElements; i++) {
        int idx = lpaElements[i];
        if (idx >= 0 && idx < 25) {
            g_sys_colors[idx] = lpaRgbValues[i];
        }
    }
    return TRUE;
}

COLORREF GetPixel(HDC hdc, int X, int Y) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return 0;
    return (COLORREF)gui_fb_get_pixel((uint32_t)(X + dc->offset_x), (uint32_t)(Y + dc->offset_y));
}

COLORREF SetPixel(HDC hdc, int X, int Y, COLORREF crColor) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return 0;
    gui_fb_set_pixel((uint32_t)(X + dc->offset_x), (uint32_t)(Y + dc->offset_y), fb_convert_color(crColor));
    return crColor;
}

BOOL MoveToEx(HDC hdc, int X, int Y, POINT* lpPoint) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return FALSE;
    if (lpPoint != NULL) {
        lpPoint->x = (LONG)dc->pen_pos_x;
        lpPoint->y = (LONG)dc->pen_pos_y;
    }
    dc->pen_pos_x = X;
    dc->pen_pos_y = Y;
    return TRUE;
}

BOOL LineTo(HDC hdc, int X, int Y) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* pen_obj;
    if (dc == NULL) return FALSE;
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (pen_obj != NULL) {
        pen_color = pen_obj->color;
    }
    gfx_draw_line((int32_t)(dc->pen_pos_x + dc->offset_x),
                  (int32_t)(dc->pen_pos_y + dc->offset_y),
                  (int32_t)(X + dc->offset_x),
                  (int32_t)(Y + dc->offset_y),
                  fb_convert_color(pen_color));
    dc->pen_pos_x = X;
    dc->pen_pos_y = Y;
    return TRUE;
}

BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF brush_color = RGB(255, 255, 255);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* brush_obj;
    gdi_object_t* pen_obj;
    int x, y, w, h;
    if (dc == NULL) return FALSE;
    brush_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_brush);
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (brush_obj != NULL) brush_color = brush_obj->color;
    if (pen_obj != NULL) pen_color = pen_obj->color;
    x = left + dc->offset_x;
    y = top + dc->offset_y;
    w = right - left;
    h = bottom - top;
    if (w > 0 && h > 0) {
        gui_fb_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, fb_convert_color(brush_color));
    }
    gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, fb_convert_color(pen_color));
    gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, fb_convert_color(pen_color));
    gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, fb_convert_color(pen_color));
    gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, fb_convert_color(pen_color));
    return TRUE;
}

BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int width, int height) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF brush_color = RGB(255, 255, 255);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* brush_obj;
    gdi_object_t* pen_obj;
    int x, y, w, h, rw, rh;
    if (dc == NULL) return FALSE;
    brush_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_brush);
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (brush_obj != NULL) brush_color = brush_obj->color;
    if (pen_obj != NULL) pen_color = pen_obj->color;
    x = left + dc->offset_x;
    y = top + dc->offset_y;
    w = right - left;
    h = bottom - top;
    rw = width / 2;
    rh = height / 2;
    if (w > 0 && h > 0) {
        gui_fb_fill_rect((uint32_t)(x + rw), (uint32_t)y, (uint32_t)(w - 2 * rw), (uint32_t)h, fb_convert_color(brush_color));
        gui_fb_fill_rect((uint32_t)x, (uint32_t)(y + rh), (uint32_t)rw, (uint32_t)(h - 2 * rh), fb_convert_color(brush_color));
        gui_fb_fill_rect((uint32_t)(x + w - rw), (uint32_t)(y + rh), (uint32_t)rw, (uint32_t)(h - 2 * rh), fb_convert_color(brush_color));
    }
    if (w > 2 * rw) {
        gfx_draw_hline((uint32_t)(x + rw), (uint32_t)y, (uint32_t)(w - 2 * rw), fb_convert_color(pen_color));
        gfx_draw_hline((uint32_t)(x + rw), (uint32_t)(y + h - 1), (uint32_t)(w - 2 * rw), fb_convert_color(pen_color));
    }
    if (h > 2 * rh) {
        gfx_draw_vline((uint32_t)x, (uint32_t)(y + rh), (uint32_t)(h - 2 * rh), fb_convert_color(pen_color));
        gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)(y + rh), (uint32_t)(h - 2 * rh), fb_convert_color(pen_color));
    }
    return TRUE;
}

static const int16_t g_ellipse_sin[25] = {
    0, 8363, 16069, 22765, 28138, 31939, 33999, 34263, 32702,
    29437, 24624, 18510, 11420, 3699, -4239, -11954, -19051,
    -25102, -29755, -32787, -34104, -33649, -31480, -27710, -22506
};
static const int16_t g_ellipse_cos[25] = {
    32767, 31683, 28588, 23715, 17364, 9952, 1916, -6270, -14256,
    -21473, -27452, -31783, -34183, -34516, -32730, -28899, -23224,
    -16016, -7649, 1299, 10197, 18435, 25369, 30478, 33497
};

BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF brush_color = RGB(255, 255, 255);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* brush_obj;
    gdi_object_t* pen_obj;
    int cx, cy, rx, ry;
    int i;
    const int segments = 24;
    if (dc == NULL) return FALSE;
    brush_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_brush);
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (brush_obj != NULL) brush_color = brush_obj->color;
    if (pen_obj != NULL) pen_color = pen_obj->color;
    cx = (left + right) / 2 + dc->offset_x;
    cy = (top + bottom) / 2 + dc->offset_y;
    rx = (right - left) / 2;
    ry = (bottom - top) / 2;
    gfx_draw_circle((int32_t)cx, (int32_t)cy, (int32_t)(rx < ry ? rx : ry), fb_convert_color(pen_color));
    for (i = 0; i < segments; i++) {
        int32_t x0, y0, x1, y1;
        int32_t s0, c0, s1, c1;
        s0 = (int32_t)g_ellipse_sin[i];
        c0 = (int32_t)g_ellipse_cos[i];
        s1 = (int32_t)g_ellipse_sin[i + 1];
        c1 = (int32_t)g_ellipse_cos[i + 1];
        x0 = (int32_t)cx + (int32_t)((c0 * (int32_t)rx) / 32767);
        y0 = (int32_t)cy - (int32_t)((s0 * (int32_t)ry) / 32767);
        x1 = (int32_t)cx + (int32_t)((c1 * (int32_t)rx) / 32767);
        y1 = (int32_t)cy - (int32_t)((s1 * (int32_t)ry) / 32767);
        gfx_draw_line(x0, y0, x1, y1, fb_convert_color(pen_color));
    }
    return TRUE;
}

BOOL Polygon(HDC hdc, const POINT* lpPoints, int iCount) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* pen_obj;
    int i;
    if (dc == NULL || lpPoints == NULL || iCount < 2) return FALSE;
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (pen_obj != NULL) pen_color = pen_obj->color;
    for (i = 0; i < iCount - 1; i++) {
        gfx_draw_line((int32_t)(lpPoints[i].x + dc->offset_x),
                      (int32_t)(lpPoints[i].y + dc->offset_y),
                      (int32_t)(lpPoints[i + 1].x + dc->offset_x),
                      (int32_t)(lpPoints[i + 1].y + dc->offset_y),
                      fb_convert_color(pen_color));
    }
    gfx_draw_line((int32_t)(lpPoints[iCount - 1].x + dc->offset_x),
                  (int32_t)(lpPoints[iCount - 1].y + dc->offset_y),
                  (int32_t)(lpPoints[0].x + dc->offset_x),
                  (int32_t)(lpPoints[0].y + dc->offset_y),
                  fb_convert_color(pen_color));
    return TRUE;
}

BOOL Polyline(HDC hdc, const POINT* lppt, int cPoints) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF pen_color = RGB(0, 0, 0);
    gdi_object_t* pen_obj;
    int i;
    if (dc == NULL || lppt == NULL || cPoints < 2) return FALSE;
    pen_obj = gdi_obj_from_handle((HGDIOBJ)dc->current_pen);
    if (pen_obj != NULL) pen_color = pen_obj->color;
    for (i = 0; i < cPoints - 1; i++) {
        gfx_draw_line((int32_t)(lppt[i].x + dc->offset_x),
                      (int32_t)(lppt[i].y + dc->offset_y),
                      (int32_t)(lppt[i + 1].x + dc->offset_x),
                      (int32_t)(lppt[i + 1].y + dc->offset_y),
                      fb_convert_color(pen_color));
    }
    return TRUE;
}

HBRUSH CreateSolidBrush(COLORREF color) {
    uint32_t slot = gdi_obj_find_slot();
    if (slot == 0) return NULL;
    g_gdi_objects[slot - 1].sig = GDI_OBJ_SIG_BRUSH;
    g_gdi_objects[slot - 1].color = color;
    g_gdi_objects[slot - 1].style = 0;
    return (HBRUSH)(uintptr_t)slot;
}

HPEN CreatePen(int iStyle, int cWidth, COLORREF color) {
    uint32_t slot = gdi_obj_find_slot();
    if (slot == 0) return NULL;
    g_gdi_objects[slot - 1].sig = GDI_OBJ_SIG_PEN;
    g_gdi_objects[slot - 1].color = color;
    g_gdi_objects[slot - 1].style = iStyle;
    g_gdi_objects[slot - 1].width = cWidth;
    return (HPEN)(uintptr_t)slot;
}

HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
                  DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
                  DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
                  DWORD iPitchAndFamily, const char* pszFaceName) {
    uint32_t slot = gdi_obj_find_slot();
    gdi_object_t* obj;
    if (slot == 0) return NULL;
    obj = &g_gdi_objects[slot - 1];
    obj->sig = GDI_OBJ_SIG_FONT;
    obj->height = cHeight;
    obj->width = cWidth;
    obj->escapement = cEscapement;
    obj->orientation = cOrientation;
    obj->weight = cWeight;
    obj->italic = bItalic;
    obj->underline = bUnderline;
    obj->strikeout = bStrikeOut;
    obj->charset = iCharSet;
    obj->out_precision = iOutPrecision;
    obj->clip_precision = iClipPrecision;
    obj->quality = iQuality;
    obj->pitch_and_family = iPitchAndFamily;
    if (pszFaceName != NULL) {
        strncpy(obj->face, pszFaceName, sizeof(obj->face) - 1);
        obj->face[sizeof(obj->face) - 1] = '\0';
    } else {
        obj->face[0] = '\0';
    }
    return (HFONT)(uintptr_t)slot;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ hgdiobj) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    gdi_object_t* obj;
    HGDIOBJ old = NULL;
    if (dc == NULL || hgdiobj == NULL) return NULL;
    obj = gdi_obj_from_handle(hgdiobj);
    if (obj == NULL) return NULL;
    switch (obj->sig) {
        case GDI_OBJ_SIG_BRUSH:
            old = (HGDIOBJ)dc->current_brush;
            dc->current_brush = (HBRUSH)hgdiobj;
            break;
        case GDI_OBJ_SIG_PEN:
            old = (HGDIOBJ)dc->current_pen;
            dc->current_pen = (HPEN)hgdiobj;
            break;
        case GDI_OBJ_SIG_FONT:
            old = (HGDIOBJ)dc->current_font;
            dc->current_font = (HFONT)hgdiobj;
            break;
        default:
            return NULL;
    }
    return old;
}

BOOL DeleteObject(HGDIOBJ ho) {
    gdi_object_t* obj = gdi_obj_from_handle(ho);
    if (obj == NULL) return FALSE;
    obj->sig = 0;
    return TRUE;
}

COLORREF SetBkColor(HDC hdc, COLORREF color) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF old;
    if (dc == NULL) return 0;
    old = dc->bk_color;
    dc->bk_color = color;
    return old;
}

COLORREF SetTextColor(HDC hdc, COLORREF crColor) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    COLORREF old;
    if (dc == NULL) return 0;
    old = dc->text_color;
    dc->text_color = crColor;
    return old;
}

int SetBkMode(HDC hdc, int mode) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    int old;
    if (dc == NULL) return 0;
    old = dc->bk_mode;
    dc->bk_mode = mode;
    return old;
}

int SetMapMode(HDC hdc, int fnMapMode) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    int old;
    if (dc == NULL) return 0;
    old = dc->map_mode;
    dc->map_mode = fnMapMode;
    return old;
}

BOOL SetViewportOrgEx(HDC hdc, int X, int Y, POINT* lpPoint) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return FALSE;
    if (lpPoint != NULL) {
        lpPoint->x = (LONG)dc->viewport_x;
        lpPoint->y = (LONG)dc->viewport_y;
    }
    dc->viewport_x = X;
    dc->viewport_y = Y;
    return TRUE;
}

BOOL SetWindowOrgEx(HDC hdc, int X, int Y, POINT* lpPoint) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return FALSE;
    if (lpPoint != NULL) {
        lpPoint->x = (LONG)dc->window_x;
        lpPoint->y = (LONG)dc->window_y;
    }
    dc->window_x = X;
    dc->window_y = Y;
    return TRUE;
}

BOOL SetViewportExtEx(HDC hdc, int xExt, int yExt, SIZE* lpSize) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return FALSE;
    if (lpSize != NULL) {
        lpSize->cx = (LONG)dc->viewport_ext_x;
        lpSize->cy = (LONG)dc->viewport_ext_y;
    }
    dc->viewport_ext_x = xExt;
    dc->viewport_ext_y = yExt;
    return TRUE;
}

BOOL SetWindowExtEx(HDC hdc, int xExt, int yExt, SIZE* lpSize) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    if (dc == NULL) return FALSE;
    if (lpSize != NULL) {
        lpSize->cx = (LONG)dc->window_ext_x;
        lpSize->cy = (LONG)dc->window_ext_y;
    }
    dc->window_ext_x = xExt;
    dc->window_ext_y = yExt;
    return TRUE;
}

BOOL FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    gdi_object_t* brush_obj;
    COLORREF brush_color = RGB(255, 255, 255);
    int x, y, w, h;
    if (dc == NULL || lprc == NULL) return FALSE;
    brush_obj = gdi_obj_from_handle((HGDIOBJ)hbr);
    if (brush_obj != NULL) brush_color = brush_obj->color;
    x = (int)lprc->left + dc->offset_x;
    y = (int)lprc->top + dc->offset_y;
    w = (int)(lprc->right - lprc->left);
    h = (int)(lprc->bottom - lprc->top);
    if (w > 0 && h > 0) {
        gui_fb_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, fb_convert_color(brush_color));
    }
    return TRUE;
}

BOOL FrameRect(HDC hdc, const RECT* lprc, HBRUSH hbr) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    gdi_object_t* brush_obj;
    COLORREF brush_color = RGB(0, 0, 0);
    int x, y, w, h;
    if (dc == NULL || lprc == NULL) return FALSE;
    brush_obj = gdi_obj_from_handle((HGDIOBJ)hbr);
    if (brush_obj != NULL) brush_color = brush_obj->color;
    x = (int)lprc->left + dc->offset_x;
    y = (int)lprc->top + dc->offset_y;
    w = (int)(lprc->right - lprc->left);
    h = (int)(lprc->bottom - lprc->top);
    if (w > 0 && h > 0) {
        gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, fb_convert_color(brush_color));
        gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, fb_convert_color(brush_color));
        gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, fb_convert_color(brush_color));
        gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, fb_convert_color(brush_color));
    }
    return TRUE;
}

BOOL InvertRect(HDC hdc, const RECT* lprc) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    int x, y, w, h, i, j;
    if (dc == NULL || lprc == NULL) return FALSE;
    x = (int)lprc->left + dc->offset_x;
    y = (int)lprc->top + dc->offset_y;
    w = (int)(lprc->right - lprc->left);
    h = (int)(lprc->bottom - lprc->top);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint32_t pix = gui_fb_get_pixel((uint32_t)(x + i), (uint32_t)(y + j));
            uint8_t r = (uint8_t)(pix & 0xFF);
            uint8_t g = (uint8_t)((pix >> 8) & 0xFF);
            uint8_t b = (uint8_t)((pix >> 16) & 0xFF);
            r = (uint8_t)(~r);
            g = (uint8_t)(~g);
            b = (uint8_t)(~b);
            gui_fb_set_pixel((uint32_t)(x + i), (uint32_t)(y + j), RGB(r, g, b));
        }
    }
    return TRUE;
}

BOOL TextOutA(HDC hdc, int X, int Y, const char* lpString, int c) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    char buf[1024];
    if (dc == NULL || lpString == NULL || c <= 0) return FALSE;
    if (c > (int)(sizeof(buf) - 1)) c = (int)(sizeof(buf) - 1);
    memcpy(buf, lpString, (size_t)c);
    buf[c] = '\0';
    font_draw_text((uint32_t)(X + dc->offset_x), (uint32_t)(Y + dc->offset_y), buf, fb_convert_color(dc->text_color));
    return TRUE;
}

BOOL ExtTextOutA(HDC hdc, int X, int Y, UINT fuOptions, const RECT* lprc,
                 const char* lpString, UINT cbCount, const int* lpDx) {
    (void)fuOptions;
    (void)lprc;
    (void)lpDx;
    return TextOutA(hdc, X, Y, lpString, (int)cbCount);
}

int DrawTextA(HDC hdc, const char* lpchText, int cchText, RECT* lprc, UINT format) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    int text_w, text_h, draw_x, draw_y;
    if (dc == NULL || lpchText == NULL || lprc == NULL) return 0;
    if (cchText == -1) cchText = (int)strlen(lpchText);
    text_w = (int)font_text_width(lpchText);
    text_h = FONT_HEIGHT;
    if (format & DT_CALCRECT) {
        lprc->right = lprc->left + (LONG)text_w;
        lprc->bottom = lprc->top + (LONG)text_h;
        return text_h;
    }
    draw_x = (int)lprc->left;
    if (format & DT_CENTER) {
        draw_x += (int)((lprc->right - lprc->left - (LONG)text_w) / 2);
    } else if (format & DT_RIGHT) {
        draw_x += (int)(lprc->right - lprc->left - (LONG)text_w);
    }
    draw_y = (int)lprc->top;
    if (format & DT_VCENTER) {
        draw_y += (int)((lprc->bottom - lprc->top - (LONG)text_h) / 2);
    } else if (!(format & DT_SINGLELINE)) {
    }
    TextOutA(hdc, draw_x - dc->offset_x, draw_y - dc->offset_y, lpchText, cchText);
    return text_h;
}

BOOL DrawEdge(HDC hdc, RECT* qrc, UINT edge, UINT grfFlags) {
    COLORREF light, shadow, dkshadow;
    int x, y, w, h;
    if (hdc == NULL || qrc == NULL) return FALSE;
    light = GetSysColor(COLOR_3DLIGHT);
    shadow = GetSysColor(COLOR_BTNSHADOW);
    dkshadow = GetSysColor(COLOR_3DDKSHADOW);
    x = (int)qrc->left;
    y = (int)qrc->top;
    w = (int)(qrc->right - qrc->left);
    h = (int)(qrc->bottom - qrc->top);
    (void)grfFlags;
    if (edge & EDGE_RAISED) {
        gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, light);
        gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, light);
        gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, shadow);
        gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, shadow);
    } else if (edge & EDGE_SUNKEN) {
        gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, shadow);
        gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, shadow);
        gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, light);
        gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, light);
    } else if (edge & EDGE_ETCHED) {
        gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, shadow);
        gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, shadow);
        gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, dkshadow);
        gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, dkshadow);
        y++; x++; w -= 2; h -= 2;
        if (w > 0 && h > 0) {
            gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, light);
            gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, light);
        }
    } else if (edge & EDGE_BUMP) {
        gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, light);
        gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, light);
        y++; x++; w -= 2; h -= 2;
        if (w > 0 && h > 0) {
            gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, shadow);
            gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, shadow);
        }
    }
    return TRUE;
}

BOOL DrawFrameControl(HDC hdc, RECT* lpRect, UINT uType, UINT uState) {
    COLORREF face, light, shadow, text;
    int x, y, w, h;
    if (hdc == NULL || lpRect == NULL) return FALSE;
    face = GetSysColor(COLOR_BTNFACE);
    light = GetSysColor(COLOR_3DLIGHT);
    shadow = GetSysColor(COLOR_BTNSHADOW);
    text = GetSysColor(COLOR_BTNTEXT);
    x = (int)lpRect->left;
    y = (int)lpRect->top;
    w = (int)(lpRect->right - lpRect->left);
    h = (int)(lpRect->bottom - lpRect->top);
    if (uType == DFC_BUTTON) {
        FillRect(hdc, lpRect, CreateSolidBrush(face));
        if (uState & DFCS_PUSHED) {
            gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, shadow);
            gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, shadow);
            gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, light);
            gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, light);
        } else {
            gfx_draw_hline((uint32_t)x, (uint32_t)y, (uint32_t)w, light);
            gfx_draw_vline((uint32_t)x, (uint32_t)y, (uint32_t)h, light);
            gfx_draw_hline((uint32_t)x, (uint32_t)(y + h - 1), (uint32_t)w, shadow);
            gfx_draw_vline((uint32_t)(x + w - 1), (uint32_t)y, (uint32_t)h, shadow);
        }
    } else if (uType == DFC_CAPTION) {
        COLORREF cap = GetSysColor(COLOR_ACTIVECAPTION);
        FillRect(hdc, lpRect, CreateSolidBrush(cap));
    } else if (uType == DFC_MENU) {
        COLORREF menu = GetSysColor(COLOR_MENU);
        FillRect(hdc, lpRect, CreateSolidBrush(menu));
    } else if (uType == DFC_SCROLLBAR) {
        FillRect(hdc, lpRect, CreateSolidBrush(face));
        if (uState & DFCS_UP) {
            gfx_draw_line((int32_t)(x + w / 2), (int32_t)(y + 2),
                          (int32_t)(x + 2), (int32_t)(y + h - 3), fb_convert_color(text));
            gfx_draw_line((int32_t)(x + w / 2), (int32_t)(y + 2),
                          (int32_t)(x + w - 3), (int32_t)(y + h - 3), fb_convert_color(text));
        } else if (uState & DFCS_DOWN) {
            gfx_draw_line((int32_t)(x + 2), (int32_t)(y + 2),
                          (int32_t)(x + w / 2), (int32_t)(y + h - 3), fb_convert_color(text));
            gfx_draw_line((int32_t)(x + w - 3), (int32_t)(y + 2),
                          (int32_t)(x + w / 2), (int32_t)(y + h - 3), fb_convert_color(text));
        }
    }
    return TRUE;
}

BOOL PatBlt(HDC hdc, int X, int Y, int W, int H, DWORD rop) {
    gdi_dc_t* dc = dc_from_handle(hdc);
    int x, y;
    if (dc == NULL) return FALSE;
    x = X + dc->offset_x;
    y = Y + dc->offset_y;
    if (W <= 0 || H <= 0) return FALSE;
    if (rop == BLACKNESS) {
        gui_fb_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)W, (uint32_t)H, RGB(0, 0, 0));
    } else if (rop == WHITENESS) {
        gui_fb_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)W, (uint32_t)H, RGB(255, 255, 255));
    }
    return TRUE;
}

BOOL BitBlt(HDC hdc, int X, int Y, int W, int H, HDC hdcSrc, int XSrc, int YSrc, DWORD dwRop) {
    gdi_dc_t* dst = dc_from_handle(hdc);
    gdi_dc_t* src = dc_from_handle(hdcSrc);
    int i, j;
    if (dst == NULL || src == NULL) return FALSE;
    if (dwRop != SRCCOPY) return FALSE;
    if (W <= 0 || H <= 0) return FALSE;
    for (j = 0; j < H; j++) {
        for (i = 0; i < W; i++) {
            uint32_t pix = gui_fb_get_pixel((uint32_t)(XSrc + src->offset_x + i),
                                            (uint32_t)(YSrc + src->offset_y + j));
            gui_fb_set_pixel((uint32_t)(X + dst->offset_x + i),
                             (uint32_t)(Y + dst->offset_y + j), pix);
        }
    }
    return TRUE;
}

BOOL StretchBlt(HDC hdc, int XDest, int YDest, int WDest, int HDest,
                HDC hdcSrc, int XSrc, int YSrc, int WSrc, int HSrc, DWORD dwRop) {
    gdi_dc_t* dst = dc_from_handle(hdc);
    gdi_dc_t* src = dc_from_handle(hdcSrc);
    int i, j;
    if (dst == NULL || src == NULL) return FALSE;
    if (dwRop != SRCCOPY) return FALSE;
    if (WDest <= 0 || HDest <= 0 || WSrc <= 0 || HSrc <= 0) return FALSE;
    for (j = 0; j < HDest; j++) {
        int sy = (j * HSrc) / HDest;
        for (i = 0; i < WDest; i++) {
            int sx = (i * WSrc) / WDest;
            uint32_t pix = gui_fb_get_pixel((uint32_t)(XSrc + src->offset_x + sx),
                                            (uint32_t)(YSrc + src->offset_y + sy));
            gui_fb_set_pixel((uint32_t)(XDest + dst->offset_x + i),
                             (uint32_t)(YDest + dst->offset_y + j), pix);
        }
    }
    return TRUE;
}

static WNDPROC get_builtin_wndproc(const char* class_name) {
    if (class_name == NULL) return default_window_proc;
    if (strcmp(class_name, "Button") == 0) return default_button_proc;
    if (strcmp(class_name, "Edit") == 0) return default_edit_proc;
    if (strcmp(class_name, "Static") == 0) return default_static_proc;
    if (strcmp(class_name, "ListBox") == 0) return default_listbox_proc;
    return default_window_proc;
}

static LRESULT default_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    win32_window_t* win = win_from_handle(hwnd);
    switch (msg) {
        case WM_CREATE:
        case WM_NCCREATE:
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (win != NULL) {
                RECT rc = win->rect;
                FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return (LRESULT)TRUE;
        case WM_DESTROY:
        case WM_NCDESTROY:
            return 0;
        default:
            break;
    }
    return 0;
}

static LRESULT default_button_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    win32_window_t* win = win_from_handle(hwnd);
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (win != NULL) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                DrawFrameControl(hdc, &rc, DFC_BUTTON, DFCS_BUTTONPUSH);
                if (win->window_name[0] != '\0') {
                    DrawTextA(hdc, win->window_name, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
            EndPaint(hwnd, &ps);
        } return 0;
        case WM_LBUTTONDOWN:
            return 0;
        case WM_LBUTTONUP:
            if (win != NULL && win->parent != NULL) {
                PostMessageA(win->parent, WM_COMMAND, (WPARAM)((uintptr_t)win->menu), (LPARAM)hwnd);
            }
            return 0;
        default:
            break;
    }
    return default_window_proc(hwnd, msg, wparam, lparam);
}

static LRESULT default_edit_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    win32_window_t* win = win_from_handle(hwnd);
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (win != NULL) {
                RECT rc;
                HBRUSH brush;
                GetClientRect(hwnd, &rc);
                brush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
                FillRect(hdc, &rc, brush);
                DeleteObject((HGDIOBJ)brush);
                DrawEdge(hdc, &rc, EDGE_SUNKEN, BF_RECT);
                if (win->window_name[0] != '\0') {
                    RECT inner = rc;
                    inner.left += 2;
                    inner.top += 2;
                    DrawTextA(hdc, win->window_name, -1, &inner, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            EndPaint(hwnd, &ps);
        } return 0;
        case WM_CHAR: {
            size_t len;
            if (win == NULL) return 0;
            len = strlen(win->window_name);
            if ((wparam == (WPARAM)KEY_BACKSPACE) && len > 0) {
                win->window_name[len - 1] = '\0';
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (wparam >= (WPARAM)0x20 && wparam < (WPARAM)0x7F && len < sizeof(win->window_name) - 1) {
                win->window_name[len] = (char)wparam;
                win->window_name[len + 1] = '\0';
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } return 0;
        default:
            break;
    }
    return default_window_proc(hwnd, msg, wparam, lparam);
}

static LRESULT default_static_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    win32_window_t* win = win_from_handle(hwnd);
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (win != NULL) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (win->dwStyle & WS_BORDER) {
                    FrameRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOWFRAME));
                }
                if (win->window_name[0] != '\0') {
                    DrawTextA(hdc, win->window_name, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            EndPaint(hwnd, &ps);
        } return 0;
        default:
            break;
    }
    return default_window_proc(hwnd, msg, wparam, lparam);
}

static LRESULT default_listbox_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            DrawEdge(hdc, &rc, EDGE_SUNKEN, BF_RECT);
            EndPaint(hwnd, &ps);
        } return 0;
        default:
            break;
    }
    return default_window_proc(hwnd, msg, wparam, lparam);
}

HWND CreateWindowExA(uint32_t dwExStyle, const char* lpClassName, const char* lpWindowName,
                     uint32_t dwStyle, int X, int Y, int nWidth, int nHeight,
                     HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, void* lpParam) {
    win32_window_t* win;
    win32_window_t* parent;
    WNDPROC user_wndproc = NULL;
    (void)lpParam;
    win = win_alloc_slot();
    if (win == NULL) return NULL;
    if (lpClassName != NULL) {
        strncpy(win->class_name, lpClassName, sizeof(win->class_name) - 1);
    }
    if (lpWindowName != NULL) {
        strncpy(win->window_name, lpWindowName, sizeof(win->window_name) - 1);
    }
    win->dwExStyle = dwExStyle;
    win->dwStyle = dwStyle;
    win->hinstance = hInstance;
    win->menu = hMenu;
    win->parent = hWndParent;
    win->enabled = (dwStyle & WS_DISABLED) ? FALSE : TRUE;
    win->visible = (dwStyle & WS_VISIBLE) ? TRUE : FALSE;
    win->dirty = FALSE;
    win->wndproc = get_builtin_wndproc(lpClassName);
    parent = win_from_handle(hWndParent);
    if (parent != NULL) {
        win->rect.left = (LONG)X;
        win->rect.top = (LONG)Y;
        win->window_rect.left = parent->window_rect.left + (LONG)X;
        win->window_rect.top = parent->window_rect.top + (LONG)Y;
        add_child_to_parent(parent, win->handle);
    } else {
        win->rect.left = (LONG)X;
        win->rect.top = (LONG)Y;
        win->window_rect.left = (LONG)X;
        win->window_rect.top = (LONG)Y;
    }
    win->rect.right = win->rect.left + (LONG)nWidth;
    win->rect.bottom = win->rect.top + (LONG)nHeight;
    win->window_rect.right = win->window_rect.left + (LONG)nWidth;
    win->window_rect.bottom = win->window_rect.top + (LONG)nHeight;
    if (win->wndproc != NULL) {
        CREATESTRUCTA cs;
        memset(&cs, 0, sizeof(cs));
        cs.lpCreateParams = lpParam;
        cs.hInstance = hInstance;
        cs.hMenu = hMenu;
        cs.hwndParent = hWndParent;
        cs.cx = nWidth;
        cs.cy = nHeight;
        cs.y = Y;
        cs.x = X;
        cs.style = dwStyle;
        cs.lpszName = lpWindowName;
        cs.lpszClass = lpClassName;
        cs.dwExStyle = dwExStyle;
        (void)user_wndproc;
        win->wndproc(win->handle, WM_NCCREATE, 0, (LPARAM)&cs);
        win->wndproc(win->handle, WM_CREATE, 0, (LPARAM)&cs);
    }
    return win->handle;
}

HWND CreateWindowA(const char* lpClassName, const char* lpWindowName, uint32_t dwStyle,
                   int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
                   HINSTANCE hInstance, void* lpParam) {
    return CreateWindowExA(0, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
                           hWndParent, hMenu, hInstance, lpParam);
}

BOOL DestroyWindow(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    win32_window_t* parent;
    uint32_t i;
    if (win == NULL) return FALSE;
    if (win->wndproc != NULL) {
        win->wndproc(hWnd, WM_DESTROY, 0, 0);
        win->wndproc(hWnd, WM_NCDESTROY, 0, 0);
    }
    for (i = 0; i < win->child_count; i++) {
        DestroyWindow(win->children[i]);
    }
    parent = win_from_handle(win->parent);
    if (parent != NULL) {
        remove_child_from_parent(parent, hWnd);
    }
    if (g_focus_window == hWnd) g_focus_window = NULL;
    if (g_foreground_window == hWnd) g_foreground_window = NULL;
    win->sig = 0;
    return TRUE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
    win32_window_t* win = win_from_handle(hWnd);
    BOOL prev;
    if (win == NULL) return FALSE;
    prev = win->visible;
    switch (nCmdShow) {
        case SW_HIDE:
            win->visible = FALSE;
            break;
        case SW_SHOW:
        case SW_SHOWNORMAL:
        case SW_SHOWNOACTIVATE:
        case SW_RESTORE:
        case SW_SHOWNA:
            win->visible = TRUE;
            break;
        case SW_SHOWMINIMIZED:
        case SW_MINIMIZE:
        case SW_SHOWMINNOACTIVE:
        case SW_FORCEMINIMIZE:
            win->visible = TRUE;
            break;
        case SW_SHOWMAXIMIZED:
            win->visible = TRUE;
            break;
        case SW_SHOWDEFAULT:
            win->visible = TRUE;
            break;
        default:
            break;
    }
    if (win->visible) {
        InvalidateRect(hWnd, NULL, TRUE);
    }
    return prev;
}

BOOL UpdateWindow(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return FALSE;
    if (win->dirty && win->visible) {
        if (win->wndproc != NULL) {
            win->wndproc(hWnd, WM_PAINT, 0, 0);
        }
        win->dirty = FALSE;
    }
    return TRUE;
}

BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) {
    win32_window_t* win = win_from_handle(hWnd);
    (void)bErase;
    if (win == NULL) return FALSE;
    if (lpRect != NULL) {
        win->dirty_rect = *lpRect;
    } else {
        win->dirty_rect = win->rect;
    }
    win->dirty = TRUE;
    return TRUE;
}

BOOL ValidateRect(HWND hWnd, const RECT* lpRect) {
    win32_window_t* win = win_from_handle(hWnd);
    (void)lpRect;
    if (win == NULL) return FALSE;
    win->dirty = FALSE;
    return TRUE;
}

BOOL GetWindowRect(HWND hWnd, RECT* lpRect) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL || lpRect == NULL) return FALSE;
    *lpRect = win->window_rect;
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, RECT* lpRect) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL || lpRect == NULL) return FALSE;
    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = win->rect.right - win->rect.left;
    lpRect->bottom = win->rect.bottom - win->rect.top;
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, uint32_t uFlags) {
    win32_window_t* win = win_from_handle(hWnd);
    LONG w, h;
    (void)hWndInsertAfter;
    if (win == NULL) return FALSE;
    w = win->window_rect.right - win->window_rect.left;
    h = win->window_rect.bottom - win->window_rect.top;
    if (!(uFlags & SWP_NOMOVE)) {
        win->rect.left += (LONG)X - win->window_rect.left;
        win->rect.top += (LONG)Y - win->window_rect.top;
        win->rect.right += (LONG)X - win->window_rect.left;
        win->rect.bottom += (LONG)Y - win->window_rect.top;
        win->window_rect.left = (LONG)X;
        win->window_rect.top = (LONG)Y;
        win->window_rect.right = (LONG)X + w;
        win->window_rect.bottom = (LONG)Y + h;
    }
    if (!(uFlags & SWP_NOSIZE)) {
        win->window_rect.right = win->window_rect.left + (LONG)cx;
        win->window_rect.bottom = win->window_rect.top + (LONG)cy;
        win->rect.right = win->rect.left + (LONG)cx;
        win->rect.bottom = win->rect.top + (LONG)cy;
    }
    if (uFlags & SWP_SHOWWINDOW) {
        win->visible = TRUE;
    }
    if (uFlags & SWP_HIDEWINDOW) {
        win->visible = FALSE;
    }
    if (!(uFlags & SWP_NOREDRAW)) {
        InvalidateRect(hWnd, NULL, TRUE);
    }
    return TRUE;
}

BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint) {
    return SetWindowPos(hWnd, NULL, X, Y, nWidth, nHeight,
                        bRepaint ? 0 : (uint32_t)SWP_NOREDRAW);
}

BOOL IsWindowVisible(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return FALSE;
    return win->visible;
}

BOOL IsWindow(HWND hWnd) {
    return win_from_handle(hWnd) != NULL;
}

BOOL IsWindowEnabled(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return FALSE;
    return win->enabled;
}

HWND GetParent(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return NULL;
    return win->parent;
}

HWND GetWindow(HWND hWnd, uint32_t uCmd) {
    win32_window_t* win = win_from_handle(hWnd);
    win32_window_t* parent;
    if (win == NULL) return NULL;
    if (uCmd == GW_OWNER) {
        return win->parent;
    }
    if (uCmd == GW_CHILD) {
        if (win->child_count > 0) return win->children[0];
        return NULL;
    }
    parent = win_from_handle(win->parent);
    if (parent != NULL) {
        uint32_t i;
        for (i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == hWnd) {
                if (uCmd == GW_HWNDNEXT) {
                    if (i + 1 < parent->child_count) return parent->children[i + 1];
                    return NULL;
                } else if (uCmd == GW_HWNDPREV) {
                    if (i > 0) return parent->children[i - 1];
                    return NULL;
                }
            }
        }
    }
    return NULL;
}

HWND GetDesktopWindow(void) {
    return (HWND)(uintptr_t)1;
}

HWND GetForegroundWindow(void) {
    return g_foreground_window;
}

BOOL SetForegroundWindow(HWND hWnd) {
    g_foreground_window = hWnd;
    return TRUE;
}

HWND SetActiveWindow(HWND hWnd) {
    g_foreground_window = hWnd;
    return hWnd;
}

BOOL SetFocus(HWND hWnd) {
    HWND prev = g_focus_window;
    win32_window_t* win;
    if (prev != NULL && prev != hWnd) {
        win = win_from_handle(prev);
        if (win != NULL && win->wndproc != NULL) {
            win->wndproc(prev, WM_KILLFOCUS, (WPARAM)hWnd, 0);
        }
    }
    g_focus_window = hWnd;
    win = win_from_handle(hWnd);
    if (win != NULL && win->wndproc != NULL) {
        win->wndproc(hWnd, WM_SETFOCUS, (WPARAM)prev, 0);
    }
    return TRUE;
}

HWND GetFocus(void) {
    return g_focus_window;
}

HWND FindWindowA(const char* lpClassName, const char* lpWindowName) {
    int32_t i;
    for (i = 0; i < (int32_t)WINDOWS_MAX; i++) {
        if (g_windows[i].sig != WND_SIG) continue;
        if (lpClassName != NULL && strcmp(g_windows[i].class_name, lpClassName) != 0) continue;
        if (lpWindowName != NULL && strcmp(g_windows[i].window_name, lpWindowName) != 0) continue;
        return g_windows[i].handle;
    }
    return NULL;
}

HWND FindWindowExA(HWND hWndParent, HWND hWndChildAfter, const char* lpszClass, const char* lpszWindow) {
    win32_window_t* parent = win_from_handle(hWndParent);
    uint32_t i;
    int found_start = 0;
    if (parent == NULL) return NULL;
    for (i = 0; i < parent->child_count; i++) {
        HWND child = parent->children[i];
        win32_window_t* child_win;
        if (!found_start) {
            if (hWndChildAfter == NULL) found_start = 1;
            else if (child == hWndChildAfter) found_start = 1;
            continue;
        }
        child_win = win_from_handle(child);
        if (child_win == NULL) continue;
        if (lpszClass != NULL && strcmp(child_win->class_name, lpszClass) != 0) continue;
        if (lpszWindow != NULL && strcmp(child_win->window_name, lpszWindow) != 0) continue;
        return child;
    }
    return NULL;
}

BOOL SetWindowTextA(HWND hWnd, const char* lpString) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return FALSE;
    if (lpString != NULL) {
        strncpy(win->window_name, lpString, sizeof(win->window_name) - 1);
        win->window_name[sizeof(win->window_name) - 1] = '\0';
    } else {
        win->window_name[0] = '\0';
    }
    InvalidateRect(hWnd, NULL, TRUE);
    return TRUE;
}

int GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount) {
    win32_window_t* win = win_from_handle(hWnd);
    size_t len;
    if (win == NULL || lpString == NULL || nMaxCount <= 0) return 0;
    len = strlen(win->window_name);
    if (len >= (size_t)nMaxCount) len = (size_t)(nMaxCount - 1);
    memcpy(lpString, win->window_name, len);
    lpString[len] = '\0';
    return (int)len;
}

int GetWindowTextLengthA(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return 0;
    return (int)strlen(win->window_name);
}

BOOL SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return FALSE;
    switch (nIndex) {
        case GWLP_WNDPROC:
            win->wndproc = (WNDPROC)dwNewLong;
            break;
        case GWLP_HINSTANCE:
            win->hinstance = (HINSTANCE)dwNewLong;
            break;
        case GWLP_USERDATA:
            win->user_data = (void*)dwNewLong;
            break;
        case GWL_STYLE:
            win->dwStyle = (uint32_t)dwNewLong;
            break;
        case GWL_EXSTYLE:
            win->dwExStyle = (uint32_t)dwNewLong;
            break;
        case GWLP_ID:
            win->menu = (HMENU)dwNewLong;
            break;
        default:
            if (nIndex >= 0 && (size_t)nIndex < sizeof(win->extra_bytes) - sizeof(LONG_PTR)) {
                memcpy(&win->extra_bytes[nIndex], &dwNewLong, sizeof(LONG_PTR));
            }
            break;
    }
    return TRUE;
}

LONG_PTR GetWindowLongPtrA(HWND hWnd, int nIndex) {
    win32_window_t* win = win_from_handle(hWnd);
    LONG_PTR result = 0;
    if (win == NULL) return 0;
    switch (nIndex) {
        case GWLP_WNDPROC:
            result = (LONG_PTR)win->wndproc;
            break;
        case GWLP_HINSTANCE:
            result = (LONG_PTR)win->hinstance;
            break;
        case GWLP_USERDATA:
            result = (LONG_PTR)win->user_data;
            break;
        case GWL_STYLE:
            result = (LONG_PTR)win->dwStyle;
            break;
        case GWL_EXSTYLE:
            result = (LONG_PTR)win->dwExStyle;
            break;
        case GWLP_ID:
            result = (LONG_PTR)win->menu;
            break;
        default:
            if (nIndex >= 0 && (size_t)nIndex < sizeof(win->extra_bytes) - sizeof(LONG_PTR)) {
                memcpy(&result, &win->extra_bytes[nIndex], sizeof(LONG_PTR));
            }
            break;
    }
    return result;
}

BOOL EnumWindows(BOOL (*lpEnumFunc)(HWND, LPARAM), LPARAM lParam) {
    int32_t i;
    if (lpEnumFunc == NULL) return FALSE;
    for (i = 0; i < (int32_t)WINDOWS_MAX; i++) {
        if (g_windows[i].sig != WND_SIG) continue;
        if ((uintptr_t)g_windows[i].handle == 1) continue;
        if (!lpEnumFunc(g_windows[i].handle, lParam)) return FALSE;
    }
    return TRUE;
}

BOOL EnumChildWindows(HWND hWndParent, BOOL (*lpEnumFunc)(HWND, LPARAM), LPARAM lParam) {
    win32_window_t* parent = win_from_handle(hWndParent);
    uint32_t i;
    if (lpEnumFunc == NULL || parent == NULL) return FALSE;
    for (i = 0; i < parent->child_count; i++) {
        if (!lpEnumFunc(parent->children[i], lParam)) return FALSE;
    }
    return TRUE;
}

BOOL GetWindowInfo(HWND hWnd, void* pwi) {
    win32_window_t* win = win_from_handle(hWnd);
    WINDOWINFO* info = (WINDOWINFO*)pwi;
    if (win == NULL || info == NULL) return FALSE;
    info->rcWindow = win->window_rect;
    info->rcClient = win->rect;
    info->dwStyle = win->dwStyle;
    info->dwExStyle = win->dwExStyle;
    info->cxWindowBorders = 1;
    info->cyWindowBorders = 1;
    info->atomWindowType = 0;
    info->wCreatorVersion = 0;
    return TRUE;
}

BOOL BringWindowToTop(HWND hWnd) {
    win32_window_t* win = win_from_handle(hWnd);
    win32_window_t* parent;
    uint32_t i, pos;
    if (win == NULL) return FALSE;
    parent = win_from_handle(win->parent);
    if (parent == NULL) return FALSE;
    pos = (uint32_t)-1;
    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == hWnd) {
            pos = i;
            break;
        }
    }
    if (pos == (uint32_t)-1) return FALSE;
    for (i = pos; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->children[parent->child_count - 1] = hWnd;
    g_foreground_window = hWnd;
    return TRUE;
}

static BOOL msg_enqueue(const MSG* msg) {
    if (g_msg_queue.count >= MSG_QUEUE_MAX) return FALSE;
    g_msg_queue.messages[g_msg_queue.tail] = *msg;
    g_msg_queue.tail = (g_msg_queue.tail + 1) % MSG_QUEUE_MAX;
    g_msg_queue.count++;
    return TRUE;
}

static BOOL msg_dequeue(MSG* msg, BOOL filter_by_hwnd, HWND hwnd_filter,
                         UINT minMsg, UINT maxMsg, BOOL remove) {
    uint32_t i, idx;
    if (g_msg_queue.count == 0) return FALSE;
    for (i = 0; i < g_msg_queue.count; i++) {
        idx = (g_msg_queue.head + i) % MSG_QUEUE_MAX;
        if (filter_by_hwnd && hwnd_filter != NULL && g_msg_queue.messages[idx].hwnd != hwnd_filter) continue;
        if (minMsg != 0 && g_msg_queue.messages[idx].message < minMsg) continue;
        if (maxMsg != 0 && g_msg_queue.messages[idx].message > maxMsg) continue;
        *msg = g_msg_queue.messages[idx];
        if (remove) {
            uint32_t j;
            for (j = i; j < g_msg_queue.count - 1; j++) {
                uint32_t cur = (g_msg_queue.head + j) % MSG_QUEUE_MAX;
                uint32_t next = (g_msg_queue.head + j + 1) % MSG_QUEUE_MAX;
                g_msg_queue.messages[cur] = g_msg_queue.messages[next];
            }
            g_msg_queue.tail = (g_msg_queue.tail - 1 + MSG_QUEUE_MAX) % MSG_QUEUE_MAX;
            g_msg_queue.count--;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL GetMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    if (lpMsg == NULL) return FALSE;
    while (1) {
        if (g_msg_queue.quit_posted) {
            lpMsg->hwnd = NULL;
            lpMsg->message = WM_QUIT;
            lpMsg->wParam = (WPARAM)g_msg_queue.quit_code;
            lpMsg->lParam = 0;
            lpMsg->time = 0;
            lpMsg->pt.x = 0;
            lpMsg->pt.y = 0;
            return FALSE;
        }
        if (msg_dequeue(lpMsg, (BOOL)(hWnd != NULL), hWnd, wMsgFilterMin, wMsgFilterMax, TRUE)) {
            return TRUE;
        }
    }
}

BOOL PeekMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    if (lpMsg == NULL) return FALSE;
    if (g_msg_queue.quit_posted) {
        lpMsg->hwnd = NULL;
        lpMsg->message = WM_QUIT;
        lpMsg->wParam = (WPARAM)g_msg_queue.quit_code;
        lpMsg->lParam = 0;
        lpMsg->time = 0;
        lpMsg->pt.x = 0;
        lpMsg->pt.y = 0;
        return TRUE;
    }
    return msg_dequeue(lpMsg, (BOOL)(hWnd != NULL), hWnd, wMsgFilterMin, wMsgFilterMax,
                       (wRemoveMsg & PM_REMOVE) ? TRUE : FALSE);
}

BOOL TranslateMessage(const MSG* lpMsg) {
    if (lpMsg == NULL) return FALSE;
    if (lpMsg->message == WM_KEYDOWN) {
        WPARAM key = lpMsg->wParam;
        MSG char_msg;
        WPARAM ch = 0;
        if (key >= (WPARAM)0x20 && key < (WPARAM)0x7F) {
            ch = key;
        } else if (key == (WPARAM)KEY_RETURN) {
            ch = (WPARAM)'\r';
        } else if (key == (WPARAM)KEY_BACKSPACE) {
            ch = (WPARAM)KEY_BACKSPACE;
        } else if (key == (WPARAM)KEY_TAB) {
            ch = (WPARAM)'\t';
        } else if (key == (WPARAM)KEY_ENTER) {
            ch = (WPARAM)'\r';
        }
        if (ch != 0) {
            char_msg = *lpMsg;
            char_msg.message = WM_CHAR;
            char_msg.wParam = ch;
            msg_enqueue(&char_msg);
        }
    }
    return TRUE;
}

LRESULT DispatchMessageA(const MSG* lpMsg) {
    win32_window_t* win;
    if (lpMsg == NULL) return 0;
    if (lpMsg->hwnd == NULL) return 0;
    win = win_from_handle(lpMsg->hwnd);
    if (win == NULL || win->wndproc == NULL) return 0;
    return win->wndproc(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

void PostQuitMessage(int nExitCode) {
    g_msg_queue.quit_posted = TRUE;
    g_msg_queue.quit_code = nExitCode;
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    MSG msg;
    memset(&msg, 0, sizeof(msg));
    msg.hwnd = hWnd;
    msg.message = Msg;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.pt.x = (LONG)g_mouse_global_x;
    msg.pt.y = (LONG)g_mouse_global_y;
    return msg_enqueue(&msg);
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    win32_window_t* win = win_from_handle(hWnd);
    if (win == NULL) return 0;
    if (win->wndproc == NULL) return 0;
    return win->wndproc(hWnd, Msg, wParam, lParam);
}

BOOL WaitMessage(void) {
    while (g_msg_queue.count == 0 && !g_msg_queue.quit_posted) {
        volatile uint32_t i;
        for (i = 0; i < 10000; i++) {}
    }
    return TRUE;
}

BOOL ReplyMessage(LRESULT lResult) {
    (void)lResult;
    return TRUE;
}

BOOL GetInputState(void) {
    return g_msg_queue.count > 0 ? TRUE : FALSE;
}

void gui_keyboard_to_win32(const key_event_t* event) {
    MSG msg;
    HWND target;
    if (event == NULL) return;
    target = g_focus_window;
    if (target == NULL) target = g_foreground_window;
    memset(&msg, 0, sizeof(msg));
    msg.hwnd = target;
    msg.wParam = (WPARAM)event->keycode;
    msg.lParam = 0;
    if (event->pressed) {
        msg.message = WM_KEYDOWN;
    } else {
        msg.message = WM_KEYUP;
    }
    if (event->pressed && event->ascii != '\0') {
        MSG char_msg = msg;
        char_msg.message = WM_CHAR;
        char_msg.wParam = (WPARAM)event->ascii;
        msg_enqueue(&char_msg);
    }
    if (target != NULL) {
        msg_enqueue(&msg);
    }
}

void gui_mouse_to_win32(const mouse_packet_t* packet) {
    static uint8_t last_buttons = 0;
    MSG msg;
    HWND target;
    LPARAM lparam;
    int32_t x, y;
    if (packet == NULL) return;
    g_mouse_global_x += packet->dx;
    g_mouse_global_y += packet->dy;
    if (g_mouse_global_x < 0) g_mouse_global_x = 0;
    if (g_mouse_global_y < 0) g_mouse_global_y = 0;
    if (g_mouse_global_x > (int32_t)fb.width - 1) g_mouse_global_x = (int32_t)fb.width - 1;
    if (g_mouse_global_y > (int32_t)fb.height - 1) g_mouse_global_y = (int32_t)fb.height - 1;
    target = g_foreground_window;
    x = g_mouse_global_x;
    y = g_mouse_global_y;
    lparam = (LPARAM)(((uint32_t)y << 16) | (uint16_t)x);
    memset(&msg, 0, sizeof(msg));
    msg.hwnd = target;
    msg.pt.x = (LONG)x;
    msg.pt.y = (LONG)y;
    msg.message = WM_MOUSEMOVE;
    msg.wParam = 0;
    msg.lParam = lparam;
    if (packet->buttons & MOUSE_BTN_LEFT) msg.wParam |= 0x0001;
    if (packet->buttons & MOUSE_BTN_RIGHT) msg.wParam |= 0x0002;
    if (packet->buttons & MOUSE_BTN_MIDDLE) msg.wParam |= 0x0010;
    if (target != NULL) {
        msg_enqueue(&msg);
    }
    if ((packet->buttons & MOUSE_BTN_LEFT) && !(last_buttons & MOUSE_BTN_LEFT)) {
        msg.message = WM_LBUTTONDOWN;
        if (target != NULL) msg_enqueue(&msg);
    }
    if (!(packet->buttons & MOUSE_BTN_LEFT) && (last_buttons & MOUSE_BTN_LEFT)) {
        msg.message = WM_LBUTTONUP;
        if (target != NULL) msg_enqueue(&msg);
    }
    if ((packet->buttons & MOUSE_BTN_RIGHT) && !(last_buttons & MOUSE_BTN_RIGHT)) {
        msg.message = WM_RBUTTONDOWN;
        if (target != NULL) msg_enqueue(&msg);
    }
    if (!(packet->buttons & MOUSE_BTN_RIGHT) && (last_buttons & MOUSE_BTN_RIGHT)) {
        msg.message = WM_RBUTTONUP;
        if (target != NULL) msg_enqueue(&msg);
    }
    if ((packet->buttons & MOUSE_BTN_MIDDLE) && !(last_buttons & MOUSE_BTN_MIDDLE)) {
        msg.message = WM_MBUTTONDOWN;
        if (target != NULL) msg_enqueue(&msg);
    }
    if (!(packet->buttons & MOUSE_BTN_MIDDLE) && (last_buttons & MOUSE_BTN_MIDDLE)) {
        msg.message = WM_MBUTTONUP;
        if (target != NULL) msg_enqueue(&msg);
    }
    if (packet->wheel != 0) {
        msg.message = WM_MOUSEWHEEL;
        msg.wParam = (WPARAM)(((uint16_t)(packet->wheel * 120)) << 16);
        if (target != NULL) msg_enqueue(&msg);
    }
    last_buttons = packet->buttons;
}
