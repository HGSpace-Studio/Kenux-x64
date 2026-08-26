#include <arch/framebuffer.h>
#include <arch/memory.h>
#include <arch/boot.h>
#include <string.h>
#include <arch/spinlock.h>

#define DISPLAY_MAX_LAYERS      8
#define DISPLAY_MAX_WINDOWS     64
#define DISPLAY_FONT_CACHE_SIZE 4096
#define DISPLAY_CURSOR_BLINK_RATE 500

typedef struct {
    int x, y;
    uint32_t color;
    bool visible;
    bool blinking;
    uint64_t last_blink_time;
    bool blink_state;
} cursor_t;

typedef struct display_layer {
    uint32_t* buffer;
    int width;
    int height;
    int x_offset;
    int y_offset;
    bool enabled;
    bool transparent;
    uint32_t transparent_color;
    float alpha;
    int z_order;
    char name[32];
} display_layer_t;

typedef struct window_info {
    uint32_t id;
    int x, y;
    int width, height;
    char title[128];
    bool visible;
    bool focused;
    bool resizable;
    bool decorated;
    uint32_t bg_color;
    uint32_t border_color;
    int border_width;
    display_layer_t* layer;
    void (*message_handler)(uint32_t msg_type, uint64_t param1, uint64_t param2);
    void* user_data;
    struct window_info* next;
    struct window_info* prev;
} window_info_t;

typedef struct font_glyph {
    uint8_t data[32];
    int width, height;
    int advance;
    int bearing_x, bearing_y;
} font_glyph_t;

typedef struct {
    font_glyph_t glyphs[256];
    int size;
    int line_height;
    bool antialiased;
    char name[32];
} font_cache_entry_t;

typedef struct display_stats {
    uint64_t frames_rendered;
    uint64_t pixels_drawn;
    uint64_t blit_operations;
    uint64_t composite_operations;
    uint64_t text_chars_rendered;
    uint64_t window_creations;
    uint64_t window_destructions;
    uint64_t layer_switches;
    uint64_t vsync_count;
    uint64_t frame_time_ns;
    uint64_t min_frame_time_ns;
    uint64_t max_frame_time_ns;
    double avg_fps;
    double current_fps;
    uint64_t memory_used;
    int active_windows;
    int active_layers;
} display_stats_t;

static fb_info_t fb_info;
static framebuffer_t framebuffer;
static cursor_t display_cursor;
static display_layer_t layers[DISPLAY_MAX_LAYERS];
static window_info_t windows[DISPLAY_MAX_WINDOWS];
static font_cache_entry_t font_cache[DISPLAY_FONT_CACHE_SIZE];
static display_stats_t disp_stats;
static spinlock_t display_lock;
static spinlock_t window_lock;
static int initialized = 0;
static uint32_t next_window_id = 1;
static uint32_t* back_buffer = NULL;
static uint32_t* front_buffer = NULL;
static bool double_buffer_enabled = false;
static bool vsync_enabled = false;
static uint64_t last_frame_time = 0;
static uint64_t frame_times[60];
static int frame_time_index = 0;

void display_init(void)
{
    if (initialized) return;

    spin_init(&display_lock);
    spin_init(&window_lock);

    memset(&display_cursor, 0, sizeof(cursor_t));
    memset(layers, 0, sizeof(layers));
    memset(windows, 0, sizeof(windows));
    memset(&disp_stats, 0, sizeof(display_stats_t));
    memset(frame_times, 0, sizeof(frame_times));

    for (int i = 0; i < DISPLAY_MAX_LAYERS; i++) {
        layers[i].z_order = i;
        snprintf(layers[i].name, sizeof(layers[i].name), "layer_%d", i);
    }

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        windows[i].id = 0;
        windows[i].next = NULL;
        windows[i].prev = NULL;
    }

    display_cursor.visible = true;
    display_cursor.blinking = true;
    display_cursor.color = FB_COLOR_WHITE;

    initialized = 1;
}

