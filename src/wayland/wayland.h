/*
 * Kenux OS - Wayland Display Server + Xorg X11 Server (Minimal)
 * Shared header for both display servers
 */

#ifndef _WAYLAND_XORG_H
#define _WAYLAND_XORG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef _WIN32
/* MinGW 兼容: 提供 uid_t/gid_t 定义 (pid_t/ssize_t 已由 MinGW 提供) */
typedef int uid_t;
typedef int gid_t;
/* mmap stubs */
#define PROT_READ    1
#define PROT_WRITE   2
#define MAP_PRIVATE  2
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED   ((void*)-1)
static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, long off) {
    (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)off; return MAP_FAILED;
}
static inline int munmap(void *addr, size_t len) { (void)addr; (void)len; return -1; }
/* getuid/getgid 不存在 */
static inline int getuid(void) { return 0; }
static inline int getgid(void) { return 0; }
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

/* ---------------------- Common ---------------------- */

#define WL_VERSION_STR "KenuxK-Wayland 1.22 + XWayland (Minimal)"
#define XORG_VERSION_STR "KenuxK-Xorg X11R7.9 Server 21.1 (Minimal)"

#define DISP_MAX_OUTPUTS 16
#define DISP_MAX_CLIENTS 512
#define DISP_MAX_WINDOWS 8192
#define DISP_MAX_INPUTS 32
#define DISP_MAX_BUFFERS 4096
#define DISP_MAX_GLOBALS 64

typedef enum {
    DS_FMT_ARGB8888 = 0,
    DS_FMT_XRGB8888,
    DS_FMT_RGB565,
    DS_FMT_BGRA8888,
    DS_FMT_ABGR8888,
    DS_FMT_YUYV,
    DS_FMT_NV12,
    DS_FMT_YUV420,
} DsPixelFormat;

typedef enum {
    DS_TRANS_NONE = 0,       /* none */
    DS_TRANS_GBM,            /* GBM (GPU buffer manager) */
    DS_TRANS_DRM,            /* direct DRM/KMS dumb buffer */
    DS_TRANS_SHM,            /* shared memory / wl_shm */
    DS_TRANS_DMA_BUF,        /* dma-buf (PRIME) */
    DS_TRANS_WAYLAND,        /* nested wayland client (XWayland / Weston-on-Wayland) */
    DS_TRANS_X11,            /* X11 backend (nested) */
    DS_TRANS_FBDEV,          /* legacy /dev/fb0 */
    DS_TRANS_VNC,            /* VNC remote */
    DS_TRANS_HEADLESS,       /* headless / RDP only */
} DsBackendType;

typedef struct {
    int fd;
    int width;
    int height;
    int stride;
    DsPixelFormat format;
    void *mmap_ptr;
    size_t mmap_size;
    uint32_t handle;       /* gem or dma-buf handle */
    uint32_t id;
    int busy;
    int refcnt;
} DsBuffer;

typedef struct {
    int x, y;
    int width, height;
} DsRect;

typedef struct DsWindow {
    uint32_t id;
    uint32_t parent_id;
    int x, y;
    int w, h;
    int border;
    uint32_t background;
    DsPixelFormat visual;
    int override_redirect;
    int mapped;
    int minimized;
    int maximized;
    int fullscreen;
    int tiled_left, tiled_right, tiled_top, tiled_bottom;
    int always_on_top;
    int sticky;
    int accept_focus;
    char title[512];
    char app_id[256];
    char wm_class[256];
    char wm_instance[256];
    char wm_role[128];
    uint32_t client_id;
    uint32_t surface_id;
    uint32_t *children;
    int child_count;
    int child_cap;
    DsBuffer *front_buffer;
    DsBuffer *back_buffer;
    int damage_pending;
    DsRect damage;
    int opacity;        /* 0..255 */
    int has_alpha;
    /* decorations */
    int csd_enabled;     /* client side decorations */
    int have_ssd;        /* server side decorations */
    /* X11 specific */
    uint32_t event_mask;
    uint32_t wm_state;   /* Normal, Iconic, etc. */
    int depth;
    /* Wayland specific */
    int wl_role_set;
    struct {
        int min_w, min_h, max_w, max_h;
        int bw, bh;       /* base size */
        int resize_inc_w, resize_inc_h;
        float min_aspect, max_aspect;
        int wm_window_geometry_set;
    } xdg;
    struct DsWindow *next;
} DsWindow;

