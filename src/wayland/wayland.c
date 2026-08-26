/*
 * Kenux OS - Wayland Display Server + Xorg X11 Server (Minimal)
 * Implementation
 */

#include "wayland.h"

/* ---------- Lifecycle ---------- */

void disp_init(DisplayServerState *s) {
    memset(s, 0, sizeof(*s));
    s->run_wayland = 1;
    s->run_x11 = 0;
    s->run_xwayland = 0;
    s->backend = DS_TRANS_FBDEV;
    s->output_count = 1;
    s->outputs[0].enabled = 1;
    s->outputs[0].width = 1920;
    s->outputs[0].height = 1080;
    s->outputs[0].refresh_mhz = 60000;
    s->outputs[0].scale = 1;
    s->outputs[0].backlight_pct = 100;
    strcpy(s->outputs[0].manufacturer, "KenuxK");
    strcpy(s->outputs[0].model, "Generic Display");
    strcpy(s->outputs[0].connector, "eDP-1");
    strcpy(s->outputs[0].name, "eDP-1");
    s->outputs[0].mode_count = 1;
    s->outputs[0].supported_modes[0][0] = 1920;
    s->outputs[0].supported_modes[0][1] = 1080;
    s->outputs[0].supported_modes[0][2] = 60;
    s->input_count = 0;
    strcpy(s->primary_seat, "seat0");
    s->clients = NULL;
    s->client_count = 0;
    s->window_id_counter = 1;
    s->buffer_id_counter = 1;
    s->workspace_count = 4;
    s->current_workspace = 0;
    for (int i = 0; i < 4; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "Workspace %d", i + 1);
        strncpy(s->workspaces[i].name, tmp, sizeof(s->workspaces[i].name) - 1);
        s->workspaces[i].win_count = 0;
        s->workspaces[i].win_cap = 0;
        s->workspaces[i].windows = NULL;
    }
    s->renderer.type = DS_RENDERER_PIXMAN;
    s->wayland_socket_fd = -1;
    s->lock_fd = -1;
    strcpy(s->wayland_display, "wayland-0");
    s->keyboard_repeat_rate = 40;
    s->keyboard_repeat_delay_ms = 600;
    s->pointer_accel_denom = 10;
    s->pointer_accel_num = 2;
    s->pointer_threshold = 4;
    s->tap_to_click = 1;
    s->natural_scrolling = 0;
    s->two_finger_scrolling = 1;
    s->cursor_size = 24;
    strcpy(s->cursor_theme, "breeze_cursors");
    strcpy(s->icon_theme, "breeze");
    strcpy(s->gtk_theme, "Breeze");
    strcpy(s->color_scheme, "prefer-dark");
    s->running = 0;
    s->exit_code = 0;
    s->serial = 0;
    s->animations_enabled = 1;
    clock_gettime(CLOCK_MONOTONIC, &s->startup_ts);
}

void disp_cleanup(DisplayServerState *s) {
    if (!s) return;
    DsClient *c = s->clients;
    while (c) {
        DsClient *next = c->next;
        if (c->resource_ids) free(c->resource_ids);
        free(c);
        c = next;
    }
    s->clients = NULL;
    for (int i = 0; i < DISP_MAX_WINDOWS; i++) {
        if (s->windows[i]) {
            if (s->windows[i]->children) free(s->windows[i]->children);
            free(s->windows[i]);
            s->windows[i] = NULL;
        }
    }
    for (int i = 0; i < DISP_MAX_BUFFERS; i++) {
        if (s->buffers[i]) {
            free(s->buffers[i]);
            s->buffers[i] = NULL;
        }
    }
    for (int i = 0; i < 16; i++) {
        if (s->workspaces[i].windows) free(s->workspaces[i].windows);
        s->workspaces[i].windows = NULL;
    }
    if (s->x11.atom_table) free(s->x11.atom_table);
    if (s->x11.event_queue) free(s->x11.event_queue);
}

