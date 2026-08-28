#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define MAX_FILES 128
#define MAX_NAME_LEN 256
#define MAX_PATH_LEN 512

typedef enum {
    FILE_TYPE_DIR,
    FILE_TYPE_FILE,
    FILE_TYPE_LINK,
    FILE_TYPE_UNKNOWN
} FileType;

typedef struct {
    char name[MAX_NAME_LEN];
    FileType type;
    long size;
    time_t modified;
    int permissions;
} FileInfo;

static FileInfo files[MAX_FILES];
static int file_count = 0;
static int selected_file = 0;
static char current_path[MAX_PATH_LEN] = "/root";
static int show_hidden = 0;

typedef struct {
    char name[MAX_NAME_LEN];
    long size;
    time_t mod_time;
} ClipboardItem;

static ClipboardItem clipboard;
static int clipboard_has_content = 0;
static int clipboard_is_cut = 0;

static void generate_sample_files(void) {
    file_count = 12;
    
    strcpy(files[0].name, "..");
    files[0].type = FILE_TYPE_DIR;
    files[0].size = 4096;
    
    strcpy(files[1].name, "Desktop");
    files[1].type = FILE_TYPE_DIR;
    files[1].size = 4096;
    files[1].modified = time(NULL) - 86400 * 2;
    
    strcpy(files[2].name, "Documents");
    files[2].type = FILE_TYPE_DIR;
    files[2].size = 4096;
    files[2].modified = time(NULL) - 3600;
    
    strcpy(files[3].name, "Downloads");
    files[3].type = FILE_TYPE_DIR;
    files[3].size = 4096;
    files[3].modified = time(NULL) - 7200;
    
    strcpy(files[4].name, ".config");
    files[4].type = FILE_TYPE_DIR;
    files[4].size = 4096;
    files[4].permissions = 0755;
    
    strcpy(files[5].name, ".bashrc");
    files[5].type = FILE_TYPE_FILE;
    files[5].size = 512;
    files[5].modified = time(NULL) - 604800;
    files[5].permissions = 0644;
    
    strcpy(files[6].name, "readme.txt");
    files[6].type = FILE_TYPE_FILE;
    files[6].size = 8192;
    files[6].modified = time(NULL) - 1800;
    files[6].permissions = 0644;
    
    strcpy(files[7].name, "notes.md");
    files[7].type = FILE_TYPE_FILE;
    files[7].size = 1536;
    files[7].modified = time(NULL) - 900;
    files[7].permissions = 0644;
    
    strcpy(files[8].name, "image.png");
    files[8].type = FILE_TYPE_FILE;
    files[8].size = 1048576;
    files[8].modified = time(NULL) - 3600;
    files[8].permissions = 0644;
    
    strcpy(files[9].name, "backup.tar.gz");
    files[9].type = FILE_TYPE_FILE;
    files[9].size = 52428800;
    files[9].modified = time(NULL) - 86400;
    files[9].permissions = 0644;
    
    strcpy(files[10].name, "script.sh");
    files[10].type = FILE_TYPE_FILE;
    files[10].size = 4096;
    files[10].modified = time(NULL) - 600;
    files[10].permissions = 0755;
    
    strcpy(files[11].name, ".hidden_file");
    files[11].type = FILE_TYPE_FILE;
    files[11].size = 256;
    files[11].modified = time(NULL) - 172800;
    files[11].permissions = 0600;
}

static void format_size(long size, char* buf, size_t buf_size) {
    if (size < 1024) {
        snprintf(buf, buf_size, "%ld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, buf_size, "%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buf, buf_size, "%.1f MB", size / (1024.0 * 1024));
    } else {
        snprintf(buf, buf_size, "%.1f GB", size / (1024.0 * 1024 * 1024));
    }
}

static void format_time(time_t t, char* buf, size_t buf_size) {
    struct tm* tm_info = localtime(&t);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M", tm_info);
}

static const char* get_type_icon(FileType type) {
    switch (type) {
        case FILE_TYPE_DIR:   return "[DIR] ";
        case FILE_TYPE_FILE:  return "[FILE]";
        case FILE_TYPE_LINK:  return "[LINK]";
        default:              return "[???] ";
    }
}

static void draw_header(void) {
    vga_clear();
    
    vga_setcolor(0x1F, 0);
    vga_print(" Kenux File Manager v2.0 ");
    vga_setcolor(0x17, 0);
    
    for (int i = 24; i < VGA_WIDTH; i++) {
        vga_print("=");
    }
    
    vga_print("\n\n");
    
    vga_setcolor(0x0B, 0);
    vga_print("Path: ");
    vga_setcolor(0x0F, 0);
    vga_print(current_path);
    
    vga_setcolor(0x08, 0);
    char info[64];
    sprintf(info, " (%d items)\n", file_count);
    vga_print(info);
}

