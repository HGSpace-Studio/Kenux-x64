#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define LOGO_WIDTH 40

static const char* kenux_logo[] = {
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■",
    "■■■■■■■■■   ■■■■■■■■■■"
};

static void get_os_info(char* buf, size_t size) {
    snprintf(buf, size, "Kenux OS 26.8.28 (Stardust)");
}

static void get_kernel_info(char* buf, size_t size) {
    snprintf(buf, size, "Kenux Kernel KNE2.7");
}

static void get_hostname(char* buf, size_t size) {
    snprintf(buf, size, "kenux-workstation");
}

static void get_uptime(char* buf, size_t size) {
    static time_t boot_time = 0;
    if (boot_time == 0) {
        boot_time = time(NULL) - (3 * 24 * 3600 + 12 * 3600 + 45 * 60);
    }
    
    time_t now = time(NULL);
    double uptime = difftime(now, boot_time);
    
    int days = (int)(uptime / (24 * 3600));
    uptime -= days * 24 * 3600;
    int hours = (int)(uptime / 3600);
    uptime -= hours * 3600;
    int mins = (int)(uptime / 60);
    
    if (days > 0) {
        snprintf(buf, size, "%d days, %d hours, %d mins", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, size, "%d hours, %d mins", hours, mins);
    } else {
        snprintf(buf, size, "%d mins", mins);
    }
}

static void get_shell_info(char* buf, size_t size) {
    snprintf(buf, size, "/bin/ksh (Kenux Shell)");
}

static void get_resolution(char* buf, size_t size) {
    snprintf(buf, size, "1920x1080");
}

static void get_de_info(char* buf, size_t size) {
    snprintf(buf, size, "StardustUI 1.4.2");
}

static void get_wm_info(char* buf, size_t size) {
    snprintf(buf, size, "KenuxWM (KWindow Manager)");
}

static void get_theme_info(char* buf, size_t size) {
    snprintf(buf, size, "Kenux-Dark [GTK2/3]");
}

static void get_cpu_info(char* buf, size_t size) {
    snprintf(buf, size, "Kenux CPU v3.1 @ 3.6GHz (8 cores)");
}

static void get_gpu_info(char* buf, size_t size) {
    snprintf(buf, size, "Kenux Graphics G5000 (512MB VRAM)");
}

static void get_memory_info(char* buf, size_t size) {
    snprintf(buf, size, "8192 MiB / 16384 MiB (50%%)");
}

static void get_disk_info(char* buf, size_t size) {
    snprintf(buf, size, "256 GiB / 512 GiB (50%%)");
}

static void get_packages_count(char* buf, size_t size) {
    snprintf(buf, size, "142 (kpkg)");
}

static void get_local_ip(char* buf, size_t size) {
    snprintf(buf, size, "192.168.1.100");
}

static void get_public_ip(char* buf, size_t size) {
    snprintf(buf, size, "203.0.113.42");
}

static void get_battery_info(char* buf, size_t size) {
    snprintf(buf, size, "87%%, 3:42 remaining (AC Power)");
}

static void get_temp_info(char* buf, size_t size) {
    snprintf(buf, size, "42.5°C (CPU), 38.2°C (GPU)");
}

static void draw_logo(void) {
    vga_setcolor(0x0B, 0);
    
    for (int i = 0; i < sizeof(kenux_logo) / sizeof(kenux_logo[0]); i++) {
        vga_print("  ");
        vga_print(kenux_logo[i]);
        vga_print("\n");
    }
    vga_print("\n");
}

typedef struct {
    const char* label;
    void (*get_value)(char*, size_t);
} InfoItem;

static InfoItem info_items[] = {
    {"OS",           get_os_info},
    {"Kernel",       get_kernel_info},
    {"Hostname",     get_hostname},
    {"Uptime",       get_uptime},
    {"Shell",        get_shell_info},
    {"Resolution",   get_resolution},
    {"DE",           get_de_info},
    {"WM",           get_wm_info},
    {"Theme",        get_theme_info},
    {"CPU",          get_cpu_info},
    {"GPU",          get_gpu_info},
    {"Memory",       get_memory_info},
    {"Disk (/> )",   get_disk_info},
    {"Packages",     get_packages_count},
    {"Local IP",     get_local_ip},
    {"Public IP",    get_public_ip},
    {"Battery",      get_battery_info},
    {"Temperature",  get_temp_info},
    {NULL, NULL}
};

void fastfetch_run(void) {
    vga_clear();
    
    draw_logo();
    
    for (int i = 0; info_items[i].label; i++) {
        char value[128];
        info_items[i].get_value(value, sizeof(value));
        
        vga_setcolor(0x09, 0);
        vga_print(info_items[i].label);
        
        int padding = 14 - strlen(info_items[i].label);
        for (int p = 0; p < padding; p++) {
            vga_print(" ");
        }
        
        vga_setcolor(0x0F, 0);
        vga_print(value);
        vga_print("\n");
    }
    
    vga_setcolor(0x07, 0);
    vga_print("\nPress any key to exit...\n");
    
    char dummy;
    vga_getc(&dummy);
}

int main(int argc, char** argv) {
    int show_logo = 1;
    int format_ascii = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-logo") == 0) {
            show_logo = 0;
        } else if (strcmp(argv[i], "--ascii") == 0 || strcmp(argv[i], "-a") == 0) {
            format_ascii = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("fastfetch - Display system information\n\n");
            printf("Usage: fastfetch [options]\n\n");
            printf("Options:\n");
            printf("  --no-logo    Don't display the logo\n");
            printf("  --ascii, -a  Use ASCII art logo\n");
            printf("  --help, -h   Show this help\n");
            return 0;
        }
    }
    
    if (!show_logo) {
        vga_clear();
        for (int i = 0; info_items[i].label; i++) {
            char value[128];
            info_items[i].get_value(value, sizeof(value));
            
            printf("%-14s%s\n", info_items[i].label, value);
        }
        return 0;
    }
    
    fastfetch_run();
    return 0;
}