int disp_parse_arguments(DisplayServerState *s, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            wayland_print_help();
            return 1;
        } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
            wayland_print_version();
            return 1;
        } else if (!strcmp(argv[i], "--x11")) {
            s->run_x11 = 1; s->run_wayland = 0;
        } else if (!strcmp(argv[i], "--wayland")) {
            s->run_wayland = 1; s->run_x11 = 0;
        } else if (!strcmp(argv[i], "--xwayland")) {
            s->run_xwayland = 1;
        } else if (!strcmp(argv[i], "--nested")) {
            s->run_nested = 1;
        } else if (!strcmp(argv[i], "--backend") && i + 1 < argc) {
            const char *b = argv[++i];
            if (!strcmp(b, "drm")) s->backend = DS_TRANS_DRM;
            else if (!strcmp(b, "gbm")) s->backend = DS_TRANS_GBM;
            else if (!strcmp(b, "fbdev")) s->backend = DS_TRANS_FBDEV;
            else if (!strcmp(b, "shm")) s->backend = DS_TRANS_SHM;
            else if (!strcmp(b, "headless")) s->backend = DS_TRANS_HEADLESS;
            else if (!strcmp(b, "vnc")) s->backend = DS_TRANS_VNC;
        } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
            s->outputs[0].width = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
            s->outputs[0].height = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            s->outputs[0].scale = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            s->verbose = 1;
        } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            s->debug = 1;
        }
    }
    return 0;
}

int disp_probe_backends(DisplayServerState *s) {
    /* Stub: would probe /dev/dri, /dev/fb0, etc. */
    if (s->verbose) {
        fprintf(stderr, "[disp] Probing backends...\n");
    }
    return 0;
}

int disp_start(DisplayServerState *s) {
    if (s->run_wayland) {
        if (wayland_init_socket(s) < 0) return -1;
        if (wayland_init_globals(s) < 0) return -1;
    }
    if (s->run_x11) {
        if (xorg_init_socket(s) < 0) return -1;
        if (xorg_init_atoms(s) < 0) return -1;
        if (xorg_init_extensions(s) < 0) return -1;
        if (xorg_init_screen(s) < 0) return -1;
    }
    if (s->run_xwayland) {
        xwayland_start(s);
    }
    s->running = 1;
    return 0;
}

int disp_run_loop(DisplayServerState *s) {
    while (s->running) {
        if (s->run_wayland) wayland_event_dispatch(s, 16);
        if (s->run_x11) xorg_event_dispatch(s, 16);
        disp_present(s);
    }
    return s->exit_code;
}

void disp_request_exit(DisplayServerState *s, int code) {
    s->running = 0;
    s->exit_code = code;
}

/* ---------- Window management ---------- */

DsWindow *disp_window_new(DisplayServerState *s) {
    if (s->window_id_counter == 0) s->window_id_counter = 1;
    for (uint32_t i = 1; i < DISP_MAX_WINDOWS; i++) {
        uint32_t id = (s->window_id_counter + i) % DISP_MAX_WINDOWS;
        if (id == 0) id = 1;
        if (!s->windows[id]) {
            DsWindow *w = calloc(1, sizeof(*w));
            if (!w) return NULL;
            w->id = id;
            w->parent_id = 0;
            w->w = 640; w->h = 480;
            w->background = 0xFF202020;
            w->visual = DS_FMT_ARGB8888;
            w->opacity = 255;
            w->accept_focus = 1;
            w->csd_enabled = 1;
            w->border = 1;
            s->windows[id] = w;
            s->window_id_counter = (id + 1) % DISP_MAX_WINDOWS;
            return w;
        }
    }
    return NULL;
}

int disp_window_destroy(DisplayServerState *s, uint32_t wid) {
    if (wid == 0 || wid >= DISP_MAX_WINDOWS) return -ENOENT;
    DsWindow *w = s->windows[wid];
    if (!w) return -ENOENT;
    if (w->children) free(w->children);
    free(w);
    s->windows[wid] = NULL;
    return 0;
}

DsWindow *disp_window_lookup(DisplayServerState *s, uint32_t wid) {
    if (wid == 0 || wid >= DISP_MAX_WINDOWS) return NULL;
    return s->windows[wid];
}