/* Output (monitor) */
typedef struct {
    int enabled;
    int width_mm, height_mm;   /* physical size in millimeters */
    int x, y;                  /* global position in compositor space */
    int width, height;         /* current mode pixels */
    int refresh_mhz;           /* e.g. 60000 = 60Hz */
    int scale;                 /* integer scale 1,2,3 */
    char manufacturer[64];
    char model[64];
    char serial[64];
    char connector[64];        /* HDMI-A-1, DP-2, eDP-1, etc. */
    int backlight_pct;         /* 0..100 */
    /* Supported modes (minimal) */
    int supported_modes[32][3]; /* [w,h,hz] */
    int mode_count;
    int current_mode;
    int transform;             /* WL_OUTPUT_TRANSFORM */
    char name[128];            /* e.g. "DP-1" */
    char description[256];     /* e.g. "Dell Inc. DELL U2720Q 12345 via DP-1" */
} DsOutput;

/* Input devices */
typedef enum {
    DS_INPUT_POINTER = 0,
    DS_INPUT_KEYBOARD,
    DS_INPUT_TOUCH,
    DS_INPUT_TABLET_TOOL,
    DS_INPUT_TABLET_PAD,
    DS_INPUT_GESTURE,
    DS_INPUT_SWITCH,
    DS_INPUT_JOYSTICK,
} DsInputType;

typedef struct {
    DsInputType type;
    char name[128];
    char devnode[128];         /* /dev/input/eventX */
    int vendor, product;
    int id;
    /* Pointer */
    int ptr_x, ptr_y;
    int ptr_button_state;      /* bit 0..15 down? */
    /* Keyboard */
    int keymap_fd;
    size_t keymap_size;
    char *keymap;
    char xkb_rules[64], xkb_model[64], xkb_layout[64], xkb_variant[64], xkb_options[128];
    int led_state;             /* 1 = Num, 2 = Caps, 4 = Scroll */
    int modifiers_depressed;
    int modifiers_latched;
    int modifiers_locked;
    int group;
    /* Touch */
    int max_touches;
    struct { int tid; int x, y; int active; } touches[16];
} DsInputDevice;

typedef enum {
    DS_SEAT_POINTER_CAP = 1,
    DS_SEAT_KEYBOARD_CAP = 2,
    DS_SEAT_TOUCH_CAP = 4,
    DS_SEAT_TABLET_CAP = 8,
} DsSeatCaps;

typedef struct DsClient {
    uint32_t id;
    int socket_fd;
    char app_id[256];
    char binary_path[1024];
    pid_t pid;
    uid_t uid;
    gid_t gid;
    int connected;
    int is_xwayland;          /* X11 client translated via XWayland */
    uint32_t focus_window;
    /* Resources */
    uint32_t *resource_ids;
    int resource_count;
    int resource_cap;
    /* Permissions */
    int can_access_seat;
    int can_copy_paste;
    int can_screenshot;
    int can_screen_share;
    struct DsClient *next;
} DsClient;

/* Clipboard / DnD selection */
typedef enum {
    DS_SEL_CLIPBOARD = 0,
    DS_SEL_PRIMARY,
    DS_SEL_SECONDARY,
    DS_SEL_DND,
} DsSelectionKind;

#define DS_MAX_MIME_TYPES 32
typedef struct {
    char mime_types[DS_MAX_MIME_TYPES][256];
    int mime_count;
    uint32_t offer_client;
    uint32_t offer_serial;
    DsSelectionKind kind;
    uint8_t *buffer;
    size_t buffer_len;
} DsSelection;

/* DRM / graphics state */
typedef struct {
    int drm_fd;
    int card_idx;
    char render_node[64];     /* /dev/dri/renderD128 */
    char card_node[64];       /* /dev/dri/card0 */
    int gbm_dev;              /* opaque handle */
    uint32_t crtc_id[DISP_MAX_OUTPUTS];
    uint32_t connector_id[DISP_MAX_OUTPUTS];
    uint32_t encoder_id[DISP_MAX_OUTPUTS];
    uint32_t plane_id[DISP_MAX_OUTPUTS];
    int has_modifiers;
    uint64_t preferred_modifier;
} DsDrmState;

