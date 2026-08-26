/*
 * Kenux OS - QEMU Launcher / Manager (Minimal)
 * Implementation
 */

#include "qemu.h"

static const char *TARGET_BINS[] = {
    [QEMU_TGT_X86_64]        = "qemu-system-x86_64",
    [QEMU_TGT_I386]          = "qemu-system-i386",
    [QEMU_TGT_AARCH64]       = "qemu-system-aarch64",
    [QEMU_TGT_ARM]           = "qemu-system-arm",
    [QEMU_TGT_RISCV64]       = "qemu-system-riscv64",
    [QEMU_TGT_RISCV32]       = "qemu-system-riscv32",
    [QEMU_TGT_PPC64]         = "qemu-system-ppc64",
    [QEMU_TGT_PPC]           = "qemu-system-ppc",
    [QEMU_TGT_MIPS64]        = "qemu-system-mips64",
    [QEMU_TGT_LOONGARCH64]   = "qemu-system-loongarch64",
    [QEMU_TGT_S390X]         = "qemu-system-s390x",
    [QEMU_TGT_SPARC64]       = "qemu-system-sparc64",
    [QEMU_TGT_USER_X86_64]   = "qemu-x86_64",
    [QEMU_TGT_USER_AARCH64]  = "qemu-aarch64",
};

void qemu_init(QemuState *s) {
    memset(s, 0, sizeof(*s));
    s->target = QEMU_TGT_X86_64;
    s->machine = QEMU_MACHINE_PC_Q35;
    s->cpu_model = QEMU_CPU_QEMU64;
    s->accelerator = QEMU_ACCEL_TCG;
    s->smp_cpus = 2;
    s->smp_sockets = 1;
    s->smp_cores = 2;
    s->smp_threads = 1;
    s->smp_dies = 1;
    s->mem_bytes = 2ULL * 1024 * 1024 * 1024;    /* 2 GiB default */
    s->mem_slots = QEMU_MAX_MEMSLOTS;
    s->max_mem_size_mb = 8192;
    s->display = QEMU_DISPLAY_DEFAULT;
    s->vga = QEMU_VGA_STD;
    s->audio_driver = QEMU_AUDIO_NONE;
    s->audio_codec = QEMU_AC97;
    s->boot_order_enabled = 1;
    strcpy(s->boot_order, "cdn");      /* cdrom, disk, network */
    s->rtc_clock_host = 1;
    strcpy(s->rtc_base, "utc");
    s->reboot_action = QEMU_ON_EXIT_REBOOT;
    s->panic_action = QEMU_ON_EXIT_PANIC;
    s->shutdown_action = QEMU_ON_EXIT_SHUTDOWN;
    s->xhci = 1;
    s->usb_tablet = 1;
    s->usb_kbd = 1;
    s->virtio_rng = 1;
    s->virtio_balloon = 1;
    s->sandbox_on = 0;
    s->dry_run = 0;
    s->verbose = 0;
    s->pid = 0;
    strcpy(s->name, "KenuxK-Guest");
    /* default nic */
    s->nic_count = 1;
    s->nics[0].backend = QEMU_NET_USER;
    s->nics[0].model = QEMU_NIC_E1000E;
    s->nics[0].net_nr = 0;
    strcpy(s->nics[0].hostname, "kenuxk-vm");
    strcpy(s->nics[0].id, "net0");
    /* default serial */
    s->serial_count = 1;
    s->serials[0].backend = QEMU_CHR_STDIO;
    strcpy(s->serials[0].id, "serial0");
    /* default monitor */
    s->monitor_chr.backend = QEMU_CHR_NULL;
    strcpy(s->monitor_chr.id, "mon0");
}

const char *qemu_target_name(QemuTarget t) {
    return (t < sizeof(TARGET_BINS)/sizeof(TARGET_BINS[0]) && TARGET_BINS[t]) ? TARGET_BINS[t] : "qemu-system-x86_64";
}

const char *qemu_machine_name(QemuMachineType m) {
    switch (m) {
        case QEMU_MACHINE_PC_Q35: return "q35";
        case QEMU_MACHINE_PC_I440FX: return "pc";
        case QEMU_MACHINE_MICROVM: return "microvm";
        case QEMU_MACHINE_ISAPC: return "isapc";
        case QEMU_MACHINE_VIRT_AARCH64: return "virt";
        case QEMU_MACHINE_VIRT_RISCV: return "virt";
        case QEMU_MACHINE_PSERIES: return "pseries";
        default: return "q35";
    }
}

const char *qemu_cpu_name(QemuCpuModel c) {
    switch (c) {
        case QEMU_CPU_MAX: return "max";
        case QEMU_CPU_HOST: return "host";
        case QEMU_CPU_QEMU64: return "qemu64";
        case QEMU_CPU_NEHALEM: return "Nehalem";
        case QEMU_CPU_SANDYBRIDGE: return "SandyBridge";
        case QEMU_CPU_IVYBRIDGE: return "IvyBridge";
        case QEMU_CPU_HASWELL: return "Haswell";
        case QEMU_CPU_BROADWELL: return "Broadwell";
        case QEMU_CPU_SKYLAKE_CLIENT: return "Skylake-Client";
        case QEMU_CPU_SKYLAKE_SERVER: return "Skylake-Server";
        case QEMU_CPU_ICELAKE: return "Icelake-Server";
        case QEMU_CPU_TIGERLAKE: return "Tigerlake";
        case QEMU_CPU_ALDERLAKE: return "Alderlake";
        case QEMU_CPU_EPYC: return "EPYC";
        case QEMU_CPU_EPYC_ROME: return "EPYC-Rome";
        case QEMU_CPU_EPYC_MILAN: return "EPYC-Milan";
        case QEMU_CPU_CORTEX_A53: return "cortex-a53";
        case QEMU_CPU_CORTEX_A57: return "cortex-a57";
        case QEMU_CPU_CORTEX_A72: return "cortex-a72";
        case QEMU_CPU_RV64: return "rv64";
        case QEMU_CPU_POWER9: return "power9";
        case QEMU_CPU_POWER10: return "power10";
        default: return "qemu64";
    }
}