static void draw_toolbar(void) {
    vga_setcolor(0x09, 0);
    vga_print("\n [N]New [D]Delete [C]Copy [X]Cut [V]Paste [R]Rename [H]Hidden [Q]Quit\n");
}

static void draw_file_list(void) {
    vga_setcolor(0x09, 0);
    printf(" %-20s %-12s %-18s %s\n", 
           "Name", "Size", "Modified", "Permissions");
    vga_setcolor(0x07, 0);
    vga_print("-------------------- ------------ ------------------ ----------\n");
    
    int visible_start = 0;
    int visible_count = VGA_HEIGHT - 10;
    
    if (selected_file >= visible_start + visible_count) {
        visible_start = selected_file - visible_count + 1;
    }
    
    for (int i = visible_start; i < file_count && i < visible_start + visible_count; i++) {
        if (i == selected_file) {
            vga_setcolor(0x70, 0);
        } else {
            vga_setcolor(0x07, 0);
        }
        
        if (!show_hidden && files[i].name[0] == '.' && strcmp(files[i].name, "..") != 0) {
            continue;
        }
        
        char size_str[16], time_str[20], perm_str[12];
        format_size(files[i].size, size_str, sizeof(size_str));
        format_time(files[i].modified, time_str, sizeof(time_str));
        sprintf(perm_str, "%o", files[i].permissions & 0777);
        
        printf(" %-20s %-12s %-18s %s\n",
               get_type_icon(files[i].type),
               size_str,
               time_str,
               perm_str);
        
        vga_print("  ");
        vga_print(files[i].name);
        vga_print("\n");
    }
}

static void draw_status_bar(void) {
    vga_setcolor(0x17, 0);
    vga_print("\n ");
    for (int i = 1; i < VGA_WIDTH; i++) vga_print("-");
    
    if (selected_file >= 0 && selected_file < file_count) {
        vga_setcolor(0x0E, 0);
        vga_print("\n Selected: ");
        vga_print(files[selected_file].name);
        
        char size_str[16];
        format_size(files[selected_file].size, size_str, sizeof(size_str));
        vga_print(" | Size: ");
        vga_print(size_str);
        
        if (clipboard_has_content) {
            vga_setcolor(0x0A, 0);
            vga_print(" | Clipboard: ");
            vga_print(clipboard.name);
            vga_print(clipboard_is_cut ? " (CUT)" : " (COPY)");
        }
    }
    
    vga_print("\n");
}

