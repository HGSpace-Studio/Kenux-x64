#include <arch/desktop.h>
#include <arch/vga.h>
#include <string.h>
#include <memory.h>
#include <fs.h>

#define FILE_MANAGER_BUFFER_SIZE 1024

typedef struct {
    desktop_window_t* window;
    char current_path[256];
    char file_list[FILE_MANAGER_BUFFER_SIZE];
    int file_count;
    int selected_index;
    int scroll_top;
} file_manager_t;

static file_manager_t file_manager;

static void file_manager_update_list(void) {
    file_manager.file_count = 0;
    file_manager.scroll_top = 0;
    file_manager.selected_index = 0;
    
    fs_list_dir(file_manager.current_path, file_manager.file_list, FILE_MANAGER_BUFFER_SIZE);
    
    int count = 0;
    char* token = strtok(file_manager.file_list, "\n");
    while (token) {
        count++;
        token = strtok(NULL, "\n");
    }
    file_manager.file_count = count;
}

static void file_manager_draw(void* data) {
    file_manager_t* fm = (file_manager_t*)data;
    if (!fm || !fm->window) return;
    
    int win_x = fm->window->x + 1;
    int win_y = fm->window->y + 2;
    int win_w = fm->window->width - 2;
    int win_h = fm->window->height - 3;
    
    vga_setcolor(0x0F, 0x1F);
    for (int y = 0; y < win_h; y++) {
        for (int x = 0; x < win_w; x++) {
            if (win_y + y < VGA_HEIGHT && win_x + x < VGA_WIDTH) {
                vga_buffer[(win_y + y) * VGA_WIDTH + (win_x + x)] = (0x1F << 8) | ' ';
            }
        }
    }
    
    vga_setcolor(0x0F, 0x07);
    for (int i = 0; i < strlen(fm->current_path) && i < win_w; i++) {
        if (win_y < VGA_HEIGHT && win_x + i < VGA_WIDTH) {
            vga_buffer[win_y * VGA_WIDTH + win_x + i] = (0x07 << 8) | fm->current_path[i];
        }
    }
    
    vga_setcolor(0x0F, 0x17);
    for (int x = 0; x < win_w; x++) {
        if (win_y + 1 < VGA_HEIGHT && win_x + x < VGA_WIDTH) {
            vga_buffer[(win_y + 1) * VGA_WIDTH + win_x + x] = (0x17 << 8) | '-';
        }
    }
    
    char list_copy[FILE_MANAGER_BUFFER_SIZE];
    strcpy(list_copy, fm->file_list);
    
    int item_y = 0;
    char* token = strtok(list_copy, "\n");
    while (token && item_y < fm->scroll_top + win_h - 2) {
        if (item_y >= fm->scroll_top) {
            int display_y = item_y - fm->scroll_top + 2;
            if (display_y < win_h - 1) {
                uint8_t color = (item_y == fm->selected_index) ? 0x0F : 0x07;
                
                for (int x = 0; x < win_w; x++) {
                    if (win_y + display_y < VGA_HEIGHT && win_x + x < VGA_WIDTH) {
                        vga_buffer[(win_y + display_y) * VGA_WIDTH + win_x + x] = 
                            ((item_y == fm->selected_index ? 0x1A : 0x1F) << 8) | ' ';
                    }
                }
                
                for (int i = 0; i < strlen(token) && i < win_w; i++) {
                    if (win_y + display_y < VGA_HEIGHT && win_x + i < VGA_WIDTH) {
                        vga_buffer[(win_y + display_y) * VGA_WIDTH + win_x + i] = (color << 8) | token[i];
                    }
                }
            }
        }
        item_y++;
        token = strtok(NULL, "\n");
    }
}

static void file_manager_update(void* data) {
    file_manager_t* fm = (file_manager_t*)data;
    if (!fm || !fm->window) return;
    
    char c = vga_getch();
    if (c != 0) {
        if (c == '\n' || c == '\r') {
            char list_copy[FILE_MANAGER_BUFFER_SIZE];
            strcpy(list_copy, fm->file_list);
            
            int index = 0;
            char* token = strtok(list_copy, "\n");
            while (token && index < fm->selected_index) {
                index++;
                token = strtok(NULL, "\n");
            }
            
            if (token) {
                size_t len = strlen(token);
                if (len > 0 && token[len - 1] == '/') {
                    if (strcmp(token, "..") == 0) {
                        char* last_slash = strrchr(fm->current_path, '/');
                        if (last_slash && last_slash != fm->current_path) {
                            *last_slash = '\0';
                        }
                    } else {
                        strcat(fm->current_path, token);
                    }
                    file_manager_update_list();
                }
            }
        } else if (c == '\b' || c == 0x7F) {
            char* last_slash = strrchr(fm->current_path, '/');
            if (last_slash && last_slash != fm->current_path) {
                *last_slash = '\0';
            }
            file_manager_update_list();
        } else if (c == 0x1B) {
            char next = vga_getch();
            if (next == '[') {
                char dir = vga_getch();
                if (dir == 'A') {
                    if (fm->selected_index > 0) {
                        fm->selected_index--;
                        if (fm->selected_index < fm->scroll_top) {
                            fm->scroll_top--;
                        }
                    }
                } else if (dir == 'B') {
                    if (fm->selected_index < fm->file_count - 1) {
                        fm->selected_index++;
                        int win_h = fm->window->height - 3;
                        if (fm->selected_index >= fm->scroll_top + win_h - 2) {
                            fm->scroll_top++;
                        }
                    }
                }
            }
        }
    }
}

void file_manager_run(void) {
    memset(&file_manager, 0, sizeof(file_manager));
    
    file_manager.window = desktop_create_window(5, 3, 70, 18, "File Manager", WINDOW_FLAG_CLOSEABLE);
    if (!file_manager.window) return;
    
    strcpy(file_manager.current_path, "/");
    file_manager.selected_index = 0;
    file_manager.scroll_top = 0;
    
    file_manager_update_list();
    
    file_manager.window->data = &file_manager;
    file_manager.window->draw = file_manager_draw;
    file_manager.window->update = file_manager_update;
    
    desktop_activate_window(file_manager.window);
}