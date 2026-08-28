#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vga.h>

#define MAX_LINES 256
#define MAX_LINE_LEN 128
#define MAX_FILE_SIZE 65536

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_x;
    int scroll_y;
    char filename[256];
    int modified;
    int running;
} EditorState;

static EditorState editor;

void editor_init(void) {
    memset(&editor, 0, sizeof(editor));
    editor.line_count = 1;
    editor.running = 1;
}

int editor_load_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    
    strncpy(editor.filename, path, sizeof(editor.filename) - 1);
    editor.line_count = 0;
    
    while (editor.line_count < MAX_LINES && 
           fgets(editor.lines[editor.line_count], MAX_LINE_LEN, f)) {
        
        size_t len = strlen(editor.lines[editor.line_count]);
        if (len > 0 && editor.lines[editor.line_count][len-1] == '\n') {
            editor.lines[editor.line_count][len-1] = '\0';
        }
        editor.line_count++;
    }
    
    if (editor.line_count == 0) {
        editor.line_count = 1;
        editor.lines[0][0] = '\0';
    }
    
    fclose(f);
    editor.modified = 0;
    return 0;
}

int editor_save_file(void) {
    if (editor.filename[0] == '\0') return -1;
    
    FILE* f = fopen(editor.filename, "w");
    if (!f) return -1;
    
    for (int i = 0; i < editor.line_count; i++) {
        fprintf(f, "%s\n", editor.lines[i]);
    }
    
    fclose(f);
    editor.modified = 0;
    return 0;
}

void editor_insert_char(char c) {
    char* line = editor.lines[editor.cursor_y];
    int len = strlen(line);
    
    if (len < MAX_LINE_LEN - 1) {
        for (int i = len; i > editor.cursor_x; i--) {
            line[i] = line[i-1];
        }
        line[editor.cursor_x] = c;
        line[len + 1] = '\0';
        editor.cursor_x++;
        editor.modified = 1;
    }
}

void editor_delete_char(void) {
    if (editor.cursor_x > 0) {
        char* line = editor.lines[editor.cursor_y];
        int len = strlen(line);
        
        for (int i = editor.cursor_x - 1; i < len; i++) {
            line[i] = line[i+1];
        }
        editor.cursor_x--;
        editor.modified = 1;
    } else if (editor.cursor_y > 0) {
        int prev_len = strlen(editor.lines[editor.cursor_y - 1]);
        strcat(editor.lines[editor.cursor_y - 1], editor.lines[editor.cursor_y]);
        
        for (int i = editor.cursor_y; i < editor.line_count - 1; i++) {
            strcpy(editor.lines[i], editor.lines[i + 1]);
        }
        
        editor.line_count--;
        editor.cursor_y--;
        editor.cursor_x = prev_len;
        editor.modified = 1;
    }
}

void editor_split_line(void) {
    if (editor.line_count >= MAX_LINES) return;
    
    char* current = editor.lines[editor.cursor_y];
    int len = strlen(current);
    
    for (int i = editor.line_count; i > editor.cursor_y + 1; i--) {
        strcpy(editor.lines[i], editor.lines[i-1]);
    }
    
    strncpy(editor.lines[editor.cursor_y + 1], current + editor.cursor_x, MAX_LINE_LEN - 1);
    current[editor.cursor_x] = '\0';
    
    editor.line_count++;
    editor.cursor_y++;
    editor.cursor_x = 0;
    editor.modified = 1;
}

void editor_move_cursor(int dx, int dy) {
    int new_y = editor.cursor_y + dy;
    
    if (new_y >= 0 && new_y < editor.line_count) {
        editor.cursor_y = new_y;
    }
    
    int max_x = strlen(editor.lines[editor.cursor_y]);
    int new_x = editor.cursor_x + dx;
    
    if (dx > 0) {
        editor.cursor_x = (new_x <= max_x) ? new_x : max_x;
    } else if (dx < 0) {
        editor.cursor_x = (new_x >= 0) ? new_x : 0;
    } else {
        if (editor.cursor_x > max_x) {
            editor.cursor_x = max_x;
        }
    }
}

