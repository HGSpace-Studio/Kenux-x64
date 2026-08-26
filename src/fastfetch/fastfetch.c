/*
 * Kenux OS - System Information Display Tool
 * Main fastfetch functionality
 */

#include "fastfetch.h"

#ifndef _WIN32
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#endif

// Default configuration
void fastfetch_init(FastfetchConfig *config) {
    memset(config, 0, sizeof(FastfetchConfig));
    
    // Enable all features by default
    config->show_os = 1;
    config->show_kernel = 1;
    config->show_uptime = 1;
    config->show_packages = 1;
    config->show_shell = 1;
    config->show_terminal = 1;
    config->show_cpu = 1;
    config->show_gpu = 1;
    config->show_memory = 1;
    config->show_disk = 1;
    config->show_host = 1;
    config->show_user = 1;
    config->show_local_ip = 1;
    config->show_public_ip = 1;
    config->show_battery = 1;
    config->show_temp = 1;
    config->show_colors = 1;
    config->logo = 1;
    
    // Set default separator and colors
    strcpy(config->separator, ":");
    strcpy(config->color_os, "\033[0;34m");     // Blue
    strcpy(config->color_kernel, "\033[0;32m"); // Green
    strcpy(config->color_host, "\033[0;33m");   // Yellow
    strcpy(config->color_user, "\033[0;35m");   // Magenta
    strcpy(config->color_separator, "\033[0;36m"); // Cyan
}

void fastfetch_cleanup(void) {
    // Nothing to clean up
}

int parse_arguments(FastfetchConfig *config, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--os") == 0) {
            config->show_os = 1;
        } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--kernel") == 0) {
            config->show_kernel = 1;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--uptime") == 0) {
            config->show_uptime = 1;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--packages") == 0) {
            config->show_packages = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--shell") == 0) {
            config->show_shell = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--terminal") == 0) {
            config->show_terminal = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cpu") == 0) {
            config->show_cpu = 1;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gpu") == 0) {
            config->show_gpu = 1;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) {
            config->show_memory = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disk") == 0) {
            config->show_disk = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            config->show_host = 1;
        } else if (strcmp(argv[i], "--user") == 0) {
            config->show_user = 1;
        } else if (strcmp(argv[i], "--local-ip") == 0) {
            config->show_local_ip = 1;
        } else if (strcmp(argv[i], "--public-ip") == 0) {
            config->show_public_ip = 1;
        } else if (strcmp(argv[i], "--battery") == 0) {
            config->show_battery = 1;
        } else if (strcmp(argv[i], "--temp") == 0) {
            config->show_temp = 1;
        } else if (strcmp(argv[i], "--no-logo") == 0) {
            config->logo = 0;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            config->show_colors = 0;
        } else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--separator") == 0) {
            if (i + 1 < argc) {
                strncpy(config->separator, argv[++i], MAX_LEN - 1);
                config->separator[MAX_LEN - 1] = '\0';
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("kenux-fastfetch Kenux OS System Information Tool\n");
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "kenux-fastfetch: unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    
    return 0;
}

int get_os_info(SystemInfo *info) {
    // Try to get OS information from various sources
    FILE *os_release = fopen("/etc/os-release", "r");
    if (os_release) {
        char line[MAX_LEN];
        while (fgets(line, sizeof(line), os_release)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                strncpy(info->os, line + 12, MAX_LEN - 1);
                // Remove trailing newline
                info->os[strcspn(info->os, "\n")] = '\0';
                // Remove quotes
                if (info->os[0] == '"' && info->os[strlen(info->os) - 1] == '"') {
                    info->os[strlen(info->os) - 1] = '\0';
                    memmove(info->os, info->os + 1, strlen(info->os));
                }
                break;
            }
        }
        fclose(os_release);
    }
    
    if (strlen(info->os) == 0) {
        strcpy(info->os, "Kenux OS");
    }
    
    return 0;
}

int get_kernel_info(SystemInfo *info) {
    struct utsname uts;
    if (uname(&uts) == 0) {
        snprintf(info->kernel, MAX_LEN, "%s %s %s %s %s", 
                uts.sysname, uts.release, uts.version, uts.machine, uts.nodename);
    } else {
        strcpy(info->kernel, "Unknown");
    }
    
    return 0;
}

int get_uptime_info(SystemInfo *info) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->uptime = si.uptime;
    } else {
        info->uptime = 0;
    }
    
    return 0;
}