const char *qemu_accel_name(QemuAccel a) {
    switch (a) {
        case QEMU_ACCEL_TCG: return "tcg";
        case QEMU_ACCEL_KVM: return "kvm";
        case QEMU_ACCEL_HAX: return "hax";
        case QEMU_ACCEL_HVF: return "hvf";
        case QEMU_ACCEL_WHPX: return "whpx";
        case QEMU_ACCEL_TCG_MT: return "tcg,thread=multi";
        case QEMU_ACCEL_XEN: return "xen";
        default: return "tcg";
    }
}

const char *qemu_bus_name(QemuDiskBus b) {
    switch (b) {
        case QEMU_BUS_IDE: return "ide";
        case QEMU_BUS_SATA: return "sata";
        case QEMU_BUS_SCSI: return "scsi";
        case QEMU_BUS_NVME: return "nvme";
        case QEMU_BUS_VIRTIO: return "virtio";
        case QEMU_BUS_FLOPPY: return "floppy";
        case QEMU_BUS_SD: return "sd";
        default: return "none";
    }
}

const char *qemu_disk_format_name(QemuDiskFormat f) {
    switch (f) {
        case QEMU_FMT_RAW: return "raw";
        case QEMU_FMT_QCOW2: return "qcow2";
        case QEMU_FMT_QCOW3: return "qcow2";
        case QEMU_FMT_VMDK: return "vmdk";
        case QEMU_FMT_VDI: return "vdi";
        case QEMU_FMT_VHDX: return "vhdx";
        case QEMU_FMT_VPC: return "vpc";
        case QEMU_FMT_ISO: return "raw";
        default: return "raw";
    }
}

const char *qemu_nic_model_name(QemuNicModel n) {
    switch (n) {
        case QEMU_NIC_E1000: return "e1000";
        case QEMU_NIC_E1000E: return "e1000e";
        case QEMU_NIC_VIRTIO: return "virtio-net-pci";
        case QEMU_NIC_RTL8139: return "rtl8139";
        case QEMU_NIC_NE2K_PCI: return "ne2k_pci";
        case QEMU_NIC_PCNET: return "pcnet";
        case QEMU_NIC_VMXNET3: return "vmxnet3";
        default: return "e1000e";
    }
}

const char *qemu_vga_name(QemuVgaType v) {
    switch (v) {
        case QEMU_VGA_STD: return "std";
        case QEMU_VGA_CIRRUS: return "cirrus";
        case QEMU_VGA_VMWARE: return "vmware";
        case QEMU_VGA_QXL: return "qxl";
        case QEMU_VGA_VIRTIO_VGA: return "virtio-vga";
        case QEMU_VGA_VIRTIO_GL: return "virtio-gpu-gl";
        case QEMU_VGA_RAMDAC: return "none";
        case QEMU_VGA_NONE: return "none";
        default: return "std";
    }
}

int qemu_resolve_binary(QemuState *s) {
    const char *base = qemu_target_name(s->target);
    static const char *prefixes[] = { "/usr/bin/", "/usr/local/bin/", "/opt/qemu/bin/", "", NULL };
    struct stat st;
    for (int i = 0; prefixes[i]; i++) {
        snprintf(s->qemu_bin, QEMU_MAX_PATH, "%s%s", prefixes[i], base);
        if (stat(s->qemu_bin, &st) == 0) return 0;
    }
    snprintf(s->qemu_bin, QEMU_MAX_PATH, "%s", base);
    return 0;
}

static int parse_mem_size(const char *s, size_t *out) {
    char *end = NULL;
    size_t v = strtoull(s, &end, 0);
    if (!end) return -1;
    if (*end == 'G' || *end == 'g') *out = v * 1024ULL * 1024ULL * 1024ULL;
    else if (*end == 'M' || *end == 'm') *out = v * 1024ULL * 1024ULL;
    else if (*end == 'K' || *end == 'k') *out = v * 1024ULL;
    else *out = v;
    return 0;
}

static const char *chrid_for_backend(QemuChardevBackend b) {
    switch (b) {
        case QEMU_CHR_STDIO: return "stdio";
        case QEMU_CHR_FILE:  return "file";
        case QEMU_CHR_PTY:   return "pty";
        case QEMU_CHR_PIPE:  return "pipe";
        case QEMU_CHR_SOCKET:return "socket";
        case QEMU_CHR_UDP:   return "udp";
        case QEMU_CHR_TCP:   return "telnet";
        case QEMU_CHR_NULL:  return "null";
        default: return "null";
    }
}