void editor_draw(void) {
    vga_clear();
    
    vga_setcolor(0x1F, 0);
    char title[300];
    snprintf(title, sizeof(title), " Kenux Notepad - %s%s ", 
             editor.filename[0] ? editor.filename : "(untitled)",
             editor.modified ? "*" : "");
    vga_print(title);
    vga_setcolor(0x07, 0);
    vga_print("\n\n");
    
    int visible_lines = VGA_HEIGHT - 5;
    
    for (int i = 0; i < visible_lines; i++) {
        int line_idx = editor.scroll_y + i;
        
        char linenum[8];
        snprintf(linenum, sizeof(linenum), "%4d |", line_idx + 1);
        vga_setcolor(0x08, 0);
        vga_print(linenum);
        
        if (line_idx < editor.line_count) {
            const char* line = editor.lines[line_idx];
            
            if (line_idx == editor.cursor_y) {
                vga_setcolor(0x70, 0);
                
                int start = editor.scroll_x;
                int visible_len = VGA_WIDTH - 7;
                
                for (int j = start; j < start + visible_len && line[j]; j++) {
                    if (j == editor.cursor_x) {
                        vga_setcolor(0xF0, 0);
                        vga_print_char(vga_get_cursor_x(), vga_get_cursor_y(), line[j]);
                        vga_setcolor(0x70, 0);
                    } else {
                        vga_print_char(vga_get_cursor_x(), vga_get_cursor_y(), line[j]);
                    }
                }
                
                if (editor.cursor_x >= start && 
                    editor.cursor_x < start + visible_len &&
                    !line[editor.cursor_x]) {
                    vga_setcolor(0xF0, 0);
                    vga_print(" ");
                }
            } else {
                vga_setcolor(0x07, 0);
                vga_print(line + editor.scroll_x);
            }
        }
        
        vga_print("\n");
    }
    
    vga_setcolor(0x17, 0);
    vga_print("\n ");
    for (int i = 0; i < VGA_WIDTH - 2; i++) vga_print("-");
    
    vga_setcolor(0x0A, 0);
    vga_print("\n Line: ");
    char info[64];
    sprintf(info, "%d/%d  Col: %d  Lines: %d  Size: ~%d bytes",
            editor.cursor_y + 1, editor.line_count, 
            editor.cursor_x, editor.line_count,
            editor.modified ? -1 : 0);
    vga_print(info);
    
    vga_setcolor(0x0B, 0);
    vga_print("\n ^P:Save  ^O:Open  ^Q:Quit  ^S:Search  ^G:GoLine  F3:FindNext");
}

