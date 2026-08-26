#include "kapi.h"

/* Debug: output a character to COM1 */
#define SERIAL_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)

extern int kapi_process_init(void);
extern int kapi_memory_init(void);
extern int kapi_fs_init(void);
extern int kapi_device_init(void);
extern int kapi_syscall_init(void);
extern int kapi_crc_init(void);
extern int kapi_irq_init(void);
extern int kapi_io_init(void);
extern int kapi_pci_init(void);
extern int kapi_dma_init(void);
extern int kapi_security_init(void);
extern int kapi_skbuff_init(void);
extern int kapi_netdevice_init(void);
extern int kapi_socket_init(void);
extern int kapi_netlink_init(void);
extern int kapi_vfs_init(void);
extern int kapi_seq_file_init(void);
extern int kapi_debugfs_init(void);
extern int kapi_mempool_init(void);
extern int kapi_ftrace_init(void);
extern int kapi_ebpf_init(void);
extern int kapi_kprobe_init(void);

/* System utilities */
extern int kapi_device_manager_init(void);
extern int kapi_memory_ext_init(void);
extern int kapi_trace_init(void);
extern int kapi_logging_init(void);
extern int kapi_memleak_init(void);
extern int kapi_profiler_init(void);
extern int kapi_graphics2d_init(void);
extern int kapi_input_init(void);
extern int kapi_window_init(void);

/* Advanced OS subsystems (skeleton) */
extern int kapi_crypto_fs_init(void);
extern int kapi_lvm_init(void);
extern int kapi_raid_init(void);
extern int kapi_wifi_init(void);
extern int kapi_vpn_init(void);
extern int kapi_netstat_init(void);
extern int kapi_cpufreq_init(void);
extern int kapi_ns_init(void);
extern int kapi_container_init(void);
extern int kapi_fde_init(void);
extern int kapi_iommu_init(void);

/* POSIX I/O multiplexing and event mechanisms */
extern int kapi_epoll_init(void);
extern int kapi_poll_init(void);
extern int kapi_signalfd_init(void);
extern int kapi_timerfd_init(void);
extern int kapi_eventfd_init(void);
extern int kapi_inotify_init_module(void);

int kapi_init(void)
{
    int ret;

    SERIAL_DBG('1');
    ret = kapi_process_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('2');
    ret = kapi_memory_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('3');
    ret = kapi_fs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('4');
    ret = kapi_device_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('5');
    ret = kapi_syscall_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('6');
    ret = kapi_crc_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('7');
    ret = kapi_irq_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('8');
    ret = kapi_io_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('9');
    ret = kapi_pci_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('A');
    ret = kapi_dma_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('B');
    ret = kapi_security_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('C');
    ret = kapi_skbuff_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('D');
    ret = kapi_netdevice_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('E');
    ret = kapi_socket_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('F');
    ret = kapi_netlink_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('G');
    ret = kapi_vfs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('H');
    ret = kapi_seq_file_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('I');
    ret = kapi_debugfs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('J');
    ret = kapi_mempool_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('K');
    ret = kapi_ftrace_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('L');
    ret = kapi_ebpf_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('M');
    ret = kapi_kprobe_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    /* System utilities */
    SERIAL_DBG('N');
    ret = kapi_device_manager_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('O');
    ret = kapi_memory_ext_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('P');
    ret = kapi_trace_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('Q');
    ret = kapi_logging_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('R');
    ret = kapi_memleak_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('S');
    ret = kapi_profiler_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('T');
    ret = kapi_graphics2d_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('U');
    ret = kapi_input_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('V');
    ret = kapi_window_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    /* Advanced OS subsystems (skeleton, best-effort) */
    SERIAL_DBG('a');
    (void)kapi_crypto_fs_init();
    SERIAL_DBG('b');
    (void)kapi_lvm_init();
    SERIAL_DBG('c');
    (void)kapi_raid_init();
    SERIAL_DBG('d');
    (void)kapi_wireless_init();
    SERIAL_DBG('e');
    (void)kapi_vpn_init();
    SERIAL_DBG('f');
    (void)kapi_netstat_init();
    SERIAL_DBG('g');
    (void)kapi_cpufreq_init();
    SERIAL_DBG('h');
    (void)kapi_ns_init();
    SERIAL_DBG('i');
    (void)kapi_container_init();
    SERIAL_DBG('j');
    (void)kapi_fde_init();
    SERIAL_DBG('k');
    (void)kapi_iommu_init();

    /* POSIX I/O multiplexing and event mechanisms */
    SERIAL_DBG('l');
    ret = kapi_epoll_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('m');
    ret = kapi_poll_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('n');
    ret = kapi_signalfd_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('o');
    ret = kapi_timerfd_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('p');
    ret = kapi_eventfd_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('q');
    ret = kapi_inotify_init_module();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('Z');
    return KAPI_OK;
}