void display_set_double_buffer(bool enable)
{
    if (!initialized) return;

    spin_lock(&display_lock);

    if (enable && !double_buffer_enabled) {
        fb_info_t* fb = fb_get_info();
        if (fb && fb->width > 0 && fb->height > 0) {
            size_t buffer_size = fb->width * fb->height * 4;
            back_buffer = (uint32_t*)memory_alloc(buffer_size);
            front_buffer = (uint32_t*)memory_alloc(buffer_size);

            if (back_buffer && front_buffer) {
                double_buffer_enabled = true;
                disp_stats.memory_used += buffer_size * 2;
            } else {
                if (back_buffer) memory_free(back_buffer);
                if (front_buffer) memory_free(front_buffer);
                back_buffer = NULL;
                front_buffer = NULL;
            }
        }
    } else if (!enable && double_buffer_enabled) {
        if (back_buffer) {
            memory_free(back_buffer);
            back_buffer = NULL;
        }
        if (front_buffer) {
            memory_free(front_buffer);
            front_buffer = NULL;
        }
        double_buffer_enabled = false;
    }

    spin_unlock(&display_lock);
}

void display_enable_vsync(bool enable)
{
    vsync_enabled = enable;
}

void display_flip_buffers(void)
{
    if (!initialized || !double_buffer_enabled) return;

    spin_lock(&display_lock);

    fb_info_t* fb = fb_get_info();
    if (!fb || !fb->phys_addr || !back_buffer || !front_buffer) {
        spin_unlock(&display_lock);
        return;
    }

    memcpy(front_buffer, back_buffer, fb->width * fb->height * 4);
    memcpy((void*)fb->phys_addr, front_buffer, fb->width * fb->height * 4);

    disp_stats.frames_rendered++;
    uint64_t now = get_current_time_ns();
    if (last_frame_time > 0) {
        uint64_t frame_time = now - last_frame_time;
        frame_times[frame_time_index++ % 60] = frame_time;
        disp_stats.frame_time_ns = frame_time;

        if (frame_time < disp_stats.min_frame_time_ns || disp_stats.min_frame_time_ns == 0) {
            disp_stats.min_frame_time_ns = frame_time;
        }
        if (frame_time > disp_stats.max_frame_time_ns) {
            disp_stats.max_frame_time_ns = frame_time;
        }

        uint64_t sum = 0;
        int count = frame_time_index < 60 ? frame_time_index : 60;
        for (int i = 0; i < count; i++) {
            sum += frame_times[i];
        }
        if (count > 0) {
            disp_stats.avg_fps = 1000000000.0 / ((double)sum / count);
        }
        disp_stats.current_fps = 1000000000.0 / (double)frame_time;
    }
    last_frame_time = now;

    spin_unlock(&display_lock);
}

uint32_t* display_get_back_buffer(void)
{
    if (!initialized || !double_buffer_enabled) {
        fb_info_t* fb = fb_get_info();
        return fb ? (uint32_t*)fb->phys_addr : NULL;
    }
    return back_buffer;
}

int display_create_layer(int width, int height, const char* name)
{
    if (!initialized || width <= 0 || height <= 0) return -EINVAL;

    spin_lock(&display_lock);

    int layer_id = -1;
    for (int i = 0; i < DISPLAY_MAX_LAYERS; i++) {
        if (!layers[i].enabled) {
            layer_id = i;
            break;
        }
    }

    if (layer_id < 0) {
        spin_unlock(&display_lock);
        return -ENOSPC;
    }

    size_t buffer_size = width * height * 4;
    layers[layer_id].buffer = (uint32_t*)memory_alloc(buffer_size);
    if (!layers[layer_id].buffer) {
        spin_unlock(&display_lock);
        return -ENOMEM;
    }

    layers[layer_id].width = width;
    layers[layer_id].height = height;
    layers[layer_id].x_offset = 0;
    layers[layer_id].y_offset = 0;
    layers[layer_id].enabled = true;
    layers[layer_id].transparent = false;
    layers[layer_id].alpha = 1.0f;
    layers[layer_id].transparent_color = 0xFF000000;
    if (name) strncpy(layers[layer_id].name, name, 31);

    memset(layers[layer_id].buffer, 0, buffer_size);
    disp_stats.memory_used += buffer_size;
    disp_stats.active_layers++;

    spin_unlock(&display_lock);
    return layer_id;
}

void display_destroy_layer(int layer_id)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS) return;

    spin_lock(&display_lock);

    if (layers[layer_id].enabled && layers[layer_id].buffer) {
        size_t buffer_size = layers[layer_id].width * layers[layer_id].height * 4;
        memory_free(layers[layer_id].buffer);
        layers[layer_id].buffer = NULL;
        disp_stats.memory_used -= buffer_size;
        disp_stats.active_layers--;
    }

    memset(&layers[layer_id], 0, sizeof(display_layer_t));
    layers[layer_id].z_order = layer_id;

    spin_unlock(&display_lock);
}

