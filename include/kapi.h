#ifndef KAPI_H
#define KAPI_H

#define KAPI_VERSION_MAJOR 2
#define KAPI_VERSION_MINOR 0
#define KAPI_VERSION_PATCH 0

#include <stdint.h>
#include <stddef.h>

#define KAPI_OK          0
#define KAPI_ERROR      -1
#define KAPI_EINVAL     -2
#define KAPI_ENOMEM     -3
#define KAPI_ENOENT     -4
#define KAPI_EACCES     -5
#define KAPI_EBUSY      -6
#define KAPI_ENOSYS     -7
#define KAPI_EAGAIN     -8

#include "kapi_list.h"
#include "kapi_atomic.h"
#include "kapi_bitmap.h"
#include "kapi_kfifo.h"
#include "kapi_idr.h"
#include "kapi_rbtree.h"
#include "kapi_hash.h"
#include "kapi_sort.h"
#include "kapi_crc.h"

#include "kapi_mutex.h"
#include "kapi_completion.h"
#include "kapi_wait.h"
#include "kapi_rcu.h"

#include "kapi_string.h"
#include "kapi_time.h"
#include "kapi_random.h"

#include "kapi_irq.h"
#include "kapi_kthread.h"
#include "kapi_notifier.h"
#include "kapi_sched.h"
#include "kapi_smp.h"
#include "kapi_cpumask.h"
#include "kapi_module.h"
#include "kapi_params.h"

#include "kapi_io.h"
#include "kapi_pci.h"
#include "kapi_dma.h"
#include "kapi_cdev.h"
#include "kapi_blkdev.h"
#include "kapi_security.h"

#include "kapi_skbuff.h"
#include "kapi_netdevice.h"
#include "kapi_socket.h"
#include "kapi_netlink.h"

#include "kapi_vfs.h"
#include "kapi_seq_file.h"
#include "kapi_debugfs.h"
#include "kapi_kobject.h"
#include "kapi_mempool.h"
#include "kapi_ftrace.h"
#include "kapi_ebpf.h"
#include "kapi_kprobe.h"
#include "kapi_leonos.h"
#include "kapi_leonos_ext.h"

/* System utilities */
#include "kapi_device_manager.h"
#include "kapi_memory_ext.h"
#include "kapi_trace.h"
#include "kapi_logging.h"
#include "kapi_memleak.h"
#include "kapi_profiler.h"
#include "kapi_graphics2d.h"
#include "kapi_input.h"
#include "kapi_window.h"

/* Advanced OS subsystems (skeleton) */
#include "kapi_crypto_fs.h"
#include "kapi_lvm.h"
#include "kapi_raid.h"
#include "kapi_wireless.h"
#include "kapi_vpn.h"
#include "kapi_netstat.h"
#include "kapi_cpufreq.h"
#include "kapi_namespace.h"
#include "kapi_container.h"
#include "kapi_full_disk_encryption.h"
#include "kapi_iommu.h"

/* Enhanced API modules */
#include "kapi_process.h"
#include "kapi_memory.h"
#include "kapi_fs_ext.h"
#include "kapi_net_ext.h"
#include "kapi_device_ext.h"
#include "kapi_sync_ext.h"
#include "kapi_sysinfo.h"
#include "kapi_security_ext.h"
#include "kapi_virt_ext.h"

static inline void kapi_get_version(int* major, int* minor, int* patch)
{
    if (major) *major = KAPI_VERSION_MAJOR;
    if (minor) *minor = KAPI_VERSION_MINOR;
    if (patch) *patch = KAPI_VERSION_PATCH;
}

int kapi_init(void);

#endif