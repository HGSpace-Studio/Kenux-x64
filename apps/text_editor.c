#include <text_editor.h>
#include <stdio.h>
#include <string.h>
#include <vga.h>
#include <mouse.h>
#include <shell.h>

void text_editor_init(text_editor_t* editor)
{
    memset(editor, 0, sizeof(text_editor_t));
    editor->line_count = 1;
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->scroll_x = 0;
    editor->scroll_y = 0;
    editor->undo_count = 0;
    editor->syntax_highlighting = 1;
    editor->filename[0] = '\0';
}

void text_editor_load(text_editor_t* editor, const char* filename)
{
    strncpy(editor->filename, filename, 255);
    editor->filename[255] = '\0';
    
    editor->line_count = 1;
    editor->lines[0][0] = '\0';
}

void text_editor_save(text_editor_t* editor, const char* filename)
{
    if (filename) {
        strncpy(editor->filename, filename, 255);
    }
    
    vga_print("File saved: ");
    vga_print(editor->filename);
    vga_print("\n");
}

void text_editor_insert_char(text_editor_t* editor, char c)
{
    if (editor->cursor_y >= editor->line_count) {
        editor->line_count = editor->cursor_y + 1;
    }
    
    if (editor->cursor_x < TEXT_EDITOR_MAX_LINE_LEN - 1) {
        memmove(&editor->lines[editor->cursor_y][editor->cursor_x + 1],
                &editor->lines[editor->cursor_y][editor->cursor_x],
                strlen(&editor->lines[editor->cursor_y][editor->cursor_x]) + 1);
        
        editor->lines[editor->cursor_y][editor->cursor_x] = c;
        editor->cursor_x++;
    }
}

void text_editor_delete_char(text_editor_t* editor)
{
    if (editor->cursor_x > 0) {
        memmove(&editor->lines[editor->cursor_y][editor->cursor_x - 1],
                &editor->lines[editor->cursor_y][editor->cursor_x],
                strlen(&editor->lines[editor->cursor_y][editor->cursor_x]) + 1);
        editor->cursor_x--;
    } else if (editor->cursor_y > 0 && editor->cursor_x == 0) {
        int prev_len = strlen(editor->lines[editor->cursor_y - 1]);
        memmove(&editor->lines[editor->cursor_y - 1][prev_len],
                &editor->lines[editor->cursor_y][0],
                strlen(&editor->lines[editor->cursor_y][0]) + 1);
        
        editor->cursor_x = prev_len;
        
        for (int i = editor->cursor_y; i < editor->line_count - 1; i++) {
            strcpy(editor->lines[i], editor->lines[i + 1]);
        }
        editor->line_count--;
        editor->cursor_y--;
    }
}

void text_editor_newline(text_editor_t* editor)
{
    if (editor->cursor_y >= editor->line_count - 1) {
        editor->line_count++;
    }
    
    char temp[TEXT_EDITOR_MAX_LINE_LEN];
    strcpy(temp, &editor->lines[editor->cursor_y][editor->cursor_x]);
    
    memmove(&editor->lines[editor->cursor_y + 1][0],
            &editor->lines[editor->cursor_y][editor->cursor_x],
            strlen(&editor->lines[editor->cursor_y][editor->cursor_x]) + 1);
    
    editor->lines[editor->cursor_y][editor->cursor_x] = '\0';
    
    editor->cursor_y++;
    editor->cursor_x = 0;
    
    strcpy(&editor->lines[editor->cursor_y][0], temp);
}

void text_editor_backspace(text_editor_t* editor)
{
    text_editor_delete_char(editor);
}

void text_editor_move_cursor(text_editor_t* editor, int dx, int dy)
{
    editor->cursor_x += dx;
    editor->cursor_y += dy;
    
    if (editor->cursor_x < 0) editor->cursor_x = 0;
    if (editor->cursor_y < 0) editor->cursor_y = 0;
    if (editor->cursor_y >= editor->line_count) editor->cursor_y = editor->line_count - 1;
    if (editor->cursor_x >= strlen(editor->lines[editor->cursor_y])) editor->cursor_x = strlen(editor->lines[editor->cursor_y]);
}

void text_editor_draw(text_editor_t* editor)
{
    vga_setcolor(0x0F, 0x1E);
    
    int start_y = editor->scroll_y;
    int end_y = start_y + 23;
    
    if (end_y > editor->line_count) end_y = editor->line_count;
    
    for (int y = start_y; y < end_y; y++) {
        char line[256];
        strncpy(line, editor->lines[y], 255);
        line[255] = '\0';
        
        vga_setcolor(0x0F, 0x1E);
        vga_print(line);
        
        for (int i = strlen(line); i < 78; i++) {
            vga_print(" ");
        }
        
        vga_print("\n");
    }
    
    vga_setcolor(0x0F, 0x1F);
    vga_setcolor(0x0F, 0x00);
    vga_print(" ");
    vga_setcolor(0x0F, 0x1F);
    vga_print(" ");
}

void text_editor_run(void)
{
    text_editor_t editor;
    text_editor_init(&editor);
    
    vga_clear();
    vga_print("Kenux Text Editor v1.0\n");
    vga_print("Commands:\n");
    vga_print("  Type text to edit\n");
    vga_print("  Enter: New line\n");
    vga_print("  Backspace: Delete character\n");
    vga_print("  Ctrl+S: Save\n");
    vga_print("  Ctrl+Q: Quit\n\n");
    
    text_editor_load(&editor, "Untitled.txt");
    
    while (1) {
        text_editor_draw(&editor);
        
        char c = vga_getchar();
        
        if (c == '\n') {
            text_editor_newline(&editor);
        } else if (c == '\b' || c == 127) {
            text_editor_backspace(&editor);
        } else if (c >= 32 && c < 127) {
            text_editor_insert_char(&editor, c);
        } else if (c == 19) {
            text_editor_save(&editor, editor.filename);
        } else if (c == 17) {
            break;
        }
    }
}