void display_set_layer_position(int layer_id, int x, int y)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS) return;

    spin_lock(&display_lock);
    layers[layer_id].x_offset = x;
    layers[layer_id].y_offset = y;
    spin_unlock(&display_lock);
}

void display_set_layer_alpha(int layer_id, float alpha)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS) return;

    spin_lock(&display_lock);
    layers[layer_id].alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    spin_unlock(&display_lock);
}

void display_set_layer_zorder(int layer_id, int z_order)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS) return;

    spin_lock(&display_lock);
    layers[layer_id].z_order = z_order;
    spin_unlock(&display_lock);
}

uint32_t display_create_window(int x, int y, int width, int height,
                               const char* title, bool decorated)
{
    if (!initialized || width <= 0 || height <= 0) return 0;

    spin_lock(&window_lock);

    int win_idx = -1;
    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == 0) {
            win_idx = i;
            break;
        }
    }

    if (win_idx < 0) {
        spin_unlock(&window_lock);
        return 0;
    }

    uint32_t id = next_window_id++;
    windows[win_idx].id = id;
    windows[win_idx].x = x;
    windows[win_idx].y = y;
    windows[win_idx].width = width;
    windows[win_idx].height = height;
    windows[win_idx].visible = true;
    windows[win_idx].focused = false;
    windows[win_idx].resizable = true;
    windows[win_idx].decorated = decorated;
    windows[win_idx].bg_color = 0xFFE0E0E0;
    windows[win_idx].border_color = 0xFF808080;
    windows[win_idx].border_width = 3;
    windows[win_idx].message_handler = NULL;
    windows[win_idx].user_data = NULL;
    windows[win_idx].next = NULL;
    windows[win_idx].prev = NULL;

    if (title) strncpy(windows[win_idx].title, title, 127);

    int layer_id = display_create_layer(width, height,
                                        title ? title : "window");
    if (layer_id >= 0) {
        windows[win_idx].layer = &layers[layer_id];
        display_set_layer_position(layer_id, x, y);
    }

    disp_stats.window_creations++;
    disp_stats.active_windows++;

    spin_unlock(&window_lock);
    return id;
}

