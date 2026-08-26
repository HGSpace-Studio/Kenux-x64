#include <arch/desktop.h>
#include <arch/vga.h>
#include <string.h>
#include <memory.h>
#include <kenuxapi.h>

#define TERMINAL_BUFFER_SIZE 4096
#define TERMINAL_HISTORY_SIZE 32

typedef struct {
    desktop_window_t* window;
    char buffer[TERMINAL_BUFFER_SIZE];
    int buffer_pos;
    char history[TERMINAL_HISTORY_SIZE][256];
    int history_count;
    int history_index;
    int cursor_x;
    int cursor_y;
    int scroll_top;
    char command[256];
    int command_pos;
} terminal_t;

static terminal_t terminal;

static void terminal_draw(void* data) {
    terminal_t* term = (terminal_t*)data;
    if (!term || !term->window) return;
    
    int win_x = term->window->x + 1;
    int win_y = term->window->y + 2;
    int win_w = term->window->width - 2;
    int win_h = term->window->height - 3;
    
    vga_setcolor(0x0F, 0x1F);
    for (int y = 0; y < win_h; y++) {
        for (int x = 0; x < win_w; x++) {
            int buf_idx = (term->scroll_top + y) * win_w + x;
            char c = (buf_idx < term->buffer_pos) ? term->buffer[buf_idx] : ' ';
            if (win_y + y < VGA_HEIGHT && win_x + x < VGA_WIDTH) {
                vga_buffer[(win_y + y) * VGA_WIDTH + (win_x + x)] = (0x07 << 8) | c;
            }
        }
    }
    
    vga_setcolor(0x0F, 0x0F);
    if (win_y + term->cursor_y - term->scroll_top < VGA_HEIGHT && 
        win_x + term->cursor_x < VGA_WIDTH) {
        vga_buffer[(win_y + term->cursor_y - term->scroll_top) * VGA_WIDTH + (win_x + term->cursor_x)] = 
            (0x0F << 8) | '_';
    }
}

static void terminal_process_input(char c) {
    int win_w = terminal.window->width - 2;
    int win_h = terminal.window->height - 3;
    
    if (c == '\n' || c == '\r') {
        terminal.buffer[terminal.buffer_pos++] = '\n';
        
        if (terminal.command_pos > 0) {
            strcpy(terminal.history[terminal.history_count % TERMINAL_HISTORY_SIZE], terminal.command);
            terminal.history_count++;
            terminal.history_index = terminal.history_count;
        }
        
        terminal.cursor_y++;
        terminal.cursor_x = 0;
        terminal.command_pos = 0;
        terminal.command[0] = '\0';
        
        terminal.buffer[terminal.buffer_pos++] = '>';
        terminal.buffer[terminal.buffer_pos++] = ' ';
        terminal.cursor_x = 2;
        
        if (terminal.cursor_y >= win_h + terminal.scroll_top) {
            terminal.scroll_top++;
        }
    } else if (c == '\b' || c == 0x7F) {
        if (terminal.buffer_pos > 0) {
            terminal.buffer_pos--;
            terminal.cursor_x--;
            if (terminal.cursor_x < 0) {
                terminal.cursor_x = win_w - 1;
                terminal.cursor_y--;
                if (terminal.cursor_y < terminal.scroll_top) {
                    terminal.scroll_top--;
                }
            }
        }
        if (terminal.command_pos > 0) {
            terminal.command_pos--;
            terminal.command[terminal.command_pos] = '\0';
        }
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            terminal.buffer[terminal.buffer_pos++] = ' ';
            terminal.command[terminal.command_pos++] = ' ';
            terminal.command[terminal.command_pos] = '\0';
            terminal.cursor_x++;
            if (terminal.cursor_x >= win_w) {
                terminal.cursor_x = 0;
                terminal.cursor_y++;
            }
        }
    } else {
        terminal.buffer[terminal.buffer_pos++] = c;
        terminal.command[terminal.command_pos++] = c;
        terminal.command[terminal.command_pos] = '\0';
        terminal.cursor_x++;
        
        if (terminal.cursor_x >= win_w) {
            terminal.cursor_x = 0;
            terminal.cursor_y++;
            
            if (terminal.cursor_y >= win_h + terminal.scroll_top) {
                terminal.scroll_top++;
            }
        }
    }
    
    if (terminal.buffer_pos >= TERMINAL_BUFFER_SIZE - 1) {
        terminal.buffer_pos = 0;
        terminal.scroll_top = 0;
        terminal.cursor_y = 0;
    }
}