/* Renderer / composite */
typedef enum {
    DS_RENDERER_PIXMAN = 0,    /* CPU/pixman (software fallback) */
    DS_RENDERER_GL,            /* OpenGL(ES) via EGL */
    DS_RENDERER_VK,            /* Vulkan renderer */
    DS_RENDERER_DUMB,          /* dumb buffer / no composite */
} DsRendererType;

typedef struct {
    DsRendererType type;
    int egl_display;
    int egl_context;
    int vk_device;
    int textures_valid;
    int max_texture_size;
} DsRendererState;

/* ------------------------- Wayland ------------------------- */

typedef enum {
    WL_ROLE_NONE = 0,
    WL_ROLE_XDG_TOPLEVEL,
    WL_ROLE_XDG_POPUP,
    WL_ROLE_XDG_POSITIONER,
    WL_ROLE_WL_SUBSURFACE,
    WL_ROLE_WL_CURSOR,
    WL_ROLE_WL_SHELL_SURFACE,  /* legacy */
    WL_ROLE_IVI_SURFACE,
    WL_ROLE_FULLSCREEN_SHELL,
} WaylandRole;

typedef struct {
    const char *name;
    uint32_t version;
    uint32_t id;
    int (*bind)(DsClient *c, uint32_t id, uint32_t version);
} WaylandGlobal;

typedef struct {
    /* core objects */
    uint32_t wl_display_id;
    uint32_t wl_registry_id;
    WaylandGlobal globals[DISP_MAX_GLOBALS];
    int global_count;
    /* seats */
    uint32_t wl_seat_id;
    char seat_name[256];
    int seat_caps;
    /* selection */
    DsSelection selections[4];
    /* data_device_manager (clipboard/DnD) */
    uint32_t ddm_id;
    /* xdg_wm_base */
    uint32_t xdg_wm_id;
    /* layer_shell */
    uint32_t layer_shell_id;
    /* zwp_input_method */
    int input_method_enabled;
    /* zwp_virtual_keyboard */
    int virtual_keyboard_enabled;
    /* fractional_scale_v1 */
    int fractional_scale_enabled;
    /* viewporter */
    int viewporter_enabled;
    /* cursor-shape-v1 */
    int cursor_shape_enabled;
    /* security context v1 */
    int security_context_enabled;
} WaylandServerState;

/* ------------------------- Xorg X11 ------------------------- */