void display_destroy_window(uint32_t window_id)
{
    if (!initialized || window_id == 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            if (windows[i].layer) {
                int layer_id = windows[i].layer - layers;
                display_destroy_layer(layer_id);
            }

            memset(&windows[i], 0, sizeof(window_info_t));
            disp_stats.window_destructions++;
            disp_stats.active_windows--;
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_move_window(uint32_t window_id, int x, int y)
{
    if (!initialized || window_id == 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            windows[i].x = x;
            windows[i].y = y;
            if (windows[i].layer) {
                int layer_id = windows[i].layer - layers;
                display_set_layer_position(layer_id, x, y);
            }
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_resize_window(uint32_t window_id, int width, int height)
{
    if (!initialized || window_id == 0 || width <= 0 || height <= 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            if (windows[i].layer) {
                int layer_id = windows[i].layer - layers;
                display_destroy_layer(layer_id);

                int new_layer_id = display_create_layer(width, height,
                                                        windows[i].title);
                if (new_layer_id >= 0) {
                    windows[i].layer = &layers[new_layer_id];
                    display_set_layer_position(new_layer_id, windows[i].x, windows[i].y);
                }
            }
            windows[i].width = width;
            windows[i].height = height;
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_show_window(uint32_t window_id)
{
    if (!initialized || window_id == 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            windows[i].visible = true;
            if (windows[i].layer) {
                windows[i].layer->enabled = true;
            }
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_hide_window(uint32_t window_id)
{
    if (!initialized || window_id == 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            windows[i].visible = false;
            if (windows[i].layer) {
                windows[i].layer->enabled = false;
            }
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_focus_window(uint32_t window_id)
{
    if (!initialized || window_id == 0) return;

    spin_lock(&window_lock);

    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id == window_id) {
            windows[i].focused = true;
            if (windows[i].layer) {
                int max_z = 0;
                for (int j = 0; j < DISPLAY_MAX_LAYERS; j++) {
                    if (layers[j].enabled && layers[j].z_order > max_z) {
                        max_z = layers[j].z_order;
                    }
                }
                display_set_layer_zorder(windows[i].layer - layers, max_z + 1);
            }
            break;
        }
    }

    spin_unlock(&window_lock);
}

void display_composite_layers(void)
{
    if (!initialized) return;

    spin_lock(&display_lock);

    uint32_t* target = display_get_back_buffer();
    if (!target) {
        spin_unlock(&display_lock);
        return;
    }

    fb_info_t* fb = fb_get_info();
    if (!fb) {
        spin_unlock(&display_lock);
        return;
    }

    int sorted_layers[DISPLAY_MAX_LAYERS];
    int layer_count = 0;

    for (int i = 0; i < DISPLAY_MAX_LAYERS; i++) {
        if (layers[i].enabled) {
            sorted_layers[layer_count++] = i;
        }
    }

    for (int i = 0; i < layer_count - 1; i++) {
        for (int j = i + 1; j < layer_count; j++) {
            if (layers[sorted_layers[j]].z_order < layers[sorted_layers[i]].z_order) {
                int temp = sorted_layers[i];
                sorted_layers[i] = sorted_layers[j];
                sorted_layers[j] = temp;
            }
        }
    }

    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            uint32_t final_color = 0xFF000000;

            for (int i = layer_count - 1; i >= 0; i--) {
                display_layer_t* layer = &layers[sorted_layers[i]];
                int lx = x - layer->x_offset;
                int ly = y - layer->y_offset;

                if (lx >= 0 && lx < layer->width &&
                    ly >= 0 && ly < layer->height) {

                    uint32_t pixel = layer->buffer[ly * layer->width + lx];

                    if (layer->transparent && pixel == layer->transparent_color) {
                        continue;
                    }

                    if (layer->alpha < 1.0f) {
                        uint8_t src_a = (pixel >> 24) & 0xFF;
                        uint8_t src_r = (pixel >> 16) & 0xFF;
                        uint8_t src_g = (pixel >> 8) & 0xFF;
                        uint8_t src_b = pixel & 0xFF;

                        uint8_t dst_a = (final_color >> 24) & 0xFF;
                        uint8_t dst_r = (final_color >> 16) & 0xFF;
                        uint8_t dst_g = (final_color >> 8) & 0xFF;
                        uint8_t dst_b = final_color & 0xFF;

                        float alpha = (src_a / 255.0f) * layer->alpha;
                        float inv_alpha = 1.0f - alpha;

                        uint8_t out_a = (uint8_t)(src_a * alpha + dst_a * inv_alpha);
                        uint8_t out_r = (uint8_t)(src_r * alpha + dst_r * inv_alpha);
                        uint8_t out_g = (uint8_t)(src_g * alpha + dst_g * inv_alpha);
                        uint8_t out_b = (uint8_t)(src_b * alpha + dst_b * inv_alpha);

                        final_color = (out_a << 24) | (out_r << 16) |
                                      (out_g << 8) | out_b;
                    } else {
                        final_color = pixel;
                    }

                    break;
                }
            }

            target[y * fb->width + x] = final_color;
        }
    }

    disp_stats.composite_operations++;
    disp_stats.pixels_drawn += fb->width * fb->height;

    spin_unlock(&display_lock);
}

void display_fill_rect(int layer_id, int x, int y, int width, int height,
                       uint32_t color)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[layer_id].enabled || width <= 0 || height <= 0) return;

    display_layer_t* layer = &layers[layer_id];

    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > layer->width) width = layer->width - x;
    if (y + height > layer->height) height = layer->height - y;
    if (width <= 0 || height <= 0) return;

    for (int row = y; row < y + height; row++) {
        for (int col = x; col < x + width; col++) {
            layer->buffer[row * layer->width + col] = color;
        }
    }

    disp_stats.pixels_drawn += width * height;
}

void display_blit(int dest_layer_id, int dx, int dy, int src_layer_id,
                  int sx, int sy, int width, int height)
{
    if (!initialized || dest_layer_id < 0 || dest_layer_id >= DISPLAY_MAX_LAYERS ||
        src_layer_id < 0 || src_layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[dest_layer_id].enabled || !layers[src_layer_id].enabled ||
        width <= 0 || height <= 0) return;

    display_layer_t* dest = &layers[dest_layer_id];
    display_layer_t* src = &layers[src_layer_id];

    if (sx < 0) { dx -= sx; width += sx; sx = 0; }
    if (sy < 0) { dy -= sy; height += sy; sy = 0; }
    if (dx < 0) { sx -= dx; width += dx; dx = 0; }
    if (dy < 0) { sy -= dy; height += dy; dy = 0; }

    if (sx + width > src->width) width = src->width - sx;
    if (sy + height > src->height) height = src->height - sy;
    if (dx + width > dest->width) width = dest->width - dx;
    if (dy + height > dest->height) height = dest->height - dy;
    if (width <= 0 || height <= 0) return;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            uint32_t pixel = src->buffer[(sy + row) * src->width + (sx + col)];
            dest->buffer[(dy + row) * dest->width + (dx + col)] = pixel;
        }
    }

    disp_stats.blit_operations++;
    disp_stats.pixels_drawn += width * height;
}

void display_update_cursor(int x, int y)
{
    if (!initialized) return;

    spin_lock(&display_lock);

    display_cursor.x = x;
    display_cursor.y = y;

    if (display_cursor.blinking) {
        uint64_t now = get_current_time_ns();
        if (now - display_cursor.last_blink_time >= DISPLAY_CURSOR_BLINK_RATE * 1000000ULL) {
            display_cursor.blink_state = !display_cursor.blink_state;
            display_cursor.last_blink_time = now;
        }
    }

    spin_unlock(&display_lock);
}

void display_draw_char(int layer_id, int x, int y, char c, uint32_t fg_color,
                       uint32_t bg_color, int scale)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[layer_id].enabled || scale <= 0) return;

    display_layer_t* layer = &layers[layer_id];

    extern const uint8_t font_8x16[95][16];
    int idx = c - 32;
    if (idx < 0 || idx >= 95) return;

    const uint8_t* glyph = font_8x16[idx];

    for (int row = 0; row < 16 * scale; row++) {
        for (int col = 0; col < 8 * scale; col++) {
            int gx = col / scale;
            int gy = row / scale;

            if (glyph[gy] & (0x80 >> gx)) {
                if (x + col < layer->width && y + row < layer->height) {
                    layer->buffer[(y + row) * layer->width + (x + col)] = fg_color;
                }
            } else if (bg_color != 0x00000000) {
                if (x + col < layer->width && y + row < layer->height) {
                    layer->buffer[(y + row) * layer->width + (x + col)] = bg_color;
                }
            }
        }
    }

    disp_stats.text_chars_rendered++;
    disp_stats.pixels_drawn += 8 * 16 * scale * scale;
}

