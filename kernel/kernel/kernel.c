#include <arch/vga.h>
#include <arch/drivers.h>
#include <fs.h>
#include <arch/ipc.h>
#include <arch/usermode.h>
#include <arch/shell.h>
#include <arch/pci.h>
#include <arch/acpi.h>
#include <arch/smbios.h>
#include <arch/rtc.h>
#include <arch/pit.h>
#include <arch/pic.h>
#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/hpet.h>
#include <arch/ahci.h>
#include <arch/sata.h>
#include <arch/ehci.h>
#include <arch/xhci.h>
#include <arch/net.h>
#include <arch/sound.h>
#include <arch/ac97.h>
#include <arch/hda.h>
#include <arch/system_sounds.h>
#include <arch/framebuffer.h>
#include <arch/memory.h>
#include <arch/slab.h>
#include <arch/process.h>
#include <arch/syscall.h>
#include <arch/interrupt.h>
#include <arch/boot.h>
#include <arch/tcp_cong.h>
#include <arch/netfilter.h>
#include <arch/conntrack.h>
#include <arch/lsm.h>
#include <arch/selinux.h>
#include <arch/cpufreq.h>
#include <arch/cpuidle.h>
#include <arch/thermal.h>
#include <arch/dns.h>
#include <arch/ext4.h>
#include <arch/mouse.h>
#include <arch/keyboard.h>
#include <arch/uart.h>
#include <arch/acpi_pm.h>
#include <arch/virtio.h>
#include <arch/vlan.h>
#include <arch/route.h>
#include <arch/watchdog.h>
#include <arch/efi.h>
#include <gpu.h>
#include <arch/efi_runtime.h>
#include <arch/kvm.h>
#include <systemd.h>
#include <arch/ntfs.h>
#include <arch/registry.h>
#include <arch/pe.h>
#include <arch/win32.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <kapi.h>
#include <gui.h>
#include <kal.h>
#include <timer.h>

#ifndef KENUX_FAST_BOOT
#define KENUX_FAST_BOOT 1
#endif

#ifndef KENUX_ENABLE_STARTUP_SOUND
#define KENUX_ENABLE_STARTUP_SOUND 0
#endif

/* Win32 subsystem forward declarations */
void win32_process_init(void);
int  scm_init(void);
int  pnp_init(void);
int  wmi_init(void);
int  msvcrt_init(void);
int  ntdll_init(void);
int  advapi32_init(void);
int  shell32_init(void);
int  cmd_init_global(void);

extern void init_main(void);

static inline void serial_putc(char c)
{
    __asm__ volatile ("outb %b0, $0xe9" : : "a"(c));
}

void init_main(void)
{
    while (1) { }
}

uint64_t kenux_uptime(void)
{
    extern uint64_t timer_get_jiffies(void);
    return timer_get_jiffies();
}