int get_package_info(SystemInfo *info) {
    // Count packages in common package manager directories
    int count = 0;
    
    // Check for dpkg (Debian/Ubuntu)
    DIR *dpkg_dir = opendir("/var/lib/dpkg/info");
    if (dpkg_dir) {
        struct dirent *entry;
        while ((entry = readdir(dpkg_dir)) != NULL) {
            if (strstr(entry->d_name, ".list")) {
                count++;
            }
        }
        closedir(dpkg_dir);
    }
    
    // Check for rpm (RedHat/CentOS)
    if (count == 0) {
        DIR *rpm_dir = opendir("/var/lib/rpm/Packages");
        if (rpm_dir) {
            struct dirent *entry;
            count = 0;
            while ((entry = readdir(rpm_dir)) != NULL) {
                if (strstr(entry->d_name, ".rpm")) {
                    count++;
                }
            }
            closedir(rpm_dir);
        }
    }
    
    // Check for pacman (Arch)
    if (count == 0) {
        FILE *pacman_db = fopen("/var/lib/pacman/local/ALPM_DB_VERSION", "r");
        if (pacman_db) {
            count = 0;
            char line[MAX_LEN];
            while (fgets(line, sizeof(line), pacman_db)) {
                count++;
            }
            fclose(pacman_db);
        }
    }
    
    // Default count
    if (count == 0) {
        count = 42; // Default value
    }
    
    info->package_count = count;
    return 0;
}

int get_shell_info(SystemInfo *info) {
    const char *shell = getenv("SHELL");
    if (shell) {
        strncpy(info->shell, shell, MAX_LEN - 1);
        info->shell[MAX_LEN - 1] = '\0';
    } else {
        strcpy(info->shell, "/bin/sh");
    }
    
    return 0;
}

int get_terminal_info(SystemInfo *info) {
    const char *term = getenv("TERM");
    if (term) {
        strncpy(info->terminal, term, MAX_LEN - 1);
        info->terminal[MAX_LEN - 1] = '\0';
    } else {
        strcpy(info->terminal, "unknown");
    }
    
    return 0;
}

int get_cpu_info(SystemInfo *info) {
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[MAX_LEN];
        int model_line = 0;
        
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                strncpy(info->cpu_info, line + 13, MAX_CPU_INFO - 1);
                info->cpu_info[strcspn(info->cpu_info, "\n")] = '\0';
                model_line = 1;
                break;
            }
        }
        
        if (!model_line) {
            // Fallback to /proc/cpuinfo without model name
            while (fgets(line, sizeof(line), cpuinfo)) {
                if (strlen(line) > 1) {
                    strncpy(info->cpu_info, line, MAX_CPU_INFO - 1);
                    info->cpu_info[strcspn(info->cpu_info, "\n")] = '\0';
                    break;
                }
            }
        }
        
        fclose(cpuinfo);
    } else {
        strcpy(info->cpu_info, "Unknown");
    }
    
    return 0;
}

int get_gpu_info(SystemInfo *info) {
    // Try to get GPU information from /proc/bus/pci or other sources
    FILE * lspci = popen("lspci | grep -i 'vga'", "r");
    if (lspci) {
        char line[MAX_CPU_INFO];
        if (fgets(line, sizeof(line), lspci)) {
            strncpy(info->gpu_info, line, MAX_CPU_INFO - 1);
            info->gpu_info[strcspn(info->gpu_info, "\n")] = '\0';
        } else {
            strcpy(info->gpu_info, "Unknown");
        }
        pclose(lspci);
    } else {
        strcpy(info->gpu_info, "Unknown");
    }
    
    return 0;
}

int get_memory_info(SystemInfo *info) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        unsigned long total = si.totalram * si.mem_unit / (1024 * 1024);
        unsigned long free = si.freeram * si.mem_unit / (1024 * 1024);
        unsigned long used = total - free;
        
        snprintf(info->memory_info, MAX_MEM_INFO, 
                "%llu MB / %llu MB (%.1f%%)", used, total, 
                (double)used / total * 100);
    } else {
        strcpy(info->memory_info, "Unknown");
    }
    
    return 0;
}

int get_disk_info(SystemInfo *info) {
    FILE *df = popen("df -h / | tail -1", "r");
    if (df) {
        char line[MAX_DISK_INFO];
        if (fgets(line, sizeof(line), df)) {
            char filesystem[MAX_LEN], size[MAX_LEN], used[MAX_LEN], 
                 avail[MAX_LEN], use_pct[MAX_LEN], mount[MAX_LEN];
            
            int items = sscanf(line, "%s %s %s %s %s %s", 
                               filesystem, size, used, avail, use_pct, mount);
            
            if (items >= 4) {
                snprintf(info->disk_info, MAX_DISK_INFO, 
                        "%s (%s used)", size, used);
            } else {
                strcpy(info->disk_info, "Unknown");
            }
        } else {
            strcpy(info->disk_info, "Unknown");
        }
        pclose(df);
    } else {
        strcpy(info->disk_info, "Unknown");
    }
    
    return 0;
}