void display_draw_text(int layer_id, int x, int y, const char* text,
                       uint32_t fg_color, uint32_t bg_color, int scale)
{
    if (!initialized || !text || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[layer_id].enabled) return;

    int cx = x, cy = y;
    while (*text) {
        if (*text == '\n') {
            cx = x;
            cy += 16 * scale;
        } else if (*text == '\t') {
            cx += 8 * 4 * scale;
        } else {
            display_draw_char(layer_id, cx, cy, *text, fg_color, bg_color, scale);
            cx += 8 * scale;
        }
        text++;
    }
}

void display_draw_line(int layer_id, int x1, int y1, int x2, int y2,
                       uint32_t color)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[layer_id].enabled) return;

    display_layer_t* layer = &layers[layer_id];

    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    while (true) {
        if (x1 >= 0 && x1 < layer->width && y1 >= 0 && y1 < layer->height) {
            layer->buffer[y1 * layer->width + x1] = color;
        }
        disp_stats.pixels_drawn++;

        if (x1 == x2 && y1 == y2) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x1 += sx; }
        if (e2 < dy) { err += dx; y1 += sy; }
    }
}

void display_draw_circle(int layer_id, int cx, int cy, int radius,
                         uint32_t color, bool filled)
{
    if (!initialized || layer_id < 0 || layer_id >= DISPLAY_MAX_LAYERS ||
        !layers[layer_id].enabled || radius <= 0) return;

    display_layer_t* layer = &layers[layer_id];

    int x = 0, y = radius;
    int d = 3 - 2 * radius;

    auto plot_circle_points = [&](int px, int py) {
        if (filled) {
            display_draw_line(layer_id, cx - py, cy + px, cx + py, cy + px, color);
            display_draw_line(layer_id, cx - py, cy - px, cx + py, cy - px, color);
            display_draw_line(layer_id, cx - px, cy + py, cx + px, cy + py, color);
            display_draw_line(layer_id, cx - px, cy - py, cx + px, cy - py, color);
        } else {
            if (cx + px >= 0 && cx + px < layer->width &&
                cy + py >= 0 && cy + py < layer->height)
                layer->buffer[(cy + py) * layer->width + (cx + px)] = color;
            if (cx - px >= 0 && cx - px < layer->width &&
                cy + py >= 0 && cy + py < layer->height)
                layer->buffer[(cy + py) * layer->width + (cx - px)] = color;
            if (cx + px >= 0 && cx + px < layer->width &&
                cy - py >= 0 && cy - py < layer->height)
                layer->buffer[(cy - py) * layer->width + (cx + px)] = color;
            if (cx - px >= 0 && cx - px < layer->width &&
                cy - py >= 0 && cy - py < layer->height)
                layer->buffer[(cy - py) * layer->width + (cx - px)] = color;
            if (cx + py >= 0 && cx + py < layer->width &&
                cy + px >= 0 && cy + px < layer->height)
                layer->buffer[(cy + px) * layer->width + (cx + py)] = color;
            if (cx - py >= 0 && cx - py < layer->width &&
                cy + px >= 0 && cy + px < layer->height)
                layer->buffer[(cy + px) * layer->width + (cx - py)] = color;
            if (cx + py >= 0 && cx + py < layer->width &&
                cy - px >= 0 && cy - px < layer->height)
                layer->buffer[(cy - px) * layer->width + (cx + py)] = color;
            if (cx - py >= 0 && cx - py < layer->width &&
                cy - px >= 0 && cy - px < layer->height)
                layer->buffer[(cy - px) * layer->width + (cx - py)] = color;
        }
    };

    while (y >= x) {
        plot_circle_points(x, y);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

int display_get_stats(display_stats_t* stats)
{
    if (!stats || !initialized) return -EINVAL;

    spin_lock(&display_lock);
    memcpy(stats, &disp_stats, sizeof(display_stats_t));

    stats->active_windows = 0;
    spin_lock(&window_lock);
    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id != 0 && windows[i].visible) {
            stats->active_windows++;
        }
    }
    spin_unlock(&window_lock);

    spin_unlock(&display_lock);
    return 0;
}