int qemu_parse_arguments(QemuState *s, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i]; if (!a) continue;
        /* Target shortcuts: if invoked via symlink qemu-system-xxx */
        if (strcmp(a, "-machine") == 0 || strcmp(a, "-M") == 0) {
            const char *v = (++i < argc) ? argv[i] : "";
            if (strcmp(v, "q35") == 0) s->machine = QEMU_MACHINE_PC_Q35;
            else if (strcmp(v, "pc") == 0) s->machine = QEMU_MACHINE_PC_I440FX;
            else if (strcmp(v, "microvm") == 0) s->machine = QEMU_MACHINE_MICROVM;
            else if (strcmp(v, "virt") == 0) {
                if (s->target == QEMU_TGT_AARCH64 || s->target == QEMU_TGT_ARM)
                    s->machine = QEMU_MACHINE_VIRT_AARCH64;
                else if (s->target == QEMU_TGT_RISCV64 || s->target == QEMU_TGT_RISCV32)
                    s->machine = QEMU_MACHINE_VIRT_RISCV;
            } else if (strcmp(v, "pseries") == 0) s->machine = QEMU_MACHINE_PSERIES;
        } else if (strcmp(a, "-cpu") == 0) {
            const char *v = (++i < argc) ? argv[i] : "max";
            if (strcmp(v, "host") == 0) { s->cpu_model = QEMU_CPU_HOST; s->accelerator = QEMU_ACCEL_KVM; }
            else if (strcmp(v, "max") == 0) s->cpu_model = QEMU_CPU_MAX;
            else if (strcmp(v, "qemu64") == 0) s->cpu_model = QEMU_CPU_QEMU64;
        } else if (strcmp(a, "-accel") == 0) {
            const char *v = (++i < argc) ? argv[i] : "tcg";
            if (strcmp(v, "kvm") == 0) s->accelerator = QEMU_ACCEL_KVM;
            else if (strcmp(v, "hvf") == 0) s->accelerator = QEMU_ACCEL_HVF;
            else if (strcmp(v, "hax") == 0) s->accelerator = QEMU_ACCEL_HAX;
            else if (strcmp(v, "whpx") == 0) s->accelerator = QEMU_ACCEL_WHPX;
            else s->accelerator = QEMU_ACCEL_TCG;
        } else if (strcmp(a, "-enable-kvm") == 0) {
            s->accelerator = QEMU_ACCEL_KVM; s->cpu_model = QEMU_CPU_HOST;
        } else if (strcmp(a, "-smp") == 0) {
            const char *v = (++i < argc) ? argv[i] : "2";
            int n = atoi(v);
            if (n > 0) s->smp_cpus = n;
        } else if (strcmp(a, "-m") == 0) {
            const char *v = (++i < argc) ? argv[i] : "2G";
            parse_mem_size(v, &s->mem_bytes);
        } else if (strcmp(a, "-cdrom") == 0 && i + 1 < argc) {
            QemuDisk *d = &s->disks[s->disk_count++];
            strncpy(d->path, argv[++i], QEMU_MAX_PATH - 1);
            d->bus = QEMU_BUS_IDE;
            d->format = QEMU_FMT_ISO;
            d->bootindex = 1;
            d->readonly = 1;
            strcpy(d->device_id, "cd0");
        } else if (strcmp(a, "-hda") == 0 && i + 1 < argc) {
            QemuDisk *d = &s->disks[s->disk_count++];
            strncpy(d->path, argv[++i], QEMU_MAX_PATH - 1);
            d->bus = QEMU_BUS_IDE; d->format = QEMU_FMT_QCOW2;
            d->bootindex = 2;
        } else if (strcmp(a, "-drive") == 0 && i + 1 < argc) {
            QemuDisk *d = &s->disks[s->disk_count++];
            const char *v = argv[++i];
            if (strstr(v, "file=")) {
                const char *f = strstr(v, "file=") + 5;
                sscanf(f, "%4095[^,]", d->path);
            }
            if (strstr(v, "if=virtio")) d->bus = QEMU_BUS_VIRTIO;
            else if (strstr(v, "if=ide")) d->bus = QEMU_BUS_IDE;
            else if (strstr(v, "if=none")) d->if_none = 1;
            else if (strstr(v, "if=pflash")) { strncpy(s->pflash[s->pflash[0][0]?1:0], d->path, QEMU_MAX_PATH - 1); }
            if (strstr(v, "format=qcow2")) d->format = QEMU_FMT_QCOW2;
            else if (strstr(v, "format=raw")) d->format = QEMU_FMT_RAW;
            else if (strstr(v, "format=vmdk")) d->format = QEMU_FMT_VMDK;
            if (strstr(v, "readonly")) d->readonly = 1;
        } else if (strcmp(a, "-kernel") == 0 && i + 1 < argc) {
            strncpy(s->kernel_image, argv[++i], QEMU_MAX_PATH - 1);
        } else if (strcmp(a, "-initrd") == 0 && i + 1 < argc) {
            strncpy(s->initrd, argv[++i], QEMU_MAX_PATH - 1);
        } else if (strcmp(a, "-append") == 0 && i + 1 < argc) {
            strncpy(s->kernel_cmdline, argv[++i], 4095);
        } else if (strcmp(a, "-bios") == 0 && i + 1 < argc) {
            strncpy(s->bios, argv[++i], QEMU_MAX_PATH - 1);
        } else if (strcmp(a, "-display") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "none") == 0) s->display = QEMU_DISPLAY_NONE;
            else if (strcmp(v, "sdl") == 0) s->display = QEMU_DISPLAY_SDL;
            else if (strcmp(v, "gtk") == 0) s->display = QEMU_DISPLAY_GTK;
            else if (strcmp(v, "curses") == 0) s->display = QEMU_DISPLAY_CURSES;
            else if (strncmp(v, "vnc", 3) == 0) { s->display = QEMU_DISPLAY_VNC; strncpy(s->display_opts, v, sizeof(s->display_opts)-1); }
        } else if (strcmp(a, "-vga") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "none") == 0) s->vga = QEMU_VGA_NONE;
            else if (strcmp(v, "std") == 0) s->vga = QEMU_VGA_STD;
            else if (strcmp(v, "qxl") == 0) s->vga = QEMU_VGA_QXL;
            else if (strcmp(v, "virtio") == 0) s->vga = QEMU_VGA_VIRTIO_VGA;
            else if (strcmp(v, "vmware") == 0) s->vga = QEMU_VGA_VMWARE;
            else if (strcmp(v, "cirrus") == 0) s->vga = QEMU_VGA_CIRRUS;
        } else if (strcmp(a, "-nographic") == 0) {
            s->display = QEMU_DISPLAY_NONE; s->display_headless = 1;
            s->serials[0].backend = QEMU_CHR_STDIO;
            s->monitor_chr.backend = QEMU_CHR_NULL;
        } else if (strcmp(a, "-serial") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "stdio") == 0) s->serials[0].backend = QEMU_CHR_STDIO;
            else if (strcmp(v, "null") == 0) s->serials[0].backend = QEMU_CHR_NULL;
            else if (strcmp(v, "file:") == 0 || strncmp(v, "file:", 5) == 0) {
                s->serials[0].backend = QEMU_CHR_FILE;
                strncpy(s->serials[0].path, v + 5, QEMU_MAX_PATH - 1);
            } else if (strncmp(v, "mon:", 4) == 0 || strncmp(v, "telnet:", 7) == 0) {
                s->serials[0].backend = QEMU_CHR_TCP;
            }
        } else if (strcmp(a, "-monitor") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "stdio") == 0) s->monitor_chr.backend = QEMU_CHR_STDIO;
            else if (strncmp(v, "telnet:", 7) == 0) {
                s->monitor_chr.backend = QEMU_CHR_TCP;
                sscanf(v + 7, "%63[^:]:%d", s->monitor_chr.host, &s->monitor_chr.port);
                s->monitor_chr.server = 1; s->monitor_chr.nowait = 1; s->monitor_chr.telnet = 1;
            }
        } else if (strcmp(a, "-net") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "none") == 0) { s->net_none = 1; s->nic_count = 0; }
        } else if (strcmp(a, "-netdev") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (s->nic_count >= QEMU_MAX_NICS) continue;
            QemuNic *n = &s->nics[s->nic_count++];
            if (strstr(v, "user")) n->backend = QEMU_NET_USER;
            else if (strstr(v, "tap")) n->backend = QEMU_NET_TAP;
            else if (strstr(v, "bridge")) n->backend = QEMU_NET_BRIDGE;
            if (strstr(v, "id=")) sscanf(strstr(v, "id="), "id=%63[^,]", n->id);
        } else if (strcmp(a, "-device") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strncmp(v, "virtio-net", 10) == 0) { /* handled via -netdev later */ }
            if (strstr(v, "usb-tablet") || strncmp(v, "usb-tablet", 10) == 0) s->usb_tablet = 1;
            if (strncmp(v, "virtio-rng", 10) == 0) s->virtio_rng = 1;
            if (strncmp(v, "virtio-balloon", 14) == 0) s->virtio_balloon = 1;
            if (strncmp(v, "virtio-gpu", 10) == 0) s->vga = QEMU_VGA_VIRTIO_VGA;
            if (strncmp(v, "qemu-xhci", 9) == 0 || strstr(v, "xhci")) s->xhci = 1;
        } else if (strcmp(a, "-smp") == 0 || strcmp(a, "-boot") == 0 && i + 1 < argc) {
            const char *v = (strcmp(a, "-boot") == 0 && i + 1 < argc) ? argv[++i] : NULL;
            if (v && v[0] >= 'a' && v[0] <= 'z') {
                strncpy(s->boot_order, v, sizeof(s->boot_order) - 1);
                s->boot_order_enabled = 1;
            }
        } else if (strcmp(a, "-usb") == 0) s->usb_enabled = 1;
        else if (strcmp(a, "-device") == 0) { }
        else if (strcmp(a, "-name") == 0 && i + 1 < argc) {
            strncpy(s->name, argv[++i], sizeof(s->name) - 1);
        } else if (strcmp(a, "-uuid") == 0 && i + 1 < argc) {
            strncpy(s->uuid, argv[++i], sizeof(s->uuid) - 1);
        } else if (strcmp(a, "-pidfile") == 0 && i + 1 < argc) {
            s->pidfile_enabled = 1; strncpy(s->pidfile, argv[++i], QEMU_MAX_PATH - 1);
        } else if (strcmp(a, "-daemonize") == 0) s->daemonize = 1;
        else if (strcmp(a, "-no-reboot") == 0) s->no_reboot = 1;
        else if (strcmp(a, "-no-shutdown") == 0) s->no_shutdown = 1;
        else if (strcmp(a, "-S") == 0) {} /* freeze at startup */
        else if (strcmp(a, "-s") == 0) { s->s = 1; s->gdb_enabled = 1; s->gdb_port = 1234; }
        else if (strcmp(a, "-gdb") == 0 && i + 1 < argc) {
            s->gdb_enabled = 1;
            const char *v = argv[++i];
            sscanf(v, "tcp::%d", &s->gdb_port);
            if (s->gdb_port == 0) s->gdb_port = 1234;
        } else if (strcmp(a, "-sandbox") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strncmp(v, "on", 2) == 0) s->sandbox_on = 1;
        } else if (strcmp(a, "-runas") == 0 && i + 1 < argc) {
            strncpy(s->runas_user, argv[++i], sizeof(s->runas_user) - 1);
        } else if (strcmp(a, "-chroot") == 0 && i + 1 < argc) {
            strncpy(s->chroot_dir, argv[++i], QEMU_MAX_PATH - 1);
        } else if (strcmp(a, "-rtc") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strstr(v, "base=localtime")) { strcpy(s->rtc_base, "localtime"); s->rtc_clock_host = 1; }
            if (strstr(v, "clock=host")) s->rtc_clock_host = 1;
        } else if (strcmp(a, "-k") == 0 && i + 1 < argc) {
            strncpy(s->timezone, argv[++i], sizeof(s->timezone) - 1);
        } else if (strcmp(a, "-watchdog-action") == 0 && i + 1 < argc) {
            s->watchdog_action = 1; i++;
        } else if (strcmp(a, "-nodefaults") == 0) {
            s->nic_count = 0; s->serial_count = 0; s->usb_kbd = 0; s->usb_tablet = 0; s->vga = QEMU_VGA_NONE;
        } else if (strcmp(a, "-version") == 0 || strcmp(a, "--version") == 0) s->version = 1;
        else if (strcmp(a, "-help") == 0 || strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) s->help = 1;
        else if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) s->verbose++;
        else if (strcmp(a, "-dry-run") == 0) s->dry_run = 1;
        else if (strcmp(a, "-name") == 0) { i++; }
    }
    return 0;
}