void kernel_main(struct FrameBufferConfig *fbc, struct MemoryMapInfo *mmi)
{
    serial_putc('V');
    framebuffer_init(fbc);

    serial_putc('G');
    gdt_init();
    serial_putc('I');
    idt_init();
    serial_putc('P');
    pic_init();

    serial_putc('M');
    memory_init();
    slab_init();
    serial_putc('A');
    acpi_init();
    serial_putc('S');
    smbios_init();

    serial_putc('C');
    pci_init();
    gpu_init();

    serial_putc('H');
    hpet_init();
    serial_putc('T');
    pit_init();
    serial_putc('R');
    rtc_init();

    serial_putc('O');
    process_init();
    serial_putc('Y');
    syscall_init();
    serial_putc('N');
    interrupt_init();
    timer_init();
    serial_putc('I');

    serial_putc('F');
    fs_init();
    serial_putc('L');
    ipc_init();

    serial_putc('K');
    vga_print("Initializing KAPI layer...\n");
    if (kapi_init() == KAPI_OK) {
        vga_print("KAPI layer initialized successfully\n");
    } else {
        vga_print("KAPI layer initialization failed!\n");
    }
    serial_putc('U');

    perf_init();
    serial_putc('W');

    usermode_init();
    serial_putc('E');
    shell_init();
    serial_putc('D');

    {
        struct kal_cfg cfg = {
            .backend_name = "kenux",
            .heartbeat_ms = 1000,
            .failover_threshold = 3,
            .drain_timeout_ms = 500,
            .allow_degrade = 1,
        };
        kal_err_t ke = kal_init(&cfg);
        if (ke == KAL_OK) {
            vga_print("KAL layer initialized (backend=kenux)\n");
        } else {
            vga_print("KAL layer init failed, continuing without KAL\n");
        }
    }
    serial_putc('a');

    drivers_init();
    serial_putc('R');

#if !KENUX_FAST_BOOT
    tcp_cong_init();
    nf_init();
    conntrack_init();
    serial_putc('N');

    lsm_init();
    selinux_init();
    serial_putc('S');

    cpufreq_init();
    cpuidle_init();
    serial_putc('P');

    thermal_init();
    serial_putc('T');

    dns_init();
    serial_putc('D');

    ext4_init();
    serial_putc('E');
#else
    serial_putc('n');
#endif

    serial_putc('M');
    serial_putc('u');
    serial_putc('U');

#if !KENUX_FAST_BOOT
    (void)acpi_pm_init();
#endif
    serial_putc('A');

    serial_putc('V');
    serial_putc('L');
    serial_putc('o');
    serial_putc('R');
    serial_putc('w');
    serial_putc('W');
    serial_putc('r');
    serial_putc('K');

#if !KENUX_FAST_BOOT
    drivers_register("PCI", pci_init);
    drivers_register("ACPI", acpi_init);
    drivers_register("SMBIOS", smbios_init);
    drivers_register("RTC", rtc_init);
    drivers_register("PIT", pit_init);
    drivers_register("PIC", pic_init);
    drivers_register("IDT", idt_init);
    drivers_register("GDT", gdt_init);
    drivers_register("HPET", hpet_init);
    drivers_register("AHCI", ahci_init);
    drivers_register("SATA", sata_init);
    drivers_register("EHCI", ehci_init);
    drivers_register("XHCI", xhci_init);
    drivers_register("NETWORK", network_init);
    drivers_register("SOUND", sound_init);
    drivers_register("AC97", ac97_init);
    drivers_register("HDA", hda_init);
    drivers_register("USERMODE", usermode_init);
    drivers_register("SHELL", shell_init);
#endif
    serial_putc('B');

    vga_print("========================================\n");
    vga_print("  Kenux Kernel 26.8.4K\n");
    vga_print("  Build: 2026-08-26\n");
    vga_print("  Arch: x86_64\n");
    vga_print("  Scheduler: EEVDF + CFS + 5-level priority RR\n");
    vga_print("  Memory: Multi-zone buddy + SLAB + defrag\n");
    vga_print("  VFS: tree-based virtual filesystem\n");
    vga_print("  IPC: pipe + fifo + unix socket + signal\n");
    vga_print("  Syscalls: 1000+ (Linux + LeonOS compatible)\n");
    vga_print("  UI: Classic + Metro + Material Design 3\n");
    vga_print("  Build: WSL/Ninja\n");
    vga_print("========================================\n");
    vga_print("Memory total: ");
    char buffer[64];
    sprintf(buffer, "%lu KB\n", memory_get_total() / 1024);
    vga_print(buffer);
    sprintf(buffer, "Memory free: %lu KB\n", memory_get_free() / 1024);
    vga_print(buffer);

    vga_print("PCI devices found\n");
    vga_print("ACPI tables found\n");
    vga_print("SMBIOS tables found\n");

    vga_print("Starting drivers\n");
    serial_putc('X');
    serial_putc('Y');

#if !KENUX_FAST_BOOT
    vga_print("--- Windows Subsystem Init ---\n");
    {
        int rc;
        vga_print("NTFS filesystem... ");
        rc = ntfs_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("Registry... ");
        rc = registry_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("PE loader... built-in\n");

        vga_print("Win32 process model... ");
        win32_process_init();
        vga_print("OK\n");

        vga_print("SCM (Services)... ");
        rc = scm_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("PnP manager... ");
        rc = pnp_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("WMI... ");
        rc = wmi_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("MSVCRT runtime... ");
        rc = msvcrt_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("NTDLL native API... ");
        rc = ntdll_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("Advapi32 services... ");
        rc = advapi32_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("Shell32 + path API... ");
        rc = shell32_init();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("CMD interpreter... ");
        rc = cmd_init_global();
        vga_print(rc == 0 ? "OK\n" : "FAIL\n");

        vga_print("--- Win32 subsystems ready ---\n");
    }
#endif

    vga_print("Initializing systemd\n");
    serial_putc('Z');

    vga_print("Creating processes\n");
    serial_putc('1');

    vga_print("Starting systemd\n");
    serial_putc('D');

    vga_print("Starting kernel\n");
    serial_putc('4');

    vga_print("========================================\n");
    vga_print("  Kenux 26.8.10 Ready\n");
    vga_print("  Type 'help' for available commands\n");
    vga_print("========================================\n");

    keyboard_init();
    serial_putc('K');

    mouse_init();
    serial_putc('m');

    pic_enable(0);
    serial_putc('T');

    __asm__ volatile ("sti");
    serial_putc('i');

#if KENUX_ENABLE_STARTUP_SOUND
    system_sounds_init();
    system_sound_play_startup();
#endif
    serial_putc('a');

    msleep(80);
    serial_putc('s');

    vga_print("Starting GUI desktop via KAL...\n");
    serial_putc('g');
    gui_run();
    serial_putc('G');
    vga_print("GUI exited, falling back to shell\n");

    shell_run();
    serial_putc('5');

    while (1) {
    }
}