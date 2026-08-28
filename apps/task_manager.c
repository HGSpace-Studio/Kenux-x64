#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define MAX_PROCESSES 64
#define MAX_NAME_LEN 64

typedef struct {
    int pid;
    char name[MAX_NAME_LEN];
    int cpu_usage;
    int memory_kb;
    char state;
    int priority;
    time_t start_time;
} ProcessInfo;

static ProcessInfo processes[MAX_PROCESSES];
static int process_count = 0;
static int selected_index = 0;
static int sort_by_cpu = 1;

static void generate_sample_processes(void) {
    const char* process_names[] = {
        "kernel", "init", "kenuxwm", "ksh", "terminal",
        "filemgr", "browser", "calculator", "snake_game",
        "tetris", "minesweeper", "fastfetch", "networkd",
        "sound-server", "display-server", "usb-driver",
        "storage-daemon", "security-service", "power-manager",
        "package-manager"
    };
    
    process_count = 20;
    
    for (int i = 0; i < process_count && i < MAX_PROCESSES; i++) {
        processes[i].pid = 1000 + i * 10 + rand() % 5;
        strncpy(processes[i].name, process_names[i], MAX_NAME_LEN - 1);
        processes[i].cpu_usage = rand() % 15;
        if (i == 0) processes[i].cpu_usage += 2;
        processes[i].memory_kb = (rand() % 50000) + 512;
        
        static const char states[] = {'R', 'S', 'D', 'Z'};
        processes[i].state = states[rand() % 4];
        
        processes[i].priority = (rand() % 20) - 10;
        processes[i].start_time = time(NULL) - (rand() % 36000);
    }
}

static void sort_processes(void) {
    for (int i = 0; i < process_count - 1; i++) {
        for (int j = 0; j < process_count - i - 1; j++) {
            int should_swap = 0;
            
            if (sort_by_cpu) {
                should_swap = (processes[j].cpu_usage < processes[j+1].cpu_usage);
            } else {
                should_swap = (processes[j].memory_kb < processes[j+1].memory_kb);
            }
            
            if (should_swap) {
                ProcessInfo temp = processes[j];
                processes[j] = processes[j+1];
                processes[j+1] = temp;
            }
        }
    }
}

static void draw_header(void) {
    vga_clear();
    
    vga_setcolor(0x0E, 0);
    vga_print("=== Kenux Task Manager ===\n\n");
    
    vga_setcolor(0x09, 0);
    printf(" %-6s %-18s %-8s %-12s %-7s %-8s\n",
           "PID", "NAME", "CPU%", "MEMORY(KB)", "STATE", "PRIORITY");
    
    vga_setcolor(0x07, 0);
    vga_print("------ ------------------ -------- ------------ ------- --------\n");
}

static void draw_process_list(void) {
    int visible_start = 0;
    int visible_count = 15;
    
    if (selected_index >= visible_start + visible_count) {
        visible_start = selected_index - visible_count + 1;
    }
    
    int end = visible_start + visible_count;
    if (end > process_count) end = process_count;
    
    for (int i = visible_start; i < end; i++) {
        if (i == selected_index) {
            vga_setcolor(0x70, 0);
        } else {
            vga_setcolor(0x07, 0);
        }
        
        char uptime_str[32];
        time_t now = time(NULL);
        double elapsed = difftime(now, processes[i].start_time);
        int hours = (int)(elapsed / 3600);
        int mins = ((int)(elapsed) % 3600) / 60;
        
        if (hours > 0) {
            sprintf(uptime_str, "%dh%dm", hours, mins);
        } else {
            sprintf(uptime_str, "%dm", mins);
        }
        
        printf(" %-6d %-18s %-7d %-11d %-7c %+4d\n",
               processes[i].pid,
               processes[i].name,
               processes[i].cpu_usage,
               processes[i].memory_kb,
               processes[i].state,
               processes[i].priority);
    }
    
    vga_setcolor(0x07, 0);
    vga_print("\n");
}