int qemu_build_command_line(QemuState *s, char **argv_out, int *out_argc) {
    int ai = 0;
    char buf[2048];
    if (!argv_out || !out_argc) return -1;
    argv_out[ai++] = s->qemu_bin;
    argv_out[ai++] = "-name";
    snprintf(buf, sizeof(buf), "%s,process=%s", s->name, s->name);
    argv_out[ai++] = strdup(buf);
    argv_out[ai++] = "-machine";
    snprintf(buf, sizeof(buf), "%s,accel=%s,usb=%soff",
             qemu_machine_name(s->machine),
             qemu_accel_name(s->accelerator),
             s->usb_enabled ? "on," : "");
    argv_out[ai++] = strdup(buf);
    argv_out[ai++] = "-cpu";
    argv_out[ai++] = (char *)qemu_cpu_name(s->cpu_model);
    argv_out[ai++] = "-smp";
    snprintf(buf, sizeof(buf), "%d,sockets=%d,dies=%d,cores=%d,threads=%d,maxcpus=%d",
        s->smp_cpus, s->smp_sockets, s->smp_dies, s->smp_cores, s->smp_threads,
        s->smp_sockets * s->smp_dies * s->smp_cores * s->smp_threads);
    argv_out[ai++] = strdup(buf);
    argv_out[ai++] = "-m";
    snprintf(buf, sizeof(buf), "%zuM,slots=%d,maxmem=%dM",
        s->mem_bytes / 1024 / 1024, s->mem_slots, s->max_mem_size_mb);
    argv_out[ai++] = strdup(buf);
    if (s->bios[0]) { argv_out[ai++] = "-bios"; argv_out[ai++] = s->bios; }
    if (s->pflash[0][0]) {
        argv_out[ai++] = "-drive";
        snprintf(buf, sizeof(buf), "if=pflash,format=raw,readonly=on,file=%s", s->pflash[0]);
        argv_out[ai++] = strdup(buf);
    }
    if (s->pflash[1][0]) {
        argv_out[ai++] = "-drive";
        snprintf(buf, sizeof(buf), "if=pflash,format=raw,file=%s", s->pflash[1]);
        argv_out[ai++] = strdup(buf);
    }
    if (s->kernel_image[0]) { argv_out[ai++] = "-kernel"; argv_out[ai++] = s->kernel_image; }
    if (s->initrd[0]) { argv_out[ai++] = "-initrd"; argv_out[ai++] = s->initrd; }
    if (s->kernel_cmdline[0]) { argv_out[ai++] = "-append"; argv_out[ai++] = s->kernel_cmdline; }
    for (int d = 0; d < s->disk_count; d++) {
        QemuDisk *dk = &s->disks[d];
        argv_out[ai++] = "-drive";
        snprintf(buf, sizeof(buf),
            "file=%s,format=%s,if=%s%s%s%s%s,id=dr%d",
            dk->path[0] ? dk->path : "/dev/null",
            qemu_disk_format_name(dk->format),
            dk->if_none ? "none" : qemu_bus_name(dk->bus),
            dk->readonly ? ",readonly=on" : "",
            dk->snapshot ? ",snapshot=on" : "",
            dk->cache_mode[0] ? dk->cache_mode : ",cache=writeback",
            d);
        argv_out[ai++] = strdup(buf);
    }
    if (s->boot_order_enabled) {
        argv_out[ai++] = "-boot";
        snprintf(buf, sizeof(buf), "order=%s,menu=on,splash-time=1500", s->boot_order);
        argv_out[ai++] = strdup(buf);
    }
    if (s->display_headless) {
        argv_out[ai++] = "-nographic";
    } else if (s->display != QEMU_DISPLAY_DEFAULT) {
        argv_out[ai++] = "-display";
        const char *dsp = "gtk";
        switch (s->display) {
            case QEMU_DISPLAY_SDL: dsp = "sdl"; break;
            case QEMU_DISPLAY_GTK: dsp = "gtk"; break;
            case QEMU_DISPLAY_VNC: dsp = s->display_opts[0] ? s->display_opts : "vnc=localhost:0"; break;
            case QEMU_DISPLAY_CURSES: dsp = "curses"; break;
            case QEMU_DISPLAY_NONE: dsp = "none"; break;
            default: break;
        }
        argv_out[ai++] = strdup(dsp);
    }
    argv_out[ai++] = "-vga";
    argv_out[ai++] = (char *)qemu_vga_name(s->vga);
    /* RTC */
    argv_out[ai++] = "-rtc";
    snprintf(buf, sizeof(buf), "base=%s,clock=%s,driftfix=slew",
        s->rtc_base, s->rtc_clock_host ? "host" : "vm");
    argv_out[ai++] = strdup(buf);
    if (s->xhci) { argv_out[ai++] = "-device"; argv_out[ai++] = "qemu-xhci,id=xhci"; }
    if (s->usb_kbd)     { argv_out[ai++] = "-device"; argv_out[ai++] = "usb-kbd,bus=xhci.0"; }
    if (s->usb_tablet)  { argv_out[ai++] = "-device"; argv_out[ai++] = "usb-tablet,bus=xhci.0"; }
    if (s->virtio_rng)  { argv_out[ai++] = "-object"; argv_out[ai++] = "rng-random,filename=/dev/urandom,id=rng0";
                          argv_out[ai++] = "-device"; argv_out[ai++] = "virtio-rng-pci,rng=rng0"; }
    if (s->virtio_balloon) { argv_out[ai++] = "-device"; argv_out[ai++] = "virtio-balloon-pci,id=balloon0"; }
    /* Network */
    for (int n = 0; n < s->nic_count; n++) {
        QemuNic *nic = &s->nics[n];
        char idbuf[64]; snprintf(idbuf, sizeof(idbuf), "net%d", n);
        argv_out[ai++] = "-netdev";
        if (nic->backend == QEMU_NET_USER) {
            snprintf(buf, sizeof(buf), "user,id=%s,hostname=%s", idbuf, nic->hostname[0] ? nic->hostname : "kenuxk");
            for (int hf = 0; hf < nic->hostfwd_count; hf++) {
                size_t bl = strlen(buf);
                snprintf(buf + bl, sizeof(buf) - bl, ",hostfwd=%s", nic->hostfwd[hf]);
            }
        } else if (nic->backend == QEMU_NET_TAP) {
            snprintf(buf, sizeof(buf), "tap,id=%s,ifname=%s,script=no,downscript=no",
                idbuf, nic->ifname[0] ? nic->ifname : "tap0");
        } else if (nic->backend == QEMU_NET_BRIDGE) {
            snprintf(buf, sizeof(buf), "bridge,id=%s,br=%s", idbuf, nic->brname[0] ? nic->brname : "br0");
        } else {
            snprintf(buf, sizeof(buf), "user,id=%s", idbuf);
        }
        argv_out[ai++] = strdup(buf);
        argv_out[ai++] = "-device";
        snprintf(buf, sizeof(buf), "%s,netdev=%s,id=nic%d%s%s",
            qemu_nic_model_name(nic->model), idbuf, n,
            nic->mac[0] ? ",mac=" : "", nic->mac[0] ? nic->mac : "");
        argv_out[ai++] = strdup(buf);
    }
    if (s->net_none) { argv_out[ai++] = "-net"; argv_out[ai++] = "none"; }
    /* Serial / Monitor */
    for (int c = 0; c < s->serial_count; c++) {
        argv_out[ai++] = "-serial";
        argv_out[ai++] = strdup(chrid_for_backend(s->serials[c].backend));
    }
    if (s->monitor_chr.backend != QEMU_CHR_NULL || s->monitor_telnet) {
        argv_out[ai++] = "-monitor";
        if (s->monitor_chr.backend == QEMU_CHR_TCP) {
            snprintf(buf, sizeof(buf), "telnet:%s:%d,server,nowait,nodelay",
                s->monitor_chr.host[0] ? s->monitor_chr.host : "127.0.0.1",
                s->monitor_chr.port ? s->monitor_chr.port : 4444);
            argv_out[ai++] = strdup(buf);
        } else {
            argv_out[ai++] = (char *)chrid_for_backend(s->monitor_chr.backend);
        }
    }
    /* Audio */
    if (s->audio_driver != QEMU_AUDIO_NONE) {
        const char *aname = "none";
        switch (s->audio_driver) {
            case QEMU_AUDIO_SDL: aname = "sdl"; break;
            case QEMU_AUDIO_PULSE: aname = "pa"; break;
            case QEMU_AUDIO_ALSA: aname = "alsa"; break;
            case QEMU_AUDIO_COREAUDIO: aname = "coreaudio"; break;
            case QEMU_AUDIO_DSOUND: aname = "dsound"; break;
            case QEMU_AUDIO_WASAPI: aname = "wasapi"; break;
            case QEMU_AUDIO_JACK: aname = "jack"; break;
            case QEMU_AUDIO_SPICE: aname = "spice"; break;
            default: break;
        }
        argv_out[ai++] = "-audiodev";
        snprintf(buf, sizeof(buf), "%s,id=audio0", aname);
        argv_out[ai++] = strdup(buf);
    }
    /* GDB */
    if (s->gdb_enabled) {
        argv_out[ai++] = "-gdb";
        snprintf(buf, sizeof(buf), "tcp::%d", s->gdb_port);
        argv_out[ai++] = strdup(buf);
    }
    if (s->no_reboot) argv_out[ai++] = "-no-reboot";
    if (s->no_shutdown) argv_out[ai++] = "-no-shutdown";
    if (s->daemonize) argv_out[ai++] = "-daemonize";
    if (s->pidfile_enabled) {
        argv_out[ai++] = "-pidfile";
        argv_out[ai++] = s->pidfile;
    }
    if (s->uuid[0]) { argv_out[ai++] = "-uuid"; argv_out[ai++] = s->uuid; }
    if (s->sandbox_on) { argv_out[ai++] = "-sandbox"; argv_out[ai++] = "on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny"; }
    if (s->chroot_dir[0]) { argv_out[ai++] = "-chroot"; argv_out[ai++] = s->chroot_dir; }
    if (s->runas_user[0]) { argv_out[ai++] = "-runas"; argv_out[ai++] = s->runas_user; }
    if (s->verbose) { argv_out[ai++] = "-d"; argv_out[ai++] = "guest_errors"; }
    argv_out[ai] = NULL;
    *out_argc = ai;
    return 0;
}