int get_host_info(SystemInfo *info) {
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info->hostname, uts.nodename, MAX_LEN - 1);
        info->hostname[MAX_LEN - 1] = '\0';
    } else {
        strcpy(info->hostname, "unknown");
    }
    
    return 0;
}

int get_user_info(SystemInfo *info) {
    const char *user = getenv("USER");
    if (!user) user = getenv("USERNAME");
    if (user) {
        strncpy(info->username, user, MAX_LEN - 1);
        info->username[MAX_LEN - 1] = '\0';
    } else {
#ifndef _WIN32
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            strncpy(info->username, pw->pw_name, MAX_LEN - 1);
            info->username[MAX_LEN - 1] = '\0';
        } else {
            strcpy(info->username, "unknown");
        }
#else
        strcpy(info->username, "unknown");
#endif
    }

    return 0;
}

int get_local_ip_info(SystemInfo *info) {
#ifndef _WIN32
    struct ifaddrs *ifaddrs_ptr;
    if (getifaddrs(&ifaddrs_ptr) == 0) {
        struct ifaddrs *ifa = ifaddrs_ptr;

        while (ifa) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                if (strcmp(ifa->ifa_name, "lo") != 0) {
                    struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
                    inet_ntop(AF_INET, &(addr_in->sin_addr), info->local_ip, MAX_LEN);
                    break;
                }
            }
            ifa = ifa->ifa_next;
        }

        freeifaddrs(ifaddrs_ptr);
    }
#else
    /* Windows: no getifaddrs; leave local_ip as "Unknown". */
#endif

    if (strlen(info->local_ip) == 0) {
        strcpy(info->local_ip, "Unknown");
    }

    return 0;
}

int get_public_ip_info(SystemInfo *info) {
    FILE *curl = popen("curl -s ifconfig.me", "r");
    if (curl) {
        char ip[MAX_LEN];
        if (fgets(ip, sizeof(ip), curl)) {
            ip[strcspn(ip, "\n")] = '\0';
            strncpy(info->public_ip, ip, MAX_LEN - 1);
            info->public_ip[MAX_LEN - 1] = '\0';
        } else {
            strcpy(info->public_ip, "Unknown");
        }
        pclose(curl);
    } else {
        strcpy(info->public_ip, "Unknown");
    }
    
    return 0;
}

int get_battery_info(SystemInfo *info) {
    // Check for battery information in /sys/class/power_supply
    FILE *battery = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (battery) {
        char status[MAX_LEN];
        if (fgets(status, sizeof(status), battery)) {
            status[strcspn(status, "\n")] = '\0';
            strcpy(info->battery, status);
        } else {
            strcpy(info->battery, "Unknown");
        }
        fclose(battery);
    } else {
        strcpy(info->battery, "AC Power");
    }
    
    return 0;
}

int get_temp_info(SystemInfo *info) {
    // Try to get CPU temperature from /sys/class/thermal
    FILE *temp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp) {
        int temp_c;
        if (fscanf(temp, "%d", &temp_c) == 1) {
            double temp_celsius = temp_c / 1000.0;
            snprintf(info->temp, MAX_LEN, "%.1f°C", temp_celsius);
        } else {
            strcpy(info->temp, "Unknown");
        }
        fclose(temp);
    } else {
        strcpy(info->temp, "Unknown");
    }
    
    return 0;
}

int collect_system_info(SystemInfo *info) {
    // Initialize info
    memset(info, 0, sizeof(SystemInfo));
    
    // Collect all information
    get_os_info(info);
    get_kernel_info(info);
    get_uptime_info(info);
    get_package_info(info);
    get_shell_info(info);
    get_terminal_info(info);
    get_cpu_info(info);
    get_gpu_info(info);
    get_memory_info(info);
    get_disk_info(info);
    get_host_info(info);
    get_user_info(info);
    get_local_ip_info(info);
    get_public_ip_info(info);
    get_battery_info(info);
    get_temp_info(info);
    
    return 0;
}

void print_info_line(const char *label, const char *value, const FastfetchConfig *config) {
    if (config->show_colors) {
        printf("%s%s%s %s%s%s %s%s%s\n", 
               config->color_os, label, config->color_separator,
               config->color_kernel, value, config->color_separator,
               config->color_separator, config->separator, "\033[0m");
    } else {
        printf("%s%s%s\n", label, config->separator, value);
    }
}