static void draw_summary(void) {
    int total_cpu = 0;
    int total_memory = 0;
    int running = 0, sleeping = 0;
    
    for (int i = 0; i < process_count; i++) {
        total_cpu += processes[i].cpu_usage;
        total_memory += processes[i].memory_kb;
        
        if (processes[i].state == 'R') running++;
        else if (processes[i].state == 'S') sleeping++;
    }
    
    vga_setcolor(0x0B, 0);
    char summary[256];
    sprintf(summary,
        "Summary: Processes: %d | Running: %d | Sleeping: %d\n"
        "         Total CPU: %d%% | Total Memory: %.2f MB\n\n",
        process_count, running, sleeping,
        total_cpu, total_memory / 1024.0);
    vga_print(summary);
    
    vga_setcolor(0x0A, 0);
    vga_print("Controls:\n");
    vga_print("Up/Down: Select Process | C: Sort by CPU | M: Sort by Memory\n");
    vga_print("K: Kill Process | R: Refresh | Q: Quit\n");
}

static void draw_details(int index) {
    if (index < 0 || index >= process_count) return;
    
    ProcessInfo* p = &processes[index];
    
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Process Details ===\n\n");
    
    vga_setcolor(0x0F, 0);
    char detail[128];
    
    sprintf(detail, "PID:              %d\n", p->pid); vga_print(detail);
    sprintf(detail, "Name:             %s\n", p->name); vga_print(detail);
    sprintf(detail, "State:            %c\n", p->state); vga_print(detail);
    sprintf(detail, "CPU Usage:        %d%%\n", p->cpu_usage); vga_print(detail);
    sprintf(detail, "Memory Usage:     %d KB (%.2f MB)\n", 
            p->memory_kb, p->memory_kb / 1024.0); vga_print(detail);
    sprintf(detail, "Priority:         %d\n", p->priority); vga_print(detail);
    
    time_t now = time(NULL);
    double elapsed = difftime(now, p->start_time);
    int days = (int)(elapsed / 86400);
    int hours = ((int)(elapsed) % 86400) / 3600;
    int mins = ((int)(elapsed) % 3600) / 60;
    
    sprintf(detail, "Running Time:     ", p->pid); vga_print(detail);
    if (days > 0) {
        sprintf(detail, "%dd %dh %dm\n", days, hours, mins);
    } else if (hours > 0) {
        sprintf(detail, "%dh %dm\n", hours, mins);
    } else {
        sprintf(detail, "%dm\n", mins);
    }
    vga_print(detail);
}

void task_manager_run(void) {
    srand(time(NULL));
    generate_sample_processes();
    sort_processes();
    
    int show_details = 0;
    int running = 1;
    
    while (running) {
        draw_header();
        draw_process_list();
        draw_summary();
        
        if (show_details) {
            draw_details(selected_index);
        }
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            switch (key) {
                case 72: case 'w': case 'W':
                    if (selected_index > 0) selected_index--;
                    break;
                    
                case 80: case 's': case 'S':
                    if (selected_index < process_count - 1) selected_index++;
                    break;
                    
                case 'c': case 'C':
                    sort_by_cpu = 1;
                    sort_processes();
                    break;
                    
                case 'm': case 'M':
                    sort_by_cpu = 0;
                    sort_processes();
                    break;
                    
                case 'd': case 'D':
                    show_details = !show_details;
                    break;
                    
                case 'k': case 'K':
                    if (selected_index >= 0 && selected_index < process_count) {
                        vga_setcolor(0x0C, 0);
                        char msg[128];
                        sprintf(msg, "\nKilling process %d (%s)... [SIMULATED]\n",
                                processes[selected_index].pid,
                                processes[selected_index].name);
                        vga_print(msg);
                        
                        for (int i = selected_index; i < process_count - 1; i++) {
                            processes[i] = processes[i + 1];
                        }
                        process_count--;
                        
                        if (selected_index >= process_count) {
                            selected_index = process_count - 1;
                        }
                    }
                    break;
                    
                case 'r': case 'R':
                    generate_sample_processes();
                    sort_processes();
                    vga_setcolor(0x0A, 0);
                    vga_print("\n[Refreshed]\n");
                    break;
                    
                case 'q': case 'Q':
                    running = 0;
                    break;
            }
        }
    }
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Task Manager...\n");
    task_manager_run();
    return 0;
}