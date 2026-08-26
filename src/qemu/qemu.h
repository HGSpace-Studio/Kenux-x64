/*
 * Kenux OS - QEMU Launcher / System Emulator Manager (Minimal)
 * Header file
 */

#ifndef _QEMU_H
#define _QEMU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#ifdef _WIN32
/* MinGW 兼容: unistd.h/sys/wait.h 部分缺失 */
#include <unistd.h>
#include <process.h>
#include <io.h>
#include <inttypes.h>
#define fork()         (-1)
#define waitpid(p,s,o) (-1)
#define kill(p,s)      (-1)
#ifndef SIGKILL
#define SIGKILL 9
#endif
/* WIFEXITED/WEXITSTATUS stubs */
#define WIFEXITED(s)   (1)
#define WEXITSTATUS(s) (0)
static inline int getuid(void) { return 0; }
static inline int getgid(void) { return 0; }
#else
#include <unistd.h>
#include <sys/wait.h>
#include <inttypes.h>
#endif

#define QEMU_VERSION "KenuxK-QEMU 8.2 (Minimal Manager)"
#define QEMU_MAX_PATH 4096
#define QEMU_MAX_ARGS 512
#define QEMU_MAX_DEVICES 64
#define QEMU_MAX_NICS 16
#define QEMU_MAX_DISKS 16
#define QEMU_MAX_CPUS 256
#define QEMU_MAX_NUMA 16
#define QEMU_MAX_MEMSLOTS 8
#define QEMU_MAX_FW 16
#define QEMU_MAX_CHARS 16
#define QEMU_MAX_AUDIO 4

/* Emulator target */
typedef enum {
    QEMU_TGT_X86_64 = 0,  /* qemu-system-x86_64 */
    QEMU_TGT_I386,         /* qemu-system-i386 */
    QEMU_TGT_AARCH64,      /* qemu-system-aarch64 */
    QEMU_TGT_ARM,          /* qemu-system-arm */
    QEMU_TGT_RISCV64,      /* qemu-system-riscv64 */
    QEMU_TGT_RISCV32,
    QEMU_TGT_PPC64,
    QEMU_TGT_PPC,
    QEMU_TGT_MIPS64,
    QEMU_TGT_LOONGARCH64,
    QEMU_TGT_S390X,
    QEMU_TGT_SPARC64,
    QEMU_TGT_USER_X86_64, /* qemu-x86_64 user-mode */
    QEMU_TGT_USER_AARCH64,
} QemuTarget;

/* Machine type */
typedef enum {
    QEMU_MACHINE_DEFAULT = 0,
    QEMU_MACHINE_PC_Q35,       /* q35, the modern x86 chipset */
    QEMU_MACHINE_PC_I440FX,    /* piix3, classic */
    QEMU_MACHINE_MICROVM,      /* tiny */
    QEMU_MACHINE_ISAPC,        /* ISA-only */
    QEMU_MACHINE_VIRT_AARCH64, /* virt for arm64 */
    QEMU_MACHINE_VIRT_RISCV,   /* virt for riscv */
    QEMU_MACHINE_PSERIES,      /* pseries for ppc64 */
    QEMU_MACHINE_S390_CCWS,
    QEMU_MACHINE_SUN4U,
} QemuMachineType;

/* CPU models */
typedef enum {
    QEMU_CPU_MAX = 0,          /* -cpu max */
    QEMU_CPU_HOST,             /* -cpu host (needs KVM) */
    QEMU_CPU_QEMU64,           /* x86_64 baseline */
    QEMU_CPU_NEHALEM,
    QEMU_CPU_SANDYBRIDGE,
    QEMU_CPU_IVYBRIDGE,
    QEMU_CPU_HASWELL,
    QEMU_CPU_BROADWELL,
    QEMU_CPU_SKYLAKE_CLIENT,
    QEMU_CPU_SKYLAKE_SERVER,
    QEMU_CPU_ICELAKE,
    QEMU_CPU_TIGERLAKE,
    QEMU_CPU_ALDERLAKE,
    QEMU_CPU_EPYC,
    QEMU_CPU_EPYC_ROME,
    QEMU_CPU_EPYC_MILAN,
    QEMU_CPU_CORTEX_A53,
    QEMU_CPU_CORTEX_A57,
    QEMU_CPU_CORTEX_A72,
    QEMU_CPU_NEHERMES,
    QEMU_CPU_RV64,
    QEMU_CPU_POWER9,
    QEMU_CPU_POWER10,
} QemuCpuModel;