int disp_window_map(DisplayServerState *s, uint32_t wid) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->mapped = 1;
    w->minimized = 0;
    disp_damage_window(s, wid, 0, 0, w->w, w->h);
    return 0;
}

int disp_window_unmap(DisplayServerState *s, uint32_t wid) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->mapped = 0;
    return 0;
}

int disp_window_move(DisplayServerState *s, uint32_t wid, int x, int y) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->x = x; w->y = y;
    return 0;
}

int disp_window_resize(DisplayServerState *s, uint32_t wid, int w, int h) {
    DsWindow *win = disp_window_lookup(s, wid);
    if (!win) return -ENOENT;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    win->w = w; win->h = h;
    disp_damage_window(s, wid, 0, 0, w, h);
    return 0;
}

int disp_window_configure(DisplayServerState *s, uint32_t wid, int x, int y, int w, int h, uint32_t flags) {
    DsWindow *win = disp_window_lookup(s, wid);
    if (!win) return -ENOENT;
    if (flags & 1) win->x = x;
    if (flags & 2) win->y = y;
    if (flags & 4) win->w = (w>0)?w:win->w;
    if (flags & 8) win->h = (h>0)?h:win->h;
    return 0;
}

int disp_window_raise(DisplayServerState *s, uint32_t wid) {
    (void)s; (void)wid; return 0;
}
int disp_window_lower(DisplayServerState *s, uint32_t wid) {
    (void)s; (void)wid; return 0;
}
int disp_window_focus(DisplayServerState *s, uint32_t wid) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w && wid != 0) return -ENOENT;
    s->keyboard_focus = wid;
    s->pointer_focus = wid;
    return 0;
}
int disp_window_set_title(DisplayServerState *s, uint32_t wid, const char *title) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    strncpy(w->title, title ? title : "", sizeof(w->title) - 1);
    return 0;
}
int disp_window_set_app_id(DisplayServerState *s, uint32_t wid, const char *app_id) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    strncpy(w->app_id, app_id ? app_id : "", sizeof(w->app_id) - 1);
    return 0;
}
int disp_window_set_maximize(DisplayServerState *s, uint32_t wid, int set) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->maximized = set ? 1 : 0;
    return 0;
}
int disp_window_set_minimize(DisplayServerState *s, uint32_t wid, int set) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->minimized = set ? 1 : 0;
    if (set) w->mapped = 0;
    return 0;
}
int disp_window_set_fullscreen(DisplayServerState *s, uint32_t wid, int set, int output_idx) {
    DsWindow *w = disp_window_lookup(s, wid);
    if (!w) return -ENOENT;
    w->fullscreen = set ? 1 : 0;
    if (set && output_idx >= 0 && output_idx < s->output_count && s->outputs[output_idx].enabled) {
        w->x = s->outputs[output_idx].x;
        w->y = s->outputs[output_idx].y;
        w->w = s->outputs[output_idx].width;
        w->h = s->outputs[output_idx].height;
    }
    return 0;
}
int disp_window_set_workspace(DisplayServerState *s, uint32_t wid, int idx) {
    if (idx < 0 || idx >= s->workspace_count) return -EINVAL;
    /* Stub: would remove from old and add to new workspace list */
    (void)s; (void)wid;
    return 0;
}
int disp_workspace_switch(DisplayServerState *s, int idx) {
    if (idx < 0 || idx >= s->workspace_count) return -EINVAL;
    s->current_workspace = idx;
    return 0;
}
int disp_workspace_new(DisplayServerState *s, const char *name) {
    if (s->workspace_count >= 16) return -ENOMEM;
    int i = s->workspace_count++;
    strncpy(s->workspaces[i].name, name ? name : "New", sizeof(s->workspaces[i].name) - 1);
    s->workspaces[i].win_count = 0;
    s->workspaces[i].win_cap = 0;
    s->workspaces[i].windows = NULL;
    return i;
}

/* ---------- Render ---------- */