int qemu_start(QemuState *s) {
    qemu_resolve_binary(s);
    char *argv[QEMU_MAX_ARGS]; int argc = 0;
    qemu_build_command_line(s, argv, &argc);
    if (s->verbose || s->dry_run) {
        fprintf(stderr, "qemu: command:");
        for (int i = 0; i < argc; i++) fprintf(stderr, " %s", argv[i]);
        fprintf(stderr, "\n");
        if (s->dry_run) return 0;
    }
    pid_t pid = fork();
    if (pid < 0) { perror("qemu: fork"); return -1; }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    s->pid = pid;
    if (s->pidfile_enabled) {
        FILE *f = fopen(s->pidfile, "w");
        if (f) { fprintf(f, "%d\n", pid); fclose(f); }
    }
    return 0;
}

int qemu_wait(QemuState *s, int *exit_status) {
    if (s->pid <= 0) return -1;
    int status = 0;
    waitpid(s->pid, &status, 0);
    if (exit_status) *exit_status = status;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int qemu_terminate(QemuState *s) {
    if (s->pid <= 0) return -1;
    if (kill(s->pid, SIGTERM) < 0) return -1;
    usleep(200000);
    kill(s->pid, SIGKILL);
    return 0;
}

int qemu_create_disk_image(QemuState *s, const QemuDisk *d) {
    (void)s;
    if (!d || !d->path[0]) return -1;
    char cmd[8192];
    const char *fmt = qemu_disk_format_name(d->format);
    uint64_t sz_mb = (d->size_bytes + 1024 * 1024 - 1) / (1024 * 1024);
    snprintf(cmd, sizeof(cmd), "qemu-img create -f %s %s %" PRIu64 "M",
             fmt, d->path, sz_mb ? sz_mb : 8192ULL);
    return system(cmd);
}

int qemu_commit_disk(const char *overlay, const char *backing) {
    if (!overlay) return -1;
    char cmd[8192];
    if (backing && backing[0])
        snprintf(cmd, sizeof(cmd), "qemu-img commit -b %s %s", backing, overlay);
    else
        snprintf(cmd, sizeof(cmd), "qemu-img commit %s", overlay);
    return system(cmd);
}

int qemu_snapshot(const char *disk, const char *tag, int is_save) {
    if (!disk || !tag) return -1;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "qemu-img snapshot %s %s %s",
             is_save ? "-c" : "-a", tag, disk);
    return system(cmd);
}