static void terminal_execute_command(const char* cmd) {
    int win_w = terminal.window->width - 2;
    int win_h = terminal.window->height - 3;
    
    if (strcmp(cmd, "clear") == 0) {
        terminal.buffer_pos = 0;
        terminal.cursor_x = 0;
        terminal.cursor_y = 0;
        terminal.scroll_top = 0;
        terminal.buffer[terminal.buffer_pos++] = '>';
        terminal.buffer[terminal.buffer_pos++] = ' ';
        terminal.cursor_x = 2;
        return;
    }
    
    if (strcmp(cmd, "help") == 0) {
        const char* help_text = "\nAvailable commands:\n";
        const char* commands[] = {
            "help    - Show this help",
            "clear   - Clear terminal",
            "echo    - Print message",
            "ls      - List files",
            "pwd     - Print working directory",
            "whoami  - Show current user",
            "date    - Show current time",
            "exit    - Close terminal",
            "poweroff - Shutdown system",
            "reboot  - Reboot system",
            NULL
        };
        
        for (const char** p = commands; *p; p++) {
            for (const char* c = *p; *c; c++) {
                terminal.buffer[terminal.buffer_pos++] = *c;
            }
            terminal.buffer[terminal.buffer_pos++] = '\n';
            terminal.cursor_y++;
        }
        terminal.cursor_x = 0;
    } else if (strcmp(cmd, "pwd") == 0) {
        const char* pwd = "\n/home/user\n";
        for (const char* c = pwd; *c; c++) {
            terminal.buffer[terminal.buffer_pos++] = *c;
        }
        terminal.cursor_y++;
        terminal.cursor_x = 0;
    } else if (strcmp(cmd, "whoami") == 0) {
        const char* whoami = "\nroot\n";
        for (const char* c = whoami; *c; c++) {
            terminal.buffer[terminal.buffer_pos++] = *c;
        }
        terminal.cursor_y++;
        terminal.cursor_x = 0;
    } else if (strcmp(cmd, "date") == 0) {
        uint64_t hours, minutes, seconds;
        kenux_get_time(&hours, &minutes, &seconds);
        char date_str[40];
        sprintf(date_str, "\n%04d-%02d-%02d %02d:%02d:%02d\n", 2026, 6, 11, 
                (int)hours, (int)minutes, (int)seconds);
        for (int i = 0; date_str[i]; i++) {
            terminal.buffer[terminal.buffer_pos++] = date_str[i];
        }
        terminal.cursor_y++;
        terminal.cursor_x = 0;
    } else if (strcmp(cmd, "ls") == 0) {
        char buffer[1024];
        kenux_list_dir("/", buffer, sizeof(buffer));
        
        terminal.buffer[terminal.buffer_pos++] = '\n';
        for (int i = 0; buffer[i]; i++) {
            terminal.buffer[terminal.buffer_pos++] = buffer[i];
        }
        terminal.buffer[terminal.buffer_pos++] = '\n';
        terminal.cursor_y += 2;
        terminal.cursor_x = 0;
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal.buffer[terminal.buffer_pos++] = '\n';
        for (const char* c = cmd + 5; *c; c++) {
            terminal.buffer[terminal.buffer_pos++] = *c;
        }
        terminal.buffer[terminal.buffer_pos++] = '\n';
        terminal.cursor_y += 2;
        terminal.cursor_x = 0;
    } else if (strcmp(cmd, "exit") == 0) {
        desktop_destroy_window(terminal.window);
        return;
    } else if (strcmp(cmd, "poweroff") == 0) {
        kenux_poweroff();
        return;
    } else if (strcmp(cmd, "reboot") == 0) {
        kenux_reboot();
        return;
    } else if (strlen(cmd) > 0) {
        char err[64];
        sprintf(err, "\nUnknown command: %s\n", cmd);
        for (int i = 0; err[i]; i++) {
            terminal.buffer[terminal.buffer_pos++] = err[i];
        }
        terminal.cursor_y += 2;
        terminal.cursor_x = 0;
    }
    
    terminal.buffer[terminal.buffer_pos++] = '\n';
    terminal.cursor_y++;
    terminal.cursor_x = 0;
    
    terminal.buffer[terminal.buffer_pos++] = '>';
    terminal.buffer[terminal.buffer_pos++] = ' ';
    terminal.cursor_x = 2;
    
    if (terminal.cursor_y >= win_h + terminal.scroll_top) {
        terminal.scroll_top++;
    }
}

static void terminal_update(void* data) {
    terminal_t* term = (terminal_t*)data;
    if (!term || !term->window) return;
    
    char c = vga_getch();
    if (c != 0) {
        if (c == '\n') {
            terminal_execute_command(term->command);
        } else {
            terminal_process_input(c);
        }
    }
}

void terminal_run(void) {
    memset(&terminal, 0, sizeof(terminal));
    
    terminal.window = desktop_create_window(5, 3, 70, 18, "Terminal", WINDOW_FLAG_CLOSEABLE);
    if (!terminal.window) return;
    
    terminal.cursor_x = 2;
    terminal.cursor_y = 0;
    terminal.scroll_top = 0;
    terminal.buffer_pos = 0;
    terminal.command_pos = 0;
    terminal.history_count = 0;
    terminal.history_index = 0;
    
    terminal.buffer[terminal.buffer_pos++] = '>';
    terminal.buffer[terminal.buffer_pos++] = ' ';
    
    terminal.window->data = &terminal;
    terminal.window->draw = terminal_draw;
    terminal.window->update = terminal_update;
    
    desktop_activate_window(terminal.window);
}