int disp_damage_window(DisplayServerState *s, uint32_t wid, int x, int y, int w, int h) {
    DsWindow *win = disp_window_lookup(s, wid);
    if (!win) return -ENOENT;
    win->damage_pending = 1;
    win->damage.x = x; win->damage.y = y;
    win->damage.width = w; win->damage.height = h;
    return 0;
}
int disp_repaint_output(DisplayServerState *s, int output_idx) {
    (void)s; (void)output_idx; return 0;
}
int disp_present(DisplayServerState *s) {
    /* Stub: swap framebuffers, handle damage */
    s->serial++;
    return 0;
}

/* ---------- Input ---------- */

int disp_add_input_device(DisplayServerState *s, DsInputType t, const char *name) {
    if (s->input_count >= DISP_MAX_INPUTS) return -ENOMEM;
    int i = s->input_count++;
    s->inputs[i].type = t;
    s->inputs[i].id = i;
    strncpy(s->inputs[i].name, name ? name : "input", sizeof(s->inputs[i].name) - 1);
    return i;
}
int disp_input_pointer_motion(DisplayServerState *s, int dx, int dy) {
    s->cur_pointer_x += dx;
    s->cur_pointer_y += dy;
    if (s->cur_pointer_x < 0) s->cur_pointer_x = 0;
    if (s->cur_pointer_y < 0) s->cur_pointer_y = 0;
    if (s->output_count > 0) {
        if (s->cur_pointer_x > s->outputs[0].width) s->cur_pointer_x = s->outputs[0].width;
        if (s->cur_pointer_y > s->outputs[0].height) s->cur_pointer_y = s->outputs[0].height;
    }
    return 0;
}
int disp_input_button(DisplayServerState *s, int btn, int pressed) {
    if (btn < 0 || btn > 15) return -EINVAL;
    if (s->input_count > 0) {
        if (pressed) s->inputs[0].ptr_button_state |= (1 << btn);
        else s->inputs[0].ptr_button_state &= ~(1 << btn);
    }
    return 0;
}
int disp_input_key(DisplayServerState *s, int keycode, int pressed) {
    (void)s; (void)keycode; (void)pressed;
    return 0;
}
int disp_input_touch(DisplayServerState *s, int tid, int x, int y, int active) {
    if (s->input_count == 0) return -ENODEV;
    DsInputDevice *dev = &s->inputs[0];
    if (tid < 0 || tid >= 16) return -EINVAL;
    dev->touches[tid].tid = tid;
    dev->touches[tid].x = x;
    dev->touches[tid].y = y;
    dev->touches[tid].active = active ? 1 : 0;
    return 0;
}
int disp_input_axis(DisplayServerState *s, int axis, double value) {
    (void)s; (void)axis; (void)value; return 0;
}

/* ---------- Wayland-specific ---------- */

int wayland_init_socket(DisplayServerState *s) {
    /* Stub: would create unix socket at $XDG_RUNTIME_DIR/wayland-0 */
    char path[512];
    const char *rt = getenv("XDG_RUNTIME_DIR");
    if (!rt) rt = "/tmp";
    snprintf(path, sizeof(path), "%s/%s", rt, s->wayland_display);
    strncpy(s->wayland_socket_path, path, sizeof(s->wayland_socket_path) - 1);
    s->wayland_socket_fd = -1;
    if (s->verbose) fprintf(stderr, "[wayland] socket path: %s\n", path);
    return 0;
}