void display_system_info(const FastfetchConfig *config, const SystemInfo *info) {
    // Display logo if enabled
    if (config->logo) {
        printf("\n");
        printf("      .--.  .--. .-.\n");
        printf("     : .--': .--': : \n");
        printf("     : :   : :   : : \n");
        printf("     : :   : :   : :\n");
        printf("     : :   : :   : :\n");
        printf("     : '--. : '--. : :\n");
        printf("     `--'  `--'  `--'\n\n");
    }
    
    // Display system information based on configuration
    if (config->show_os) {
        print_info_line("OS", info->os, config);
    }
    
    if (config->show_kernel) {
        print_info_line("Kernel", info->kernel, config);
    }
    
    if (config->show_uptime) {
        int days = info->uptime / 86400;
        int hours = (info->uptime % 86400) / 3600;
        int minutes = (info->uptime % 3600) / 60;
        char uptime_str[MAX_LEN];
        
        if (days > 0) {
            snprintf(uptime_str, MAX_LEN, "%d days, %d hours, %d minutes", days, hours, minutes);
        } else if (hours > 0) {
            snprintf(uptime_str, MAX_LEN, "%d hours, %d minutes", hours, minutes);
        } else {
            snprintf(uptime_str, MAX_LEN, "%d minutes", minutes);
        }
        
        print_info_line("Uptime", uptime_str, config);
    }
    
    if (config->show_packages) {
        char packages_str[MAX_LEN];
        snprintf(packages_str, MAX_LEN, "%d", info->package_count);
        print_info_line("Packages", packages_str, config);
    }
    
    if (config->show_shell) {
        print_info_line("Shell", info->shell, config);
    }
    
    if (config->show_terminal) {
        print_info_line("Terminal", info->terminal, config);
    }
    
    if (config->show_cpu) {
        print_info_line("CPU", info->cpu_info, config);
    }
    
    if (config->show_gpu) {
        print_info_line("GPU", info->gpu_info, config);
    }
    
    if (config->show_memory) {
        print_info_line("Memory", info->memory_info, config);
    }
    
    if (config->show_disk) {
        print_info_line("Disk", info->disk_info, config);
    }
    
    if (config->show_host) {
        print_info_line("Host", info->hostname, config);
    }
    
    if (config->show_user) {
        print_info_line("User", info->username, config);
    }
    
    if (config->show_local_ip) {
        print_info_line("Local IP", info->local_ip, config);
    }
    
    if (config->show_public_ip) {
        print_info_line("Public IP", info->public_ip, config);
    }
    
    if (config->show_battery) {
        print_info_line("Battery", info->battery, config);
    }
    
    if (config->show_temp) {
        print_info_line("CPU Temp", info->temp, config);
    }
    
    printf("\n");
}

void print_usage(void) {
    printf("Kenux OS System Information Display Tool\n");
    printf("Usage: fastfetch [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -o, --os          Show OS information\n");
    printf("  -k, --kernel      Show kernel information\n");
    printf("  -u, --uptime      Show system uptime\n");
    printf("  -p, --packages    Show package count\n");
    printf("  -s, --shell       Show shell information\n");
    printf("  -t, --terminal    Show terminal information\n");
    printf("  -c, --cpu         Show CPU information\n");
    printf("  -g, --gpu         Show GPU information\n");
    printf("  -m, --memory      Show memory information\n");
    printf("  -d, --disk        Show disk information\n");
    printf("  -h, --host        Show hostname\n");
    printf("  --user            Show current user\n");
    printf("  --local-ip        Show local IP address\n");
    printf("  --public-ip       Show public IP address\n");
    printf("  --battery         Show battery status\n");
    printf("  --temp            Show CPU temperature\n");
    printf("  --no-logo         Disable ASCII logo\n");
    printf("  --no-color        Disable colored output\n");
    printf("  -S, --separator   Set custom separator\n");
    printf("  -v, --version     Show version information\n");
    printf("  --help            Show this help message\n\n");
    printf("By default, all features are enabled.\n");
}

int main(int argc, char **argv) {
    FastfetchConfig config;
    SystemInfo info;
    
    fastfetch_init(&config);
    
    // Parse arguments
    if (parse_arguments(&config, argc, argv) != 0) {
        return EXIT_FAILURE;
    }
    
    // Collect system information
    if (collect_system_info(&info) != 0) {
        return EXIT_FAILURE;
    }
    
    // Display system information
    display_system_info(&config, &info);
    
    return EXIT_SUCCESS;
}