void editor_handle_key(char key) {
    static char search_pattern[MAX_LINE_LEN];
    static int search_active = 0;
    static int search_pos_y = 0;
    static int search_pos_x = 0;
    
    switch (key) {
        case 15: case 'o': case 'O':
            if (key == 15 || key == 'O') {
                vga_setcolor(0x0E, 0);
                vga_print("\nOpen file: ");
                
                char path[256];
                if (gets_s(path, sizeof(path))) {
                    editor_load_file(path);
                }
            }
            break;
            
        case 16: case 'p': case 'P':
            if (key == 16 || key == 'P') {
                if (editor_save_file() == 0) {
                    vga_setcolor(0x0A, 0);
                    vga_print("\nFile saved successfully!");
                } else {
                    vga_setcolor(0x0C, 0);
                    vga_print("\nError saving file!");
                }
            }
            break;
            
        case 17: case 'q': case 'Q':
            if (key == 17 || key == 'Q') {
                if (editor.modified) {
                    vga_setcolor(0x0E, 0);
                    vga_print("\nSave changes? (y/n): ");
                    
                    char choice;
                    vga_getc(&choice);
                    
                    if (choice == 'y' || choice == 'Y') {
                        editor_save_file();
                    }
                }
                editor.running = 0;
            }
            break;
            
        case 19: case 's': case 'S':
            if (key == 19 || key == 'S') {
                vga_setcolor(0x0E, 0);
                vga_print("\nSearch: ");
                
                gets_s(search_pattern, sizeof(search_pattern));
                search_active = 1;
                search_pos_y = editor.cursor_y;
                search_pos_x = editor.cursor_x;
            }
            break;
            
        case 7: case 'g': case 'G':
            if (key == 7 || key == 'G') {
                vga_setcolor(0x0E, 0);
                vga_print("\nGo to line: ");
                
                char num_str[16];
                gets_s(num_str, sizeof(num_str));
                
                int line_num = atoi(num_str);
                if (line_num > 0 && line_num <= editor.line_count) {
                    editor.cursor_y = line_num - 1;
                    editor.cursor_x = 0;
                }
            }
            break;
            
        case 0:
            char ext_key;
            vga_getc(&ext_key);
            
            switch (ext_key) {
                case 72: editor_move_cursor(0, -1); break;
                case 80: editor_move_cursor(0, 1); break;
                case 75: editor_move_cursor(-1, 0); break;
                case 77: editor_move_cursor(1, 0); break;
                case 71: editor.cursor_x = 0; break;
                case 79: 
                    editor.cursor_x = strlen(editor.lines[editor.cursor_y]); 
                    break;
                case 73: editor.scroll_y -= 10; break;
                case 81: editor.scroll_y += 10; break;
                case 83: editor_delete_char(); break;
                case 61: 
                    if (search_active) {
                        for (int y = search_pos_y; y < editor.line_count; y++) {
                            int start_x = (y == search_pos_y) ? search_pos_x : 0;
                            char* found = strstr(editor.lines[y] + start_x, search_pattern);
                            
                            if (found) {
                                editor.cursor_y = y;
                                editor.cursor_x = found - editor.lines[y];
                                search_pos_y = y;
                                search_pos_x = editor.cursor_x + 1;
                                
                                vga_setcolor(0x0A, 0);
                                vga_print("\nFound!");
                                break;
                            }
                        }
                    }
                    break;
            }
            break;
            
        case '\r':
        case '\n':
            editor_split_line();
            break;
            
        case '\b':
        case 127:
            editor_delete_char();
            break;
            
        case '\t':
            for (int i = 0; i < 4; i++) {
                editor_insert_char(' ');
            }
            break;
            
        default:
            if (key >= 32 && key < 127) {
                editor_insert_char(key);
            }
            break;
    }
    
    if (editor.cursor_y - editor.scroll_y >= VGA_HEIGHT - 5) {
        editor.scroll_y = editor.cursor_y - (VGA_HEIGHT - 6);
    }
    if (editor.cursor_y < editor.scroll_y) {
        editor.scroll_y = editor.cursor_y;
    }
    
    if (editor.cursor_x - editor.scroll_x >= VGA_WIDTH - 7) {
        editor.scroll_x = editor.cursor_x - (VGA_WIDTH - 8);
    }
    if (editor.cursor_x < editor.scroll_x) {
        editor.scroll_x = editor.cursor_x;
    }
}

void notepad_run(int argc, char** argv) {
    editor_init();
    
    if (argc > 1) {
        if (editor_load_file(argv[1]) != 0) {
            vga_setcolor(0x0C, 0);
            vga_print("Creating new file: ");
            vga_print(argv[1]);
            strncpy(editor.filename, argv[1], sizeof(editor.filename) - 1);
        }
    }
    
    while (editor.running) {
        editor_draw();
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            editor_handle_key(key);
        }
    }
    
    vga_clear();
    vga_setcolor(0x07, 0);
    vga_print("Notepad closed.\n");
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Notepad...\n");
    notepad_run(argc, argv);
    return 0;
}