int wayland_init_globals(DisplayServerState *s) {
    WaylandServerState *wl = &s->wl;
    wl->global_count = 0;
    #define ADD_GLOBAL(n, v) do { \
        if (wl->global_count < DISP_MAX_GLOBALS) { \
            wl->globals[wl->global_count].name = (n); \
            wl->globals[wl->global_count].version = (v); \
            wl->globals[wl->global_count].id = wl->global_count + 1; \
            wl->globals[wl->global_count].bind = NULL; \
            wl->global_count++; \
        } \
    } while (0)
    ADD_GLOBAL("wl_display", 1);
    ADD_GLOBAL("wl_registry", 1);
    ADD_GLOBAL("wl_compositor", 4);
    ADD_GLOBAL("wl_subcompositor", 1);
    ADD_GLOBAL("wl_shm", 1);
    ADD_GLOBAL("wl_seat", 7);
    ADD_GLOBAL("wl_output", 4);
    ADD_GLOBAL("wl_data_device_manager", 3);
    ADD_GLOBAL("wl_shell", 1);
    ADD_GLOBAL("xdg_wm_base", 6);
    ADD_GLOBAL("zwp_layer_shell_v1", 4);
    ADD_GLOBAL("wp_viewporter", 1);
    ADD_GLOBAL("wp_fractional_scale_manager_v1", 1);
    ADD_GLOBAL("wp_cursor_shape_manager_v1", 1);
    ADD_GLOBAL("zwp_virtual_keyboard_manager_v1", 1);
    ADD_GLOBAL("zwp_input_method_manager_v2", 1);
    ADD_GLOBAL("wp_security_context_manager_v1", 1);
    #undef ADD_GLOBAL
    wl->wl_display_id = 1;
    wl->wl_registry_id = 2;
    wl->wl_seat_id = 0;
    strncpy(wl->seat_name, "seat0", sizeof(wl->seat_name) - 1);
    wl->seat_caps = DS_SEAT_POINTER_CAP | DS_SEAT_KEYBOARD_CAP | DS_SEAT_TOUCH_CAP;
    wl->ddm_id = 0;
    wl->xdg_wm_id = 0;
    wl->layer_shell_id = 0;
    wl->input_method_enabled = 0;
    wl->virtual_keyboard_enabled = 0;
    wl->fractional_scale_enabled = 1;
    wl->viewporter_enabled = 1;
    wl->cursor_shape_enabled = 1;
    wl->security_context_enabled = 1;
    if (s->verbose) fprintf(stderr, "[wayland] registered %d globals\n", wl->global_count);
    return 0;
}

int wayland_event_dispatch(DisplayServerState *s, int timeout_ms) {
    /* Stub: would poll() on wayland_socket_fd and dispatch client events */
    (void)s; (void)timeout_ms;
    return 0;
}

int wayland_handle_client_connect(DisplayServerState *s) {
    /* Stub: would accept() new client connection on wayland_socket_fd */
    if (s->client_count >= DISP_MAX_CLIENTS) return -ENOMEM;
    DsClient *c = calloc(1, sizeof(*c));
    if (!c) return -ENOMEM;
    c->id = (uint32_t)(++s->serial);
    c->socket_fd = -1;
    c->pid = 0;
    c->uid = getuid();
    c->gid = getgid();
    c->connected = 1;
    c->is_xwayland = 0;
    c->can_access_seat = 1;
    c->can_copy_paste = 1;
    c->can_screenshot = 0;
    c->can_screen_share = 0;
    c->next = s->clients;
    s->clients = c;
    s->client_count++;
    if (s->verbose) fprintf(stderr, "[wayland] client %u connected\n", c->id);
    return 0;
}

int xwayland_start(DisplayServerState *s) {
    if (s->x11.xwayland_started) return 0;
    if (s->verbose) {
        fprintf(stderr, "[xwayland] starting Xwayland :%d (socket %s)\n",
                s->x11.display_number, s->x11.xwayland_socket_path);
    }
    /* Stub: would fork()/execvp() Xwayland with -displayfd and wayland socket */
    s->x11.xwayland_pid = 0;
    s->x11.xwayland_started = 1;
    return 0;
}

int xwayland_stop(DisplayServerState *s) {
    if (!s->x11.xwayland_started) return 0;
    if (s->verbose) fprintf(stderr, "[xwayland] stopping (pid=%d)\n", (int)s->x11.xwayland_pid);
    /* Stub: would kill(xwayland_pid, SIGTERM) and waitpid() */
    s->x11.xwayland_pid = 0;
    s->x11.xwayland_started = 0;
    return 0;
}

/* ---------- X11-specific ---------- */

int xorg_init_socket(DisplayServerState *s) {
    snprintf(s->x11.xwayland_socket_path, sizeof(s->x11.xwayland_socket_path),
             "/tmp/.X11-unix/X%d", s->x11.display_number);
    s->x11.xwayland_fd[0] = -1;
    s->x11.xwayland_fd[1] = -1;
    if (s->verbose) fprintf(stderr, "[xorg] socket: %s\n", s->x11.xwayland_socket_path);
    return 0;
}