void display_dump_info(void)
{
    display_stats_t stats;
    display_get_stats(&stats);

    printk("Display System Status:\n");
    printk("  Frames rendered:       %llu\n", stats.frames_rendered);
    printk("  Pixels drawn:          %llu\n", stats.pixels_drawn);
    printk("  Blit operations:       %llu\n", stats.blit_operations);
    printk("  Composite operations:  %llu\n", stats.composite_operations);
    printk("  Text chars rendered:   %llu\n", stats.text_chars_rendered);
    printk("  Window creations:      %llu\n", stats.window_creations);
    printk("  Window destructions:   %llu\n", stats.window_destructions);
    printk("  Layer switches:        %llu\n", stats.layer_switches);
    printk("  VSync count:           %llu\n", stats.vsync_count);
    printk("  Frame time:            %llu us\n", stats.frame_time_ns / 1000ULL);
    printk("  Min frame time:        %llu us\n", stats.min_frame_time_ns / 1000ULL);
    printk("  Max frame time:        %llu us\n", stats.max_frame_time_ns / 1000ULL);
    printk("  Average FPS:           %.1f\n", stats.avg_fps);
    printk("  Current FPS:           %.1f\n", stats.current_fps);
    printk("  Memory used:           %llu KB\n", stats.memory_used / 1024ULL);
    printk("  Active windows:        %d\n", stats.active_windows);
    printk("  Active layers:         %d\n", stats.active_layers);
    printk("  Double buffering:      %s\n", double_buffer_enabled ? "enabled" : "disabled");
    printk("  VSync:                 %s\n", vsync_enabled ? "enabled" : "disabled");

    printk("\nActive Layers:\n");
    for (int i = 0; i < DISPLAY_MAX_LAYERS; i++) {
        if (layers[i].enabled) {
            printk("  Layer %d (%s): %dx%d @ (%d,%d) z=%d alpha=%.2f\n",
                   i, layers[i].name, layers[i].width, layers[i].height,
                   layers[i].x_offset, layers[i].y_offset,
                   layers[i].z_order, layers[i].alpha);
        }
    }

    printk("\nActive Windows:\n");
    spin_lock(&window_lock);
    for (int i = 0; i < DISPLAY_MAX_WINDOWS; i++) {
        if (windows[i].id != 0) {
            printk("  Window %u '%s': %dx%d @ (%d,%d) %s%s\n",
                   windows[i].id, windows[i].title,
                   windows[i].width, windows[i].height,
                   windows[i].x, windows[i].y,
                   windows[i].visible ? "visible" : "hidden",
                   windows[i].focused ? " [FOCUSED]" : "");
        }
    }
    spin_unlock(&window_lock);
}