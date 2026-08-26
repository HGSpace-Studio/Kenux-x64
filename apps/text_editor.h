#ifndef APPS_TEXT_EDITOR_H
#define APPS_TEXT_EDITOR_H

#include <arch/types.h>

#define TEXT_EDITOR_MAX_LINES 100
#define TEXT_EDITOR_MAX_LINE_LEN 256
#define TEXT_EDITOR_MAX_UNDO 50

typedef struct {
    char lines[TEXT_EDITOR_MAX_LINES][TEXT_EDITOR_MAX_LINE_LEN];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_x;
    int scroll_y;
    int undo_stack[TEXT_EDITOR_MAX_UNDO];
    int undo_count;
    char filename[256];
    uint8_t syntax_highlighting;
} text_editor_t;

void text_editor_init(text_editor_t* editor);
void text_editor_load(text_editor_t* editor, const char* filename);
void text_editor_save(text_editor_t* editor, const char* filename);
void text_editor_insert_char(text_editor_t* editor, char c);
void text_editor_delete_char(text_editor_t* editor);
void text_editor_newline(text_editor_t* editor);
void text_editor_backspace(text_editor_t* editor);
void text_editor_move_cursor(text_editor_t* editor, int dx, int dy);
void text_editor_draw(text_editor_t* editor);
void text_editor_run(void);

#endif