int xorg_init_atoms(DisplayServerState *s) {
    s->x11.atom_id_counter = 1;
    s->x11.atom_cap = 256;
    s->x11.atom_table = calloc(s->x11.atom_cap, sizeof(*s->x11.atom_table));
    if (!s->x11.atom_table) return -ENOMEM;
    s->x11.atom_count = 0;
    /* Pre-intern the standard X11 atoms */
    static const char *std_atoms[] = {
        "PRIMARY", "SECONDARY", "ARC", "ATOM", "BITMAP", "CARDINAL",
        "COLORMAP", "CURSOR", "CUT_BUFFER0", "FONT", "INTEGER", "PIXMAP",
        "POINT", "RECTANGLE", "STRING", "VISUALID", "WINDOW", "WM_HINTS",
        "WM_CLIENT_MACHINE", "WM_COMMAND", "WM_NAME", "WM_ICON_NAME",
        "WM_NORMAL_HINTS", "WM_SIZE_HINTS", "WM_PROTOCOLS",
        "WM_DELETE_WINDOW", "WM_TAKE_FOCUS", "WM_SAVE_YOURSELF", "WM_STATE",
        "WM_CHANGE_STATE", "WM_CLASS", "WM_TRANSIENT_FOR",
        "_NET_WM_NAME", "_NET_WM_ICON_NAME", "_NET_SUPPORTED", NULL
    };
    for (int i = 0; std_atoms[i]; i++) {
        xorg_intern_atom(s, std_atoms[i], 0);
    }
    if (s->verbose) fprintf(stderr, "[xorg] interned %d atoms\n", s->x11.atom_count);
    return 0;
}

int xorg_init_extensions(DisplayServerState *s) {
    static const struct { X11Extension ext; int maj, min; } exts[] = {
        { XEXT_SHAPE, 1, 1 }, { XEXT_RENDER, 0, 11 },
        { XEXT_RANDR, 1, 6 }, { XEXT_XINERAMA, 1, 1 },
        { XEXT_DAMAGE, 1, 1 }, { XEXT_COMPOSITE, 0, 4 },
        { XEXT_FIXES, 6, 0 }, { XEXT_DPMS, 1, 1 },
        { XEXT_GLX, 1, 4 }, { XEXT_DRI2, 1, 2 },
        { XEXT_DRI3, 1, 0 }, { XEXT_PRESENT, 1, 0 },
        { XEXT_XINPUT2, 2, 3 }, { XEXT_XKB, 1, 0 },
        { XEXT_SYNC, 3, 1 }, { XEXT_BIGREQ, 0, 0 },
        { XEXT_RECORD, 1, 13 }, { XEXT_SCREENSAVER, 1, 1 },
        { XEXT_SECURITY, 1, 0 }, { XEXT_MITSHM, 1, 1 },
        { XEXT_DOUBLE_BUFFER, 1, 1 },
    };
    for (size_t i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        if (exts[i].ext < 0 || exts[i].ext >= XEXT_MAX_COUNT) continue;
        s->x11.ext[exts[i].ext].major_version = exts[i].maj;
        s->x11.ext[exts[i].ext].minor_version = exts[i].min;
        s->x11.ext[exts[i].ext].opcode = (int)exts[i].ext + 128;
        s->x11.ext[exts[i].ext].event_base = (int)exts[i].ext * 2;
        s->x11.ext[exts[i].ext].error_base = (int)exts[i].ext + 64;
        s->x11.ext[exts[i].ext].enabled = 1;
    }
    if (s->verbose)
        fprintf(stderr, "[xorg] initialised %zu extensions\n",
                sizeof(exts)/sizeof(exts[0]));
    return 0;
}