/* Accelerator */
typedef enum {
    QEMU_ACCEL_TCG = 0,         /* Tiny Code Generator (default) */
    QEMU_ACCEL_KVM,             /* Linux KVM */
    QEMU_ACCEL_HAX,             /* Intel HAXM (Windows/macOS) */
    QEMU_ACCEL_HVF,             /* macOS Hypervisor.framework */
    QEMU_ACCEL_WHPX,            /* Windows Hypervisor Platform */
    QEMU_ACCEL_TCG_MT,          /* multi-thread TCG */
    QEMU_ACCEL_XEN,
} QemuAccel;

/* Disk bus */
typedef enum {
    QEMU_BUS_IDE = 0,
    QEMU_BUS_SATA,
    QEMU_BUS_SCSI,
    QEMU_BUS_NVME,
    QEMU_BUS_VIRTIO,
    QEMU_BUS_FLOPPY,
    QEMU_BUS_SD,
} QemuDiskBus;

/* Disk format */
typedef enum {
    QEMU_FMT_RAW = 0,
    QEMU_FMT_QCOW2,
    QEMU_FMT_QCOW3,
    QEMU_FMT_VMDK,
    QEMU_FMT_VDI,
    QEMU_FMT_VHDX,
    QEMU_FMT_VPC,
    QEMU_FMT_CLOOP,
    QEMU_FMT_ISO,
} QemuDiskFormat;

/* Network */
typedef enum {
    QEMU_NET_USER = 0,          /* -netdev user, SLIRP */
    QEMU_NET_TAP,               /* -netdev tap */
    QEMU_NET_BRIDGE,            /* -netdev bridge */
    QEMU_NET_SOCKET,            /* multicast/socket */
    QEMU_NET_VDE,
    QEMU_NET_VHOST_USER,
    QEMU_NET_NONE,
} QemuNicBackend;

/* NIC model */
typedef enum {
    QEMU_NIC_E1000 = 0,
    QEMU_NIC_E1000E,
    QEMU_NIC_VIRTIO,            /* virtio-net-pci */
    QEMU_NIC_RTL8139,
    QEMU_NIC_NE2K_PCI,
    QEMU_NIC_PCNET,
    QEMU_NIC_i82557B,
    QEMU_NIC_VMXNET3,
    QEMU_NIC_USB,
} QemuNicModel;

/* Display */
typedef enum {
    QEMU_DISPLAY_SDL = 0,
    QEMU_DISPLAY_GTK,
    QEMU_DISPLAY_VNC,
    QEMU_DISPLAY_SPICE,
    QEMU_DISPLAY_CURSES,
    QEMU_DISPLAY_COCOA,
    QEMU_DISPLAY_DBUS,
    QEMU_DISPLAY_NONE,
    QEMU_DISPLAY_DEFAULT,
} QemuDisplayType;

/* VGA card */
typedef enum {
    QEMU_VGA_STD = 0,           /* -vga std */
    QEMU_VGA_CIRRUS,
    QEMU_VGA_VMWARE,
    QEMU_VGA_QXL,
    QEMU_VGA_VIRTIO_VGA,       /* virtio-gpu-pci */
    QEMU_VGA_VIRTIO_GL,        /* virtio-gpu-gl-pci */
    QEMU_VGA_RAMDAC,
    QEMU_VGA_NONE,
} QemuVgaType;

/* Audio */
typedef enum {
    QEMU_AUDIO_NONE = 0,
    QEMU_AUDIO_SDL,
    QEMU_AUDIO_PULSE,
    QEMU_AUDIO_ALSA,
    QEMU_AUDIO_OSS,
    QEMU_AUDIO_COREAUDIO,
    QEMU_AUDIO_DSOUND,
    QEMU_AUDIO_WASAPI,
    QEMU_AUDIO_JACK,
    QEMU_AUDIO_PA,
    QEMU_AUDIO_SPICE,
} QemuAudioType;

