#include <stdio.h>
#include <string.h>
#include <vga.h>
#include <syscall.h>
#include <process.h>

void cmd_sysinfo(int argc, char** argv)
{
    vga_print("=== Kenux OS System Information ===\n\n");
    vga_print("System:    Kenux Operating System\n");
    vga_print("Version:   1.0.0\n");
    vga_print("Arch:      x86_64\n");
    vga_print("CPU:       x86_64 Compatible\n");
    vga_print("Memory:    64MB (simulated)\n");
    vga_print("Uptime:    00:00:00\n");
    vga_print("Kernel:    1.0.0\n");
}

void cmd_meminfo(int argc, char** argv)
{
    vga_print("=== Memory Information ===\n\n");
    vga_print("Total:     64MB\n");
    vga_print("Used:      12MB\n");
    vga_print("Free:      52MB\n");
    vga_print("Buffers:   2MB\n");
    vga_print("Cached:    4MB\n");
}

void cmd_diskinfo(int argc, char** argv)
{
    vga_print("=== Disk Information ===\n\n");
    vga_print("Device:    /dev/sda\n");
    vga_print("Type:      SATA\n");
    vga_print("Size:      1GB\n");
    vga_print("Used:      200MB\n");
    vga_print("Free:      800MB\n");
    vga_print("Filesystem: ext2\n");
}

void cmd_netinfo(int argc, char** argv)
{
    vga_print("=== Network Information ===\n\n");
    vga_print("Interface: eth0\n");
    vga_print("MAC:       00:11:22:33:44:55\n");
    vga_print("IP:        192.168.1.100\n");
    vga_print("Netmask:   255.255.255.0\n");
    vga_print("Gateway:   192.168.1.1\n");
    vga_print("DNS:       8.8.8.8\n");
}

void cmd_ps(int argc, char** argv)
{
    vga_print("=== Process List ===\n\n");
    vga_print("  PID    COMMAND\n");
    vga_print("  ----   -------\n");
    vga_print("  1      init\n");
    vga_print("  2      idle\n");
}

void cmd_kill(int argc, char** argv)
{
    if (argc < 2) {
        vga_print("Usage: kill <pid>\n");
        return;
    }
    
    int pid = atoi(argv[1]);
    process_kill(pid);
    vga_print("Process ");
    vga_print(argv[1]);
    vga_print(" killed\n");
}

void cmd_top(int argc, char** argv)
{
    vga_print("=== System Monitor ===\n\n");
    vga_print("CPU Usage:   5%\n");
    vga_print("Memory:      12MB / 64MB (18%)\n");
    vga_print("Disk I/O:    0 KB/s\n");
    vga_print("Network:     0 KB/s\n");
    vga_print("\nProcesses: 2 total, 1 running, 1 sleeping\n");
}

void cmd_log(int argc, char** argv)
{
    vga_print("=== System Log ===\n\n");
    vga_print("[INFO] System boot completed\n");
    vga_print("[INFO] Filesystem mounted\n");
    vga_print("[INFO] Network interface initialized\n");
    vga_print("[INFO] Shell started\n");
}

void cmd_date(int argc, char** argv)
{
    vga_print("Sat Mar  8 12:00:00 2026\n");
}

void cmd_calculator(int argc, char** argv)
{
    vga_print("Simple Calculator\n");
    vga_print("Usage: calculator <expression>\n");
    vga_print("Example: calculator 2+3\n");
    vga_print("Supported: + - * /\n");
}

void cmd_help(int argc, char** argv)
{
    vga_print("Available commands:\n");
    vga_print("  sysinfo    - Show system information\n");
    vga_print("  meminfo    - Show memory information\n");
    vga_print("  diskinfo   - Show disk information\n");
    vga_print("  netinfo    - Show network information\n");
    vga_print("  ps         - List processes\n");
    vga_print("  kill       - Kill a process\n");
    vga_print("  top        - System monitor\n");
    vga_print("  log        - Show system log\n");
    vga_print("  date       - Show date and time\n");
    vga_print("  calculator - Simple calculator\n");
    vga_print("  help       - Show this help\n");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        cmd_help(0, NULL);
        return 0;
    }
    
    if (strcmp(argv[1], "sysinfo") == 0) {
        cmd_sysinfo(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "meminfo") == 0) {
        cmd_meminfo(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "diskinfo") == 0) {
        cmd_diskinfo(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "netinfo") == 0) {
        cmd_netinfo(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "ps") == 0) {
        cmd_ps(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "kill") == 0) {
        cmd_kill(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "top") == 0) {
        cmd_top(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "log") == 0) {
        cmd_log(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "date") == 0) {
        cmd_date(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "calculator") == 0) {
        cmd_calculator(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        cmd_help(argc - 1, argv + 1);
    } else {
        vga_print("Unknown command: ");
        vga_print(argv[1]);
        vga_print("\n");
        cmd_help(0, NULL);
    }
    
    return 0;
}