void file_manager_run(void) {
    srand(time(NULL));
    generate_sample_files();
    
    int running = 1;
    
    while (running) {
        draw_header();
        draw_file_list();
        draw_status_bar();
        draw_toolbar();
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            switch (key) {
                case 72: case 'w': case 'W':
                    if (selected_file > 0) selected_file--;
                    break;
                    
                case 80: case 's': case 'S':
                    if (selected_file < file_count - 1) selected_file++;
                    break;
                    
                case '\r': case ' ':
                    if (files[selected_file].type == FILE_TYPE_DIR) {
                        if (strcmp(files[selected_file].name, "..") == 0) {
                            char* last_slash = strrchr(current_path, '/');
                            if (last_slash && last_slash != current_path) {
                                *last_slash = '\0';
                                if (strlen(current_path) == 0) strcpy(current_path, "/");
                            }
                        } else {
                            if (strcmp(current_path, "/") == 0) {
                                strcat(current_path, files[selected_file].name);
                            } else {
                                strcat(current_path, "/");
                                strcat(current_path, files[selected_file].name);
                            }
                        }
                        generate_sample_files();
                        selected_file = 0;
                    } else {
                        vga_setcolor(0x0A, 0);
                        vga_print("\nOpening: ");
                        vga_print(files[selected_file].name);
                        vga_print("\n[File opened in associated application]\n");
                        
                        char dummy;
                        vga_getc(&dummy);
                    }
                    break;
                    
                case 'h': case 'H':
                    show_hidden = !show_hidden;
                    break;
                    
                case 'c': case 'C':
                    if (selected_file > 0) {
                        clipboard = (ClipboardItem){
                            .name = {0},
                            .size = files[selected_file].size,
                            .mod_time = files[selected_file].modified
                        };
                        strncpy(clipboard.name, files[selected_file].name, MAX_NAME_LEN - 1);
                        clipboard_has_content = 1;
                        clipboard_is_cut = 0;
                        
                        vga_setcolor(0x0A, 0);
                        vga_print("\nCopied: ");
                        vga_print(files[selected_file].name);
                    }
                    break;
                    
                case 'x': case 'X':
                    if (selected_file > 0) {
                        clipboard = (ClipboardItem){0};
                        strncpy(clipboard.name, files[selected_file].name, MAX_NAME_LEN - 1);
                        clipboard.size = files[selected_file].size;
                        clipboard.mod_time = files[selected_file].modified;
                        clipboard_has_content = 1;
                        clipboard_is_cut = 1;
                        
                        vga_setcolor(0x0E, 0);
                        vga_print("\nCut: ");
                        vga_print(files[selected_file].name);
                    }
                    break;
                    
                case 'v': case 'V':
                    if (clipboard_has_content) {
                        char action[32];
                        strcpy(action, clipboard_is_cut ? "Moved" : "Pasted");
                        
                        vga_setcolor(0x0A, 0);
                        vga_print("\n");
                        vga_print(action);
                        vga_print(": ");
                        vga_print(clipboard.name);
                        vga_print(" -> ");
                        vga_print(current_path);
                        
                        if (clipboard_is_cut) {
                            clipboard_has_content = 0;
                        }
                    }
                    break;
                    
                case 'd': case 'D':
                    if (selected_file > 0) {
                        vga_setcolor(0x0C, 0);
                        vga_print("\nDelete ");
                        vga_print(files[selected_file].name);
                        vga_print("? (y/n): ");
                        
                        char confirm;
                        vga_getc(&confirm);
                        
                        if (confirm == 'y' || confirm == 'Y') {
                            vga_print("\nDeleted: ");
                            vga_print(files[selected_file].name);
                            
                            for (int i = selected_file; i < file_count - 1; i++) {
                                files[i] = files[i + 1];
                            }
                            file_count--;
                            
                            if (selected_file >= file_count) {
                                selected_file = file_count - 1;
                            }
                        }
                    }
                    break;
                    
                case 'r': case 'R':
                    if (selected_file > 0) {
                        vga_setcolor(0x0E, 0);
                        vga_print("\nNew name: ");
                        
                        char new_name[MAX_NAME_LEN];
                        int pos = 0;
                        char c;
                        while ((vga_getc(&c)) == 0 || c == '\0');
                        
                        while (c != '\r' && c != '\n' && pos < MAX_NAME_LEN - 1) {
                            if (c == '\b' && pos > 0) {
                                pos--;
                            } else if (c >= 32 && c < 127) {
                                new_name[pos++] = c;
                                vga_putc(c);
                            }
                            vga_getc(&c);
                        }
                        new_name[pos] = '\0';
                        
                        if (pos > 0) {
                            strncpy(files[selected_file].name, new_name, MAX_NAME_LEN - 1);
                            vga_setcolor(0x0A, 0);
                            vga_print("\nRenamed to: ");
                            vga_print(new_name);
                        }
                    }
                    break;
                    
                case 'n': case 'N':
                    vga_setcolor(0x0E, 0);
                    vga_print("\nCreate new file/directory? (f/d): ");
                    
                    char type_choice;
                    vga_getc(&type_choice);
                    
                    vga_print("\nName: ");
                    char new_item_name[MAX_NAME_LEN];
                    int npos = 0;
                    char nc;
                    while ((vga_getc(&nc)) == 0 || nc == '\0');
                    
                    while (nc != '\r' && nc != '\n' && npos < MAX_NAME_LEN - 1) {
                        if (nc == '\b' && npos > 0) {
                            npos--;
                        } else if (nc >= 32 && nc < 127) {
                            new_item_name[npos++] = nc;
                            vga_putc(nc);
                        }
                        vga_getc(&nc);
                    }
                    new_item_name[npos] = '\0';
                    
                    if (npos > 0 && file_count < MAX_FILES) {
                        strcpy(files[file_count].name, new_item_name);
                        files[file_count].type = (type_choice == 'd') ? FILE_TYPE_DIR : FILE_TYPE_FILE;
                        files[file_count].size = 0;
                        files[file_count].modified = time(NULL);
                        files[file_count].permissions = (type_choice == 'd') ? 0755 : 0644;
                        file_count++;
                        
                        vga_setcolor(0x0A, 0);
                        vga_print("\nCreated: ");
                        vga_print(new_item_name);
                    }
                    break;
                    
                case 'q': case 'Q':
                    running = 0;
                    break;
            }
        }
    }
    
    vga_clear();
    vga_setcolor(0x07, 0);
    vga_print("File Manager closed.\n");
}

int main(int argc, char** argv) {
    if (argc > 1) {
        strncpy(current_path, argv[1], sizeof(current_path) - 1);
    }
    
    vga_print("Starting Kenux File Manager...\n");
    file_manager_run();
    return 0;
}