int qemu_apply_kenuxk_preset(QemuState *s) {
    s->target = QEMU_TGT_X86_64;
    s->machine = QEMU_MACHINE_PC_Q35;
    s->cpu_model = QEMU_CPU_MAX;
    s->accelerator = QEMU_ACCEL_TCG_MT;
    s->smp_cpus = 4; s->smp_cores = 4; s->smp_threads = 1;
    s->mem_bytes = 4ULL * 1024 * 1024 * 1024;
    s->vga = QEMU_VGA_VIRTIO_VGA;
    s->display = QEMU_DISPLAY_GTK;
    s->xhci = 1; s->usb_kbd = 1; s->usb_tablet = 1;
    s->virtio_rng = 1; s->virtio_balloon = 1;
    s->audio_driver = QEMU_AUDIO_PULSE;
    s->audio_codec = QEMU_HDA;
    s->nic_count = 1;
    s->nics[0].backend = QEMU_NET_USER;
    s->nics[0].model = QEMU_NIC_VIRTIO;
    s->nics[0].hostfwd_count = 2;
    strcpy(s->nics[0].hostfwd[0], "tcp::2222-:22");
    strcpy(s->nics[0].hostfwd[1], "tcp::8080-:80");
    return 0;
}

void qemu_print_version(void) {
    printf("%s\n", QEMU_VERSION);
    printf("Supported system targets:");
    for (size_t i = 0; i < sizeof(TARGET_BINS)/sizeof(TARGET_BINS[0]); i++)
        if (TARGET_BINS[i]) printf(" %s", TARGET_BINS[i]);
    printf("\n");
    printf("KenuxK manager wrapper; delegates to native qemu binaries installed on host.\n");
}