/* X11 Core protocol codes (subset) */
#define X_REQUEST_CREATE_WINDOW   1
#define X_REQUEST_CHANGE_WINDOW_ATTRS 2
#define X_REQUEST_GET_WINDOW_ATTRS 3
#define X_REQUEST_DESTROY_WINDOW  4
#define X_REQUEST_DESTROY_SUBWINDOWS 5
#define X_REQUEST_UNMAP_WINDOW    10
#define X_REQUEST_MAP_WINDOW      8
#define X_REQUEST_CONFIGURE_WINDOW 12
#define X_REQUEST_MOVE_WINDOW     100
#define X_REQUEST_RESIZE_WINDOW   101
#define X_REQUEST_CREATE_GC       55
#define X_REQUEST_FREE_GC         56
#define X_REQUEST_GET_GEOMETRY    14
#define X_REQUEST_QUERY_TREE      15
#define X_REQUEST_PUT_IMAGE       72
#define X_REQUEST_GET_IMAGE       73
#define X_REQUEST_COPY_AREA       62
#define X_REQUEST_POLY_POINT      64
#define X_REQUEST_POLY_LINE       65
#define X_REQUEST_POLY_SEGMENT    66
#define X_REQUEST_POLY_RECTANGLE  67
#define X_REQUEST_POLY_ARC        68
#define X_REQUEST_FILL_POLY       69
#define X_REQUEST_POLY_FILL_RECT  70
#define X_REQUEST_POLY_FILL_ARC   71
#define X_REQUEST_INTERN_ATOM     16
#define X_REQUEST_GET_ATOM_NAME   17
#define X_REQUEST_CHANGE_PROPERTY 18
#define X_REQUEST_DELETE_PROPERTY 19
#define X_REQUEST_GET_PROPERTY    20
#define X_REQUEST_LIST_PROPERTIES 21
#define X_REQUEST_SET_SELECTION_OWNER 22
#define X_REQUEST_GET_SELECTION_OWNER 23
#define X_REQUEST_CONVERT_SELECTION 24
#define X_REQUEST_SEND_EVENT      25
#define X_REQUEST_GRAB_POINTER    26
#define X_REQUEST_UNGRAB_POINTER  27
#define X_REQUEST_GRAB_BUTTON     28
#define X_REQUEST_UNGRAB_BUTTON   29
#define X_REQUEST_GRAB_KEYBOARD   31
#define X_REQUEST_UNGRAB_KEYBOARD 32
#define X_REQUEST_GRAB_KEY        33
#define X_REQUEST_UNGRAB_KEY      34
#define X_REQUEST_QUERY_POINTER   38
#define X_REQUEST_TRANSLATE_COORDS 40
#define X_REQUEST_SET_INPUT_FOCUS 42
#define X_REQUEST_GET_INPUT_FOCUS 43
#define X_REQUEST_QUERY_KEYMAP    102
#define X_REQUEST_LOOKUP_COLOR    89
#define X_REQUEST_ALLOC_COLOR     88
#define X_REQUEST_QUERY_EXTENSION 98
#define X_REQUEST_LIST_EXTENSIONS 99
#define X_REQUEST_FORCE_SCREENSAVER 115
#define X_REQUEST_SET_SCREENSAVER 107
#define X_REQUEST_KILL_CLIENT     113
#define X_REQUEST_SET_WM_BITS     0x3F

#define X_EVENT_KEY_PRESS        2
#define X_EVENT_KEY_RELEASE      3
#define X_EVENT_BUTTON_PRESS     4
#define X_EVENT_BUTTON_RELEASE   5
#define X_EVENT_MOTION_NOTIFY    6
#define X_EVENT_ENTER_NOTIFY     7
#define X_EVENT_LEAVE_NOTIFY     8
#define X_EVENT_FOCUS_IN         9
#define X_EVENT_FOCUS_OUT        10
#define X_EVENT_EXPOSE           12
#define X_EVENT_CONFIGURE_NOTIFY 22
#define X_EVENT_MAP_NOTIFY       19
#define X_EVENT_UNMAP_NOTIFY     18
#define X_EVENT_DESTROY_NOTIFY   17
#define X_EVENT_CREATE_NOTIFY    16
#define X_EVENT_PROPERTY_NOTIFY  28
#define X_EVENT_SELECTION_REQUEST 30
#define X_EVENT_SELECTION_NOTIFY  31
#define X_EVENT_SELECTION_CLEAR   29
#define X_EVENT_CLIENT_MESSAGE   33

