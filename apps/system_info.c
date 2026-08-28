#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

static void show_cpu_info(void) {
    vga_setcolor(0x0E, 0);
    vga_print("=== CPU Information ===\n\n");
    
    vga_setcolor(0x0F, 0);
    vga_print("Processor:     Kenux CPU v3.1 (x86_64)\n");
    vga_print("Cores:         8 cores / 16 threads\n");
    vga_print("Base Clock:    3600 MHz\n");
    vga_print("Max Boost:     5200 MHz\n");
    vga_print("Cache L1d:     256 KB (per core)\n");
    vga_print("Cache L1i:     256 KB (per core)\n");
    vga_print("Cache L2:      2 MB (per core)\n");
    vga_print("Cache L3:      24 MB (shared)\n");
    vga_print("Architecture:  x86-64 (Kenux Extended)\n");
    vga_print("Virtualization: KVM enabled\n");
    vga_print("Features:      MMX SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 AVX AVX2 FMA3\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nCurrent Usage:\n");
    vga_setcolor(0x07, 0);
    vga_print("Core 0:  23% | Core 1:  18% | Core 2:  31% | Core 3:  12%\n");
    vga_print("Core 4:  15% | Core 5:   8% | Core 6:  25% | Core 7:  19%\n");
    vga_print("Average: 18.9% | Temperature: 42.5°C\n");
}

static void show_memory_info(void) {
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Memory Information ===\n\n");
    
    vga_setcolor(0x0F, 0);
    vga_print("Total Memory:      16384 MiB (16 GiB)\n");
    vga_print("Used Memory:       8192 MiB (50%%)\n");
    vga_print("Free Memory:       7680 MiB (47%%)\n");
    vga_print("Buffer/Cache:      512 MiB (3%%)\n");
    vga_print("Swap Total:        8192 MiB (8 GiB)\n");
    vga_print("Swap Used:         1024 MiB (12%%)\n");
    vga_print("Swap Free:         7168 MiB (88%%)\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nMemory Breakdown:\n");
    vga_setcolor(0x07, 0);
    vga_print("Kernel:           2048 MiB\n");
    vga_print("Userspace:        4096 MiB\n");
    vga_print("File Cache:       1536 MiB\n");
    vga_print("Slab Allocator:   512 MiB\n");
    vga_print("Page Tables:      128 MiB\n");
    vga_print("Other:            -32 MiB (overhead)\n");
}

static void show_disk_info(void) {
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Disk Information ===\n\n");
    
    vga_setcolor(0x0F, 0);
    vga_print("Device:           /dev/sda (512 GB SSD)\n");
    vga_print("Model:            Kenux NVMe Pro 500\n");
    vga_print("Interface:        NVMe 1.4 (PCIe 4.0 x4)\n");
    vga_print("Partition Table:  GPT\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nPartitions:\n");
    vga_setcolor(0x07, 0);
    vga_print("/dev/sda1  (EFI)      512 MiB    FAT32    Boot\n");
    vga_print("/dev/sda2  (/)        128 GiB    ext4     Root\n");
    vga_print("/dev/sda3  (/home)    256 GiB    ext4     Home\n");
    vga_print("/dev/sda4  (swap)     8 GiB      swap     Swap\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nUsage:\n");
    vga_setcolor(0x07, 0);
    vga_print("/          64 GiB used / 128 GiB total (50%%)\n");
    vga_print("/home      192 GiB used / 256 GiB total (75%%)\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nI/O Statistics:\n");
    vga_setcolor(0x07, 0);
    vga_print("Read Rate:   245 MB/s\n");
    vga_print("Write Rate:  189 MB/s\n");
    vga_print("Total Read:  2.3 TB\n");
    vga_print("Total Write: 1.8 TB\n");
}

static void show_gpu_info(void) {
    vga_setcolor(0x0E, 0);
    vga_print("\n=== GPU Information ===\n\n");
    
    vga_setcolor(0x0F, 0);
    vga_print("GPU:              Kenux Graphics G5000\n");
    vga_print("VRAM:             512 MB GDDR6\n");
    vga_print("Driver:           kenux-gpu 2.1.0\n");
    vga_print("API Support:      Vulkan 1.3, OpenGL ES 3.2, DirectX 12\n");
    vga_print("Resolution:       1920x1080 @ 144Hz\n");
    vga_print("Connector:        HDMI + DisplayPort\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nCurrent State:\n");
    vga_setcolor(0x07, 0);
    vga_print("Temperature:      38.2°C\n");
    vga_print("Clock Speed:      1200 MHz (Base: 900 MHz)\n");
    vga_print("VRAM Usage:       186 MB / 512 MB (36%%)\n");
    vga_print("Power Draw:       28W (TDP: 75W)\n");
}