int xorg_init_screen(DisplayServerState *s) {
    if (s->output_count > 0) {
        s->x11.display_width = s->outputs[0].width;
        s->x11.display_height = s->outputs[0].height;
    } else {
        s->x11.display_width = 1280;
        s->x11.display_height = 720;
    }
    s->x11.root_depth = 24;
    s->x11.root_visual = 0x21;
    s->x11.root_window = 0;
    s->x11.screen_count = 1;
    s->x11.default_screen = 0;
    if (s->verbose) {
        fprintf(stderr, "[xorg] screen %dx%d depth=%d\n",
                s->x11.display_width, s->x11.display_height, s->x11.root_depth);
    }
    return 0;
}

int xorg_event_dispatch(DisplayServerState *s, int timeout_ms) {
    /* Stub: would poll() on X11 socket and dispatch X requests */
    (void)s; (void)timeout_ms;
    return 0;
}

int xorg_handle_client_connect(DisplayServerState *s) {
    /* Stub: would accept() new X11 client connection */
    if (s->client_count >= DISP_MAX_CLIENTS) return -ENOMEM;
    DsClient *c = calloc(1, sizeof(*c));
    if (!c) return -ENOMEM;
    c->id = (uint32_t)(++s->serial);
    c->socket_fd = -1;
    c->pid = 0;
    c->uid = getuid();
    c->gid = getgid();
    c->connected = 1;
    c->is_xwayland = 0;
    c->can_access_seat = 1;
    c->can_copy_paste = 1;
    c->can_screenshot = 0;
    c->can_screen_share = 0;
    c->next = s->clients;
    s->clients = c;
    s->client_count++;
    if (s->verbose) fprintf(stderr, "[xorg] client %u connected\n", c->id);
    return 0;
}

uint32_t xorg_intern_atom(DisplayServerState *s, const char *name, int ifexists) {
    if (!name || !name[0]) return XATOM_NONE;
    for (int i = 0; i < s->x11.atom_count; i++) {
        if (s->x11.atom_table[i].name[0] &&
            strcmp(s->x11.atom_table[i].name, name) == 0) {
            return s->x11.atom_table[i].id;
        }
    }
    if (ifexists) return XATOM_NONE;
    if (s->x11.atom_count >= s->x11.atom_cap) {
        int new_cap = s->x11.atom_cap ? s->x11.atom_cap * 2 : 256;
        void *p = realloc(s->x11.atom_table, new_cap * sizeof(*s->x11.atom_table));
        if (!p) return XATOM_NONE;
        s->x11.atom_table = p;
        s->x11.atom_cap = new_cap;
    }
    uint32_t id = ++s->x11.atom_id_counter;
    s->x11.atom_table[s->x11.atom_count].id = id;
    strncpy(s->x11.atom_table[s->x11.atom_count].name, name,
            sizeof(s->x11.atom_table[0].name) - 1);
    s->x11.atom_table[s->x11.atom_count].name[sizeof(s->x11.atom_table[0].name) - 1] = '\0';
    s->x11.atom_count++;
    return id;
}

int xorg_get_atom_name(DisplayServerState *s, uint32_t atom, char *out, size_t out_size) {
    if (!out || out_size == 0) return -EINVAL;
    out[0] = '\0';
    for (int i = 0; i < s->x11.atom_count; i++) {
        if (s->x11.atom_table[i].id == atom) {
            strncpy(out, s->x11.atom_table[i].name, out_size - 1);
            out[out_size - 1] = '\0';
            return 0;
        }
    }
    return -ENOENT;
}

/* ---------- CLI help / version ---------- */

void xorg_print_help(void) {
    printf("%s - KenuxK Xorg X11 server (minimal)\n\n", XORG_VERSION_STR);
    printf("USAGE: Xorg [:display] [opts]\n\n");
    printf("DISPLAY:\n");
    printf("  :N               Display number (e.g. :0, :1)\n");
    printf("  -display :N      Same as above\n");
    printf("  -novtswitch      Disable VT switch on exit\n");
    printf("SCREEN / RENDER:\n");
    printf("  -screen WxHxD    Screen geometry (e.g. 1920x1080x24)\n");
    printf("  -depth BPP       Default screen depth (8/15/16/24/30)\n");
    printf("  -rgba RGB        RGBA visual order\n");
    printf("  -retro           Force software rendering\n");
    printf("INPUT:\n");
    printf("  -keyboard DEVICE Evdev keyboard node\n");
    printf("  -pointer DEVICE  Evdev pointer node\n");
    printf("  -nograb          Don't grab input devices\n");
    printf("NESTING / XWAYLAND:\n");
    printf("  -xwayland        Run as XWayland bridge (under Wayland)\n");
    printf("  -rootless        Rootless mode (per-app windows)\n");
    printf("  -fullscreen      Fullscreen root window\n");
    printf("  -geometry WxH    Rootless geometry\n");
    printf("AUTH / MISC:\n");
    printf("  -auth FILE       Xauthority file\n");
    printf("  -config FILE     xorg.conf path\n");
    printf("  -configdir DIR   xorg.conf.d path\n");
    printf("  -modulepath P    Module search path\n");
    printf("  -logpath FILE    Log file\n");
    printf("  -verbose N       Verbosity level\n");
    printf("  -quiet           Suppress non-error log\n");
    printf("  -version         Print version and exit\n");
    printf("  -help            This help\n");
}

void xorg_print_version(void) {
    printf("%s\n", XORG_VERSION_STR);
    printf("KenuxK Xorg minimal; compatible subset of X11R7.9 / Xorg 21.1.\n");
}

void wayland_print_help(void) {
    printf("%s - KenuxK Wayland compositor (minimal)\n\n", WL_VERSION_STR);
    printf("USAGE: wayland [opts]\n\n");
    printf("MODE:\n");
    printf("  --wayland        Run as Wayland compositor (default)\n");
    printf("  --x11            Run as Xorg X11 server\n");
    printf("  --xwayland       Launch XWayland bridge alongside Wayland\n");
    printf("  --nested         Run as nested compositor (in parent Wayland/X11)\n");
    printf("BACKEND:\n");
    printf("  --backend TYPE   drm|gbm|fbdev|shm|headless|vnc\n");
    printf("OUTPUT:\n");
    printf("  --width W        Output width in pixels\n");
    printf("  --height H       Output height in pixels\n");
    printf("  --scale S        Output scale (1,2,3)\n");
    printf("INPUT:\n");
    printf("  --seat NAME      Seat name (default seat0)\n");
    printf("  --tap-to-click   Enable tap-to-click on touchpad\n");
    printf("  --natural-scroll Enable natural scrolling\n");
    printf("  --left-handed    Swap mouse buttons for left-handed use\n");
    printf("RENDER:\n");
    printf("  --renderer TYPE  pixman|gl|vk|dumb\n");
    printf("CLIPBOARD / DND:\n");
    printf("  --no-clipboard   Disable clipboard manager\n");
    printf("  --no-dnd         Disable drag-and-drop\n");
    printf("SECURITY / DEBUG:\n");
    printf("  --no-csd         Force server-side decorations\n");
    printf("  --security-context Enable security-context-v1\n");
    printf("  -v / --verbose   Verbose logging\n");
    printf("  -d / --debug     Debug logging\n");
    printf("  --version        Print version\n");
    printf("  --help           This help\n");
}

void wayland_print_version(void) {
    printf("%s\n", WL_VERSION_STR);
    printf("KenuxK Wayland minimal; wl_display + wl_registry + wl_compositor subset.\n");
}

#ifndef KENUXK_NO_MAIN_WAYLAND
int main(int argc, char **argv) {
    DisplayServerState state;
    disp_init(&state);

    if (disp_parse_arguments(&state, argc, argv) != 0) {
        disp_cleanup(&state);
        return 1;
    }
    if (disp_probe_backends(&state) < 0) {
        fprintf(stderr, "wayland: failed to probe display backends\n");
        disp_cleanup(&state);
        return 1;
    }
    if (disp_start(&state) < 0) {
        fprintf(stderr, "wayland: failed to start display server\n");
        disp_cleanup(&state);
        return 1;
    }
    int code = disp_run_loop(&state);
    disp_cleanup(&state);
    return code;
}
#endif