/* Audio codec */
typedef enum {
    QEMU_AC97 = 0,
    QEMU_HDA,                  /* intel-hda + hda-duplex */
    QEMU_CS4231A,
    QEMU_ADLIB,
    QEMU_GUS,
    QEMU_SB16,
    QEMU_ES1370,
} QemuAudioCodec;

/* Character backend */
typedef enum {
    QEMU_CHR_STDIO = 0,
    QEMU_CHR_FILE,
    QEMU_CHR_PTY,
    QEMU_CHR_PIPE,
    QEMU_CHR_SOCKET,
    QEMU_CHR_UDP,
    QEMU_CHR_TCP,
    QEMU_CHR_SERIAL,
    QEMU_CHR_PARALLEL,
    QEMU_CHR_NULL,
} QemuChardevBackend;

/* Disk */
typedef struct {
    char path[QEMU_MAX_PATH];
    QemuDiskBus bus;
    QemuDiskFormat format;
    uint64_t size_bytes;       /* if creating new image */
    char device_id[64];
    int readonly;
    int snapshot;
    int if_none;               /* none bus for raw boot */
    int bootindex;
    char cache_mode[32];       /* none, writeback, writethrough, unsafe, directsync */
    char aio[16];              /* threads, native, io_uring */
    int discard;
    int detect_zeroes;
    int logical_block_size;
    int physical_block_size;
} QemuDisk;

/* NIC */
typedef struct {
    QemuNicBackend backend;
    QemuNicModel model;
    char mac[32];
    char id[64];
    int net_nr;
    char ifname[64];           /* tap interface */
    char brname[64];           /* bridge */
    char hostname[64];         /* user/slirp */
    char dns[64];
    char dhcp_start[64];
    char netmask[64];
    char hostfwd[256][128];    /* hostfwd rules */
    int hostfwd_count;
} QemuNic;

/* Chardev (serial, parallel, monitor) */
typedef struct {
    QemuChardevBackend backend;
    char id[64];
    char path[QEMU_MAX_PATH];
    int port;
    char host[64];
    int server;
    int nowait;
    int telnet;
    int nodelay;
} QemuChardev;

/* NUMA node */
typedef struct {
    int node_id;
    size_t mem_bytes;
    int cpus[QEMU_MAX_CPUS];
    int cpu_count;
    char initiator[32];
} QemuNumaNode;

/* Action on events */
typedef enum {
    QEMU_ON_EXIT_NONE = 0,
    QEMU_ON_EXIT_SHUTDOWN = 1,
    QEMU_ON_EXIT_PANIC,
    QEMU_ON_EXIT_REBOOT,
    QEMU_ON_EXIT_WATCHDOG,
} QemuExitAction;

typedef enum {
    QEMU_WDT_NONE = 0,
    QEMU_WDT_I6300ESB,
    QEMU_WDT_IB700,
    QEMU_WDT_DIAG288,
} QemuWatchdog;

/* Global state */
typedef struct {
    QemuTarget target;
    QemuMachineType machine;
    QemuCpuModel cpu_model;
    QemuAccel accelerator;
    int smp_cpus;
    int smp_sockets;
    int smp_cores;
    int smp_threads;
    int smp_dies;
    size_t mem_bytes;
    size_t mem_prealloc_bytes;
    int mem_prealloc;
    int mem_path_enabled;
    char mem_path[QEMU_MAX_PATH];
    int mem_merge_across_nodes;
    int mem_slots;
    int max_mem_size_mb;

    /* NUMA */
    QemuNumaNode numa[QEMU_MAX_NUMA];
    int numa_count;
    int numa_guest_distances[QEMU_MAX_NUMA][QEMU_MAX_NUMA];

    /* Firmware */
    char bios[QEMU_MAX_PATH];       /* -bios */
    char pflash[2][QEMU_MAX_PATH]; /* -drive if=pflash */
    char kernel_image[QEMU_MAX_PATH]; /* -kernel */
    char initrd[QEMU_MAX_PATH];     /* -initrd */
    char kernel_cmdline[4096];      /* -append */
    char dtb[QEMU_MAX_PATH];

    /* Disks */
    QemuDisk disks[QEMU_MAX_DISKS];
    int disk_count;
    int hdachs[3];                  /* hd_x,hd_y,hd_z */
    int boot_order_enabled;
    char boot_order[16];            /* e.g. "cad" */

    /* Network */
    QemuNic nics[QEMU_MAX_NICS];
    int nic_count;
    int net_none;

    /* Display & VGA */
    QemuDisplayType display;
    char display_opts[256];         /* e.g. VNC: "localhost:0" */
    QemuVgaType vga;
    int display_headless;           /* -nographic equivalent */
    int no_reboot;
    int no_shutdown;
    int daemonize;

    /* Audio */
    QemuAudioType audio_driver;
    QemuAudioCodec audio_codec;

    /* Serial/parallel/monitor/console */
    QemuChardev serials[QEMU_MAX_CHARS];
    int serial_count;
    QemuChardev parallels[QEMU_MAX_CHARS];
    int parallel_count;
    QemuChardev monitor_chr;
    QemuChardev qemu_chr;
    int monitor_telnet;
    int monitor_stdio;
    char console[128];               /* -serial stdio etc. shortcut */

    /* USB */
    int usb_enabled;
    int usb_kbd;
    int usb_mouse;
    int usb_tablet;
    int usb_wacom;
    int xhci;
    int uhci;

    /* Other devices */
    int virtio_balloon;
    int virtio_rng;
    int virtio_scsi;
    int virtio_9p;
    char virtio_9p_path[QEMU_MAX_PATH];
    char virtio_9p_mount_tag[64];
    int kvmclock;
    int hugepages;
    int smbios_type1;                /* enable SMBIOS passthrough */
    char smbios_vendor[128];
    char smbios_product[256];
    char smbios_serial[256];
    char smbios_uuid[64];

    /* Time / RTC */
    char rtc_base[32];               /* utc, localtime, or epoch */
    int rtc_clock_host;              /* -rtc clock=host */
    char timezone[64];

    /* Boot / snapshot */
    int enable_kvm;
    int snapshot_mode;
    int loadvm_enabled;
    char loadvm_tag[64];
    int incoming_enabled;
    char incoming[QEMU_MAX_PATH];    /* -incoming for migration */

    /* Exit handling */
    int watchdog_action;
    int reboot_action;
    int panic_action;
    int shutdown_action;

    /* Profiling / debug */
    int gdb_enabled;
    int gdb_port;
    int s;                          /* -s alias for -gdb tcp::1234 */
    int d;                          /* -d debug items */
    char debug_items[256];
    int trace_enabled;
    char trace_file[QEMU_MAX_PATH];
    int plugin_enabled;
    char plugin_lib[QEMU_MAX_PATH];

    /* Security */
    int sandbox_on;
    int seccomp;
    char runas_user[128];
    char chroot_dir[QEMU_MAX_PATH];

    /* Misc */
    char name[128];                  /* guest name in -name */
    char uuid[64];
    int pidfile_enabled;
    char pidfile[QEMU_MAX_PATH];
    int version;
    int help;
    int verbose;
    int dry_run;

    /* derived: path to qemu binary */
    char qemu_bin[QEMU_MAX_PATH];
    pid_t pid;
} QemuState;

/* API */
void qemu_init(QemuState *s);
int qemu_parse_arguments(QemuState *s, int argc, char **argv);
int qemu_resolve_binary(QemuState *s);
int qemu_build_command_line(QemuState *s, char **argv_out, int *out_argc);
const char *qemu_target_name(QemuTarget t);
const char *qemu_machine_name(QemuMachineType m);
const char *qemu_cpu_name(QemuCpuModel c);
const char *qemu_accel_name(QemuAccel a);
const char *qemu_bus_name(QemuDiskBus b);
const char *qemu_disk_format_name(QemuDiskFormat f);
const char *qemu_nic_model_name(QemuNicModel n);
const char *qemu_vga_name(QemuVgaType v);
int qemu_start(QemuState *s);
int qemu_wait(QemuState *s, int *exit_status);
int qemu_terminate(QemuState *s);
int qemu_create_disk_image(QemuState *s, const QemuDisk *d);
int qemu_commit_disk(const char *overlay, const char *backing);
int qemu_snapshot(const char *disk, const char *tag, int is_save);
void qemu_print_help(void);
void qemu_print_version(void);

/* KenuxK presets */
int qemu_apply_kenuxk_preset(QemuState *s);
int qemu_apply_windows_compat_preset(QemuState *s);
int qemu_apply_secure_boot_preset(QemuState *s);

#endif