static void show_network_info(void) {
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Network Information ===\n\n");
    
    vga_setcolor(0x09, 0);
    vga_print("Ethernet (eth0):\n");
    vga_setcolor(0x0F, 0);
    vga_print("Status:           UP\n");
    vga_print("Link Speed:       1000 Mbps Full-Duplex\n");
    vga_print("MAC Address:      AA:BB:CC:DD:EE:FF\n");
    vga_print("IPv4 Address:     192.168.1.100/24\n");
    vga_print("IPv6 Address:     fe80::aabb:ccdd:eeff:00/64\n");
    vga_print("Gateway:          192.168.1.1\n");
    vga_print("DNS Servers:      192.168.1.1, 8.8.8.8, 1.1.1.1\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nWiFi (wlan0):\n");
    vga_setcolor(0x0F, 0);
    vga_print("Status:           Connected\n");
    vga_print("SSID:             KenuxNetwork-5G\n");
    vga_print("Signal Strength:  78%% (-52 dBm)\n");
    vga_print("Frequency:        5180 MHz (Channel 36)\n");
    vga_print("Security:         WPA3-Personal\n");
    vga_print("IPv4 Address:     192.168.1.101/24 (DHCP)\n");
    
    vga_setcolor(0x09, 0);
    vga_print("\nNetwork Activity:\n");
    vga_setcolor(0x07, 0);
    vga_print("Received:         1.2 GB\n");
    vga_print("Transmitted:      456 MB\n");
    vga_print("Packets In:       892341\n");
    vga_print("Packets Out:      567123\n");
}

static void show_system_summary(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    vga_clear();
    vga_setcolor(0x0E, 0);
    vga_print("╔══════════════════════════════════════╗\n");
    vga_print("║     Kenux System Information v2.7    ║\n");
    vga_print("╚══════════════════════════════════════╝\n\n");
    
    vga_setcolor(0x0F, 0);
    vga_print("System Time:      ");
    vga_print(time_str);
    vga_print("\n\n");
    
    show_cpu_info();
    show_memory_info();
    show_disk_info();
    show_gpu_info();
    show_network_info();
    
    vga_setcolor(0x0A, 0);
    vga_print("\n========================================\n");
    vga_print("Press any key to exit...\n");
}

void system_info_run(int argc, char** argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "--cpu") == 0 || strcmp(argv[1], "-c") == 0) {
            vga_clear();
            show_cpu_info();
        } else if (strcmp(argv[1], "--memory") == 0 || strcmp(argv[1], "-m") == 0) {
            vga_clear();
            show_memory_info();
        } else if (strcmp(argv[1], "--disk") == 0 || strcmp(argv[1], "-d") == 0) {
            vga_clear();
            show_disk_info();
        } else if (strcmp(argv[1], "--gpu") == 0 || strcmp(argv[1], "-g") == 0) {
            vga_clear();
            show_gpu_info();
        } else if (strcmp(argv[1], "--network") == 0 || strcmp(argv[1], "-n") == 0) {
            vga_clear();
            show_network_info();
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("sysinfo - Display detailed system information\n\n");
            printf("Usage: sysinfo [option]\n\n");
            printf("Options:\n");
            printf("  --cpu, -c      Show CPU information only\n");
            printf("  --memory, -m   Show memory information only\n");
            printf("  --disk, -d     Show disk information only\n");
            printf("  --gpu, -g      Show GPU information only\n");
            printf("  --network, -n  Show network information only\n");
            printf("  --help, -h     Show this help message\n");
            printf("\nWithout options, shows complete system summary.\n");
            return;
        } else {
            vga_setcolor(0x0C, 0);
            vga_print("Unknown option: ");
            vga_print(argv[1]);
            vga_print("\nUse --help for usage information.\n");
            return;
        }
        
        vga_setcolor(0x0A, 0);
        vga_print("\nPress any key to continue...\n");
        char dummy;
        vga_getc(&dummy);
    } else {
        show_system_summary();
        char dummy;
        vga_getc(&dummy);
    }
}

int main(int argc, char** argv) {
    system_info_run(argc, argv);
    return 0;
}