void qemu_print_help(void) {
    printf("%s - KenuxK QEMU launcher (minimal wrapper)\n\n", QEMU_VERSION);
    printf("USAGE: qemu [opts]\n\n");
    printf("HARDWARE:\n");
    printf("  -M / -machine TYPE     q35 (default), pc, microvm, virt, pseries\n");
    printf("  -cpu MODEL             qemu64, max, host, Haswell, EPYC, cortex-a57, ...\n");
    printf("  -smp N                 Number of vCPUs\n");
    printf("  -m SIZE                Memory (2G, 4096M, 1T etc.)\n");
    printf("  -accel MODE            tcg, kvm, hvf, hax, whpx\n");
    printf("  -enable-kvm            Shortcut for -accel kvm -cpu host\n");
    printf("FIRMWARE / BOOT:\n");
    printf("  -bios FILE             BIOS/OVMF firmware\n");
    printf("  -drive if=pflash,...   UEFI pflash code/vars\n");
    printf("  -kernel / -initrd / -append  Direct Linux boot\n");
    printf("  -boot order=cdn        Boot order: cd, disk, net\n");
    printf("STORAGE:\n");
    printf("  -drive file=X,format=F,if=B    Disk/CD\n");
    printf("  -cdrom FILE            Attach CD\n");
    printf("  -hda FILE              Attach IDE disk\n");
    printf("                           B= ide|sata|scsi|nvme|virtio|none\n");
    printf("                           F= raw|qcow2|vmdk|vdi|vhdx\n");
    printf("DISPLAY:\n");
    printf("  -display TYPE          sdl, gtk, vnc=host:d, curses, none\n");
    printf("  -vga TYPE              std, vmware, qxl, virtio, cirrus, none\n");
    printf("  -nographic             Headless, serial to stdio\n");
    printf("NETWORK:\n");
    printf("  -netdev user,id=X,hostfwd=tcp::HP-:GP   SLIRP with port forwarding\n");
    printf("  -netdev tap,id=X,ifname=tapX             TAP device\n");
    printf("  -device MODEL,netdev=X                   MODEL: e1000e|virtio-net-pci|rtl8139|vmxnet3\n");
    printf("  -net none                              Disable networking\n");
    printf("I/O DEVICES:\n");
    printf("  -device qemu-xhci              USB 3 xHCI host controller\n");
    printf("  -device usb-kbd / usb-tablet   USB input (tablet=absolute mouse)\n");
    printf("  -device virtio-rng-pci         Randomness source\n");
    printf("  -device virtio-balloon-pci     Balloon driver\n");
    printf("  -serial stdio/file:PATH/telnet:H:P,server   Serial port\n");
    printf("  -monitor stdio/telnet:H:P,server,nowait   Monitor console\n");
    printf("DEBUG / MISC:\n");
    printf("  -s                     Shorthand for -gdb tcp::1234 -S\n");
    printf("  -gdb tcp::PORT         GDB remote stub on PORT\n");
    printf("  -rtc base=utc,clock=host\n");
    printf("  -no-reboot / -no-shutdown\n");
    printf("  -pidfile FILE          Write PID to FILE\n");
    printf("  -daemonize             Detach process\n");
    printf("  -sandbox on            Seccomp hardening\n");
    printf("  -name NAME             Guest name\n");
    printf("  --version              Print version\n");
    printf("  --help                 This help\n");
    printf("\nPRESETS (additional CLI only):\n");
    printf("  --preset kenuxk        Optimised defaults for KenuxK guests (4C/4G/virtio-net/virtio-gpu)\n");
    printf("  --preset secure-boot   Enable OVMF Secure Boot presets\n");
}