/* Standard atoms */
typedef enum {
    XATOM_NONE = 0,
    XATOM_WM_PROTOCOLS,
    XATOM_WM_DELETE_WINDOW,
    XATOM_WM_TAKE_FOCUS,
    XATOM_WM_SAVE_YOURSELF,
    XATOM_WM_STATE,
    XATOM_WM_CHANGE_STATE,
    XATOM_WM_CLASS,
    XATOM_WM_TRANSIENT_FOR,
    XATOM_WM_GEOMETRY,
    XATOM_WM_HINTS,
    XATOM_WM_NORMAL_HINTS,
    XATOM_WM_SIZE_HINTS,
    XATOM_WM_NAME,
    XATOM_WM_ICON_NAME,
    XATOM_WM_CLIENT_MACHINE,
    XATOM_WM_COMMAND,
    XATOM_WM_CLIENT_LEADER,
    XATOM_WM_WINDOW_ROLE,
    XATOM__NET_WM_NAME,
    XATOM__NET_WM_ICON_NAME,
    XATOM__NET_SUPPORTED,
    XATOM__NET_SUPPORTING_WM_CHECK,
    XATOM__NET_WM_PID,
    XATOM__NET_WM_STATE,
    XATOM__NET_WM_STATE_MAXIMIZED_VERT,
    XATOM__NET_WM_STATE_MAXIMIZED_HORZ,
    XATOM__NET_WM_STATE_FULLSCREEN,
    XATOM__NET_WM_STATE_MODAL,
    XATOM__NET_WM_STATE_STICKY,
    XATOM__NET_WM_STATE_ABOVE,
    XATOM__NET_WM_STATE_BELOW,
    XATOM__NET_WM_STATE_DEMANDS_ATTENTION,
    XATOM__NET_WM_STATE_HIDDEN,
    XATOM__NET_WM_STATE_SKIP_TASKBAR,
    XATOM__NET_WM_STATE_SKIP_PAGER,
    XATOM__NET_ACTIVE_WINDOW,
    XATOM__NET_CLOSE_WINDOW,
    XATOM__NET_WM_MOVERESIZE,
    XATOM__NET_WM_FULLSCREEN_MONITORS,
    XATOM__NET_FRAME_EXTENTS,
    XATOM__NET_WM_ALLOWED_ACTIONS,
    XATOM__NET_WM_WINDOW_TYPE,
    XATOM__NET_WM_WINDOW_TYPE_DESKTOP,
    XATOM__NET_WM_WINDOW_TYPE_DOCK,
    XATOM__NET_WM_WINDOW_TYPE_TOOLBAR,
    XATOM__NET_WM_WINDOW_TYPE_MENU,
    XATOM__NET_WM_WINDOW_TYPE_UTILITY,
    XATOM__NET_WM_WINDOW_TYPE_SPLASH,
    XATOM__NET_WM_WINDOW_TYPE_DIALOG,
    XATOM__NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    XATOM__NET_WM_WINDOW_TYPE_POPUP_MENU,
    XATOM__NET_WM_WINDOW_TYPE_TOOLTIP,
    XATOM__NET_WM_WINDOW_TYPE_NOTIFICATION,
    XATOM__NET_WM_WINDOW_TYPE_COMBO,
    XATOM__NET_WM_WINDOW_TYPE_DND,
    XATOM__NET_WM_WINDOW_TYPE_NORMAL,
    XATOM__NET_SYSTEM_TRAY_OPCODE,
    XATOM__NET_SYSTEM_TRAY_VISUAL,
    XATOM__NET_SYSTEM_TRAY_ORIENTATION,
    XATOM_UTF8_STRING,
    XATOM_WM_S0,        /* XA_CUT_BUFFER0 */
    XATOM_CLIPBOARD,
    XATOM_PRIMARY,
    XATOM_TARGETS,
    XATOM_TIMESTAMP,
    XATOM_TEXT,
    XATOM_STRING,
    XATOM_INTEGER,
    XATOM_ATOM_PAIR,
    XATOM_WINDOW,
    XATOM_BITMAP,
    XATOM_PIXMAP,
    XATOM_CURSOR,
    XATOM_COLOR,
    XATOM_COLORMAP,
    XATOM_VISUALID,
    /* ... */
    XATOM_MAX_COUNT
} X11Atom;

/* X extension (subset) */
typedef enum {
    XEXT_SHAPE = 0,
    XEXT_RENDER,
    XEXT_RANDR,
    XEXT_XINERAMA,
    XEXT_DAMAGE,
    XEXT_COMPOSITE,
    XEXT_FIXES,
    XEXT_DPMS,
    XEXT_GLX,
    XEXT_DRI2,
    XEXT_DRI3,
    XEXT_PRESENT,
    XEXT_XINPUT2,
    XEXT_XKB,
    XEXT_SYNC,
    XEXT_BIGREQ,
    XEXT_RECORD,
    XEXT_SCREENSAVER,
    XEXT_SECURITY,
    XEXT_SELinux,
    XEXT_MITSHM,
    XEXT_DOUBLE_BUFFER,
    XEXT_MAX_COUNT
} X11Extension;

typedef struct {
    int major_version;
    int minor_version;
    int opcode;
    int event_base;
    int error_base;
    int enabled;
} X11ExtensionState;

typedef struct {
    /* display */
    int display_width, display_height;
    int root_depth;
    uint32_t root_visual;
    uint32_t root_window;
    int screen_count;
    int default_screen;

    /* XWayland bridge */
    int xwayland_fd[2];         /* socket pair */
    pid_t xwayland_pid;
    int xwayland_started;
    char xwayland_socket_path[256];  /* unix socket /tmp/.X11-unix/X0 */
    char xauth_cookie[32];
    int display_number;         /* :0, :1 etc. */
    int vt;                     /* virtual terminal, e.g. 2 */

    /* XKB/XDG_RUNTIME_DIR */
    char runtime_dir[512];
    char xkb_path[512];
    char font_path[2048];
    char xinitrc[1024];

    /* auth */
    char xauthority[1024];

    /* WM selection ownership */
    int has_wm;                 /* has a window manager running? */
    uint32_t wm_check_window;
    uint32_t wm_pid;

    /* Root properties (atoms + values) */
    uint32_t root_wallpaper_pixmap;
    uint32_t root_bg_pixel;
    /* Extensions state */
    X11ExtensionState ext[XEXT_MAX_COUNT];
    /* Resources */
    uint64_t resource_serial;
    /* Input grab */
    int pointer_grab_active;
    uint32_t pointer_grab_window;
    uint32_t pointer_grab_owner_events;
    int keyboard_grab_active;
    uint32_t keyboard_grab_window;
    /* Atoms */
    uint32_t atom_id_counter;
    struct {
        uint32_t id;
        char name[512];
    } *atom_table;
    int atom_count;
    int atom_cap;

    /* X11 event queue */
    uint8_t *event_queue;
    size_t event_queue_len;
    size_t event_queue_cap;

    /* X client state (minimal: we are both server + WM) */
    DsClient *clients_head;
} XorgServerState;

/* ---------------------- Unified server state ---------------------- */

typedef struct {
    /* Which display server we are */
    int run_wayland;
    int run_x11;
    int run_xwayland;          /* launch Xwayland as Wayland socket */
    int run_nested;            /* run as nested compositor */

    /* Primary backend */
    DsBackendType backend;

    /* Outputs / monitors */
    DsOutput outputs[DISP_MAX_OUTPUTS];
    int output_count;

    /* Inputs */
    DsInputDevice inputs[DISP_MAX_INPUTS];
    int input_count;
    char primary_seat[256];

    /* Clients */
    DsClient *clients;          /* head of linked list */
    int client_count;

    /* Windows */
    DsWindow *windows[DISP_MAX_WINDOWS];
    uint32_t window_id_counter;

    /* Buffers */
    DsBuffer *buffers[DISP_MAX_BUFFERS];
    uint32_t buffer_id_counter;

    /* Focus / pointer */
    int cur_pointer_x, cur_pointer_y;
    uint32_t pointer_focus;
    uint32_t keyboard_focus;
    uint32_t motion_window;

    /* Compositor workspace/desktop state */
    int workspace_count;
    int current_workspace;
    struct {
        char name[128];
        uint32_t *windows;
        int win_count;
        int win_cap;
    } workspaces[16];

    /* Renderer */
    DsRendererState renderer;

    /* DRM */
    DsDrmState drm;

    /* socket paths */
    char wayland_display[256];
    int wayland_socket_fd;
    char wayland_socket_path[512];
    int lock_fd;

    /* Wayland */
    WaylandServerState wl;

    /* X11 */
    XorgServerState x11;

    /* Config */
    int keyboard_repeat_rate;
    int keyboard_repeat_delay_ms;
    int pointer_accel_denom;
    int pointer_accel_num;
    int pointer_threshold;
    int tap_to_click;
    int natural_scrolling;
    int edge_scrolling;
    int two_finger_scrolling;
    int left_handed;
    int cursor_size;
    char cursor_theme[256];
    char wallpaper_path[2048];
    char icon_theme[256];
    char gtk_theme[256];
    char color_scheme[64];  /* prefer-dark / prefer-light / no-preference */
    int fractional_scaling;

    /* Runtime */
    int running;
    int exit_code;
    int reload_config;
    uint64_t serial;
    struct timespec startup_ts;
    int verbose;
    int debug;
    int config_valid;
    char config_file[2048];
    char startup_apps[4096];
    int idle_timeout_s;
    int dpms_standby_s;
    int dpms_suspend_s;
    int dpms_off_s;
    int animations_enabled;

} DisplayServerState;

/* ---------------------- Public API ---------------------- */

/* Lifecycle */
void disp_init(DisplayServerState *s);
void disp_cleanup(DisplayServerState *s);
int disp_parse_arguments(DisplayServerState *s, int argc, char **argv);
int disp_probe_backends(DisplayServerState *s);
int disp_start(DisplayServerState *s);
int disp_run_loop(DisplayServerState *s);
void disp_request_exit(DisplayServerState *s, int code);

/* Window management (common) */
DsWindow *disp_window_new(DisplayServerState *s);
int disp_window_destroy(DisplayServerState *s, uint32_t wid);
DsWindow *disp_window_lookup(DisplayServerState *s, uint32_t wid);
int disp_window_map(DisplayServerState *s, uint32_t wid);
int disp_window_unmap(DisplayServerState *s, uint32_t wid);
int disp_window_move(DisplayServerState *s, uint32_t wid, int x, int y);
int disp_window_resize(DisplayServerState *s, uint32_t wid, int w, int h);
int disp_window_configure(DisplayServerState *s, uint32_t wid, int x, int y, int w, int h, uint32_t flags);
int disp_window_raise(DisplayServerState *s, uint32_t wid);
int disp_window_lower(DisplayServerState *s, uint32_t wid);
int disp_window_focus(DisplayServerState *s, uint32_t wid);
int disp_window_set_title(DisplayServerState *s, uint32_t wid, const char *title);
int disp_window_set_app_id(DisplayServerState *s, uint32_t wid, const char *app_id);
int disp_window_set_maximize(DisplayServerState *s, uint32_t wid, int set);
int disp_window_set_minimize(DisplayServerState *s, uint32_t wid, int set);
int disp_window_set_fullscreen(DisplayServerState *s, uint32_t wid, int set, int output_idx);
int disp_window_set_workspace(DisplayServerState *s, uint32_t wid, int idx);
int disp_workspace_switch(DisplayServerState *s, int idx);
int disp_workspace_new(DisplayServerState *s, const char *name);

/* Render / damage */
int disp_damage_window(DisplayServerState *s, uint32_t wid, int x, int y, int w, int h);
int disp_repaint_output(DisplayServerState *s, int output_idx);
int disp_present(DisplayServerState *s);

/* Input */
int disp_add_input_device(DisplayServerState *s, DsInputType t, const char *name);
int disp_input_pointer_motion(DisplayServerState *s, int dx, int dy);
int disp_input_button(DisplayServerState *s, int btn, int pressed);
int disp_input_key(DisplayServerState *s, int keycode, int pressed);
int disp_input_touch(DisplayServerState *s, int tid, int x, int y, int active);
int disp_input_axis(DisplayServerState *s, int axis, double value);

/* Wayland-specific */
int wayland_init_socket(DisplayServerState *s);
int wayland_init_globals(DisplayServerState *s);
int wayland_event_dispatch(DisplayServerState *s, int timeout_ms);
int wayland_handle_client_connect(DisplayServerState *s);
int xwayland_start(DisplayServerState *s);
int xwayland_stop(DisplayServerState *s);

/* X11-specific */
int xorg_init_socket(DisplayServerState *s);
int xorg_init_atoms(DisplayServerState *s);
int xorg_init_extensions(DisplayServerState *s);
int xorg_init_screen(DisplayServerState *s);
int xorg_event_dispatch(DisplayServerState *s, int timeout_ms);
int xorg_handle_client_connect(DisplayServerState *s);
uint32_t xorg_intern_atom(DisplayServerState *s, const char *name, int ifexists);
int xorg_get_atom_name(DisplayServerState *s, uint32_t atom, char *out, size_t out_size);
void xorg_print_help(void);
void xorg_print_version(void);

/* Wayland CLI help */
void wayland_print_help(void);
void wayland_print_version(void);

#endif