static int qemu_main_internal(int argc, char **argv) {
    QemuState s; qemu_init(&s);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            const char *p = argv[++i];
            if (strcmp(p, "kenuxk") == 0) qemu_apply_kenuxk_preset(&s);
            else if (strcmp(p, "secure-boot") == 0) qemu_apply_secure_boot_preset(&s);
        }
    }
    qemu_parse_arguments(&s, argc, argv);
    if (s.version) { qemu_print_version(); return 0; }
    if (s.help)    { qemu_print_help(); return 0; }
    qemu_start(&s);
    int rc = 0;
    qemu_wait(&s, &rc);
    return rc;
}

int qemu_apply_windows_compat_preset(QemuState *s) {
    s->target = QEMU_TGT_X86_64;
    s->machine = QEMU_MACHINE_PC_Q35;
    s->vga = QEMU_VGA_QXL;
    s->display = QEMU_DISPLAY_SPICE;
    return 0;
}

int qemu_apply_secure_boot_preset(QemuState *s) {
    s->machine = QEMU_MACHINE_PC_Q35;
    if (!s->pflash[0][0]) strcpy(s->pflash[0], "/usr/share/OVMF/OVMF_CODE.secboot.fd");
    if (!s->pflash[1][0]) strcpy(s->pflash[1], "OVMF_VARS.secboot.fd");
    return 0;
}

#ifndef KENUXK_NO_MAIN_QEMU
int main(int argc, char **argv) {
    return qemu_main_internal(argc, argv);
}
#endif
