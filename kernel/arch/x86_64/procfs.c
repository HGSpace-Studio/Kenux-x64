

#include <arch/fs.h>
#include <arch/process.h>
#include <arch/memory.h>
#include <arch/smp.h>
#include <string.h>
#include <stdio.h>
#include <slab.h>
#include <kapi.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern uint64_t kenux_uptime(void);

static uint64_t __uptime_cached = 0;

typedef int (*procfs_read_fn_t)(char* buf, uint64_t max_len);

typedef struct procfs_entry {
    char name[FS_MAX_NAME];
    procfs_read_fn_t read_fn;
    uint64_t pid;
} procfs_entry_t;

static int procfs_cpuinfo_read(char* buf, uint64_t max_len)
{
    int n = 0;
    n += snprintf(buf + n, max_len - n,
        "processor\t: 0\n"
        "vendor_id\t: GenuineIntel\n"
        "cpu family\t: 6\n"
        "model\t\t: 158\n"
        "model name\t: Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz\n"
        "stepping\t: 13\n"
        "microcode\t: 0xb4\n"
        "cpu MHz\t\t: 3600.000\n"
        "cache size\t: 12288 KB\n"
        "physical id\t: 0\n"
        "siblings\t: 1\n"
        "core id\t\t: 0\n"
        "cpu cores\t: 1\n"
        "apicid\t\t: 0\n"
        "initial apicid\t: 0\n"
        "fpu\t\t: yes\n"
        "fpu_exception\t: yes\n"
        "cpuid level\t: 22\n"
        "wp\t\t: yes\n"
        "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss syscall nx pdpe1gb rdtscp lm constant_tsc arch_perfmon nopl xtopology tsc_reliable nonstop_tsc cpuid pni pclmulqdq ssse3 fma cx16 pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand hypervisor lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single ssbd ibrs ibpb stibp ibrs_enhanced fsgsbase tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx avx512f avx512dq rdseed adx smap clflushopt clwb avx512cd avx512bw avx512vl xsaveopt xsavec xsaves arat avx512_vnni md_clear flush_l1d arch_capabilities\n"
        "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds swapgs itlb_multihit mmio_stale_data retbleed gds\n"
        "bogomips\t: 7200.00\n"
        "clflush size\t: 64\n"
        "cache_alignment\t: 64\n"
        "address sizes\t: 45 bits physical, 48 bits virtual\n"
        "power management:\n");
    return n;
}

static int procfs_meminfo_read(char* buf, uint64_t max_len)
{
    uint64_t total = memory_get_total();
    uint64_t free = memory_get_free();
    uint64_t used = total - free;
    int n = 0;
    n += snprintf(buf + n, max_len - n,
        "MemTotal:       %lu kB\n"
        "MemFree:        %lu kB\n"
        "MemAvailable:   %lu kB\n"
        "Buffers:        0 kB\n"
        "Cached:         0 kB\n"
        "SwapCached:     0 kB\n"
        "Active:         %lu kB\n"
        "Inactive:       0 kB\n"
        "SwapTotal:      0 kB\n"
        "SwapFree:       0 kB\n"
        "Dirty:          0 kB\n"
        "Writeback:      0 kB\n"
        "AnonPages:      %lu kB\n"
        "Mapped:         0 kB\n"
        "Shmem:          0 kB\n"
        "Slab:           0 kB\n"
        "SReclaimable:   0 kB\n"
        "SUnreclaim:     0 kB\n",
        total / 1024, free / 1024, free / 1024,
        used / 1024, used / 1024);
    return n;
}

static int procfs_uptime_read(char* buf, uint64_t max_len)
{
    uint64_t uptime = kenux_uptime();
    __uptime_cached = uptime;
    return snprintf(buf, max_len, "%lu.%02lu %lu.%02lu\n",
        uptime / 100, (uptime % 100) * 10 / 100,
        uptime / 100, (uptime % 100) * 10 / 100);
}

static int procfs_version_read(char* buf, uint64_t max_len)
{
    return snprintf(buf, max_len,
        "Kenux Kernel version 26.7.9K (builder@kenux) (gcc version 16.1.0) #1 SMP %s\n",
        "2026-07-07");
}

static int procfs_stat_read(char* buf, uint64_t max_len)
{
    int n = 0;
    uint64_t user_time = 0;
    uint64_t sys_time = 0;
    uint64_t idle_time = 0;
    uint64_t irq_total = 0;
    uint32_t cpu_count = smp_cpu_count();
    if (cpu_count == 0) cpu_count = 1;
    for (uint32_t i = 0; i < cpu_count; i++) {
        cpu_info_t* cpu = smp_cpu_info(i);
        if (cpu) {
            user_time += cpu->sched_ticks;
            irq_total += cpu->irq_count;
            idle_time += cpu->idle_ticks;
        }
    }
    sys_time = irq_total;
    n += snprintf(buf + n, max_len - n, "cpu  %lu 0 %lu %lu 0 0 0 0 0 0\n",
                  user_time, sys_time, idle_time);
    for (uint32_t i = 0; i < cpu_count; i++) {
        cpu_info_t* cpu = smp_cpu_info(i);
        if (cpu) {
            n += snprintf(buf + n, max_len - n, "cpu%u %lu 0 %lu %lu 0 0 0 0 0 0\n",
                          i, cpu->sched_ticks, cpu->irq_count, cpu->idle_ticks);
        }
    }
    n += snprintf(buf + n, max_len - n, "intr %lu\n", irq_total);
    scheduler_stats_t* stats = scheduler_get_stats();
    n += snprintf(buf + n, max_len - n, "ctxt %lu\n",
                  stats ? stats->total_switches : (uint64_t)0);
    n += snprintf(buf + n, max_len - n, "btime %lu\n", __uptime_cached / 1000);
    uint64_t proc_count = 0;
    uint64_t running = 0;
    uint64_t blocked = 0;
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state != PROCESS_DEAD && processes[i].state != PROCESS_UNUSED) {
            proc_count++;
            if (processes[i].state == PROCESS_RUNNING) running++;
            if (processes[i].state == PROCESS_WAITING) blocked++;
        }
    }
    n += snprintf(buf + n, max_len - n, "processes %lu\n", proc_count);
    n += snprintf(buf + n, max_len - n, "procs_running %lu\n", running ? running : 1);
    n += snprintf(buf + n, max_len - n, "procs_blocked %lu\n", blocked);
    return n;
}

static int procfs_pid_status_read(char* buf, uint64_t max_len, uint64_t pid)
{
    process_t* proc = process_get(pid);
    if (!proc) return 0;

    const char* state_str = "unknown";
    switch (proc->state) {
        case PROCESS_RUNNING:  state_str = "R (running)"; break;
        case PROCESS_READY:    state_str = "S (sleeping)"; break;
        case PROCESS_WAITING:  state_str = "D (disk sleep)"; break;
        case PROCESS_ZOMBIE:   state_str = "Z (zombie)"; break;
        case PROCESS_DEAD:     state_str = "X (dead)"; break;
    }

    return snprintf(buf, max_len,
        "Name:\t%s\n"
        "State:\t%s\n"
        "Pid:\t%lu\n"
        "PPid:\t0\n"
        "TracerPid:\t0\n"
        "Uid:\t0\t0\t0\t0\n"
        "Gid:\t0\t0\t0\t0\n"
        "FDSize:\t256\n"
        "Groups:\t0 \n"
        "VmSize:\t%lu kB\n"
        "VmRSS:\t%lu kB\n",
        proc->name, state_str, pid,
        proc->mem_usage / 1024, proc->mem_usage / 1024);
}

static int procfs_pid_cmdline_read(char* buf, uint64_t max_len, uint64_t pid)
{
    process_t* proc = process_get(pid);
    if (!proc) return 0;
    uint64_t len = strlen(proc->name);
    if (len >= max_len) len = max_len - 1;
    memcpy(buf, proc->name, len);
    buf[len] = '\0';
    return (int)len;
}

static int procfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t size)
{
    procfs_entry_t* ent = (procfs_entry_t*)node->impl_data;
    if (!ent || !ent->read_fn) return 0;

    static char temp_buf[4096];
    int len;
    if (ent->pid) {

        if (strcmp(ent->name, "status") == 0) {
            len = procfs_pid_status_read(temp_buf, sizeof(temp_buf), ent->pid);
        } else if (strcmp(ent->name, "cmdline") == 0) {
            len = procfs_pid_cmdline_read(temp_buf, sizeof(temp_buf), ent->pid);
        } else {
            len = 0;
        }
    } else {
        len = ent->read_fn(temp_buf, sizeof(temp_buf));
    }

    if (offset >= (uint64_t)len) return 0;
    if (offset + size > (uint64_t)len) size = (uint64_t)len - offset;
    memcpy(buf, temp_buf + offset, size);
    return (int)size;
}

static int procfs_readdir(vfs_node_t* node, int index, char* name, uint64_t max_len)
{
    (void)node;
    int count = 0;

    const char* sys_files[] = { "cpuinfo", "meminfo", "uptime", "version", "stat" };
    int sys_count = sizeof(sys_files) / sizeof(sys_files[0]);

    if (index < sys_count) {
        uint64_t len = strlen(sys_files[index]);
        if (len >= max_len) len = max_len - 1;
        memcpy(name, sys_files[index], len);
        name[len] = '\0';
        return 0;
    }
    index -= sys_count;

    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state != PROCESS_DEAD) {
            if (count == index) {
                snprintf(name, max_len, "%lu", processes[i].id);
                return 0;
            }
            count++;
        }
    }
    return -1;
}

static vfs_node_t* procfs_finddir(vfs_node_t* node, const char* name)
{
    (void)node;

    procfs_entry_t* ent = (procfs_entry_t*)memory_zalloc(sizeof(procfs_entry_t));
    if (!ent) return NULL;

    strncpy(ent->name, name, FS_MAX_NAME - 1);
    ent->pid = 0;

    if (strcmp(name, "cpuinfo") == 0) ent->read_fn = procfs_cpuinfo_read;
    else if (strcmp(name, "meminfo") == 0) ent->read_fn = procfs_meminfo_read;
    else if (strcmp(name, "uptime") == 0) ent->read_fn = procfs_uptime_read;
    else if (strcmp(name, "version") == 0) ent->read_fn = procfs_version_read;
    else if (strcmp(name, "stat") == 0) ent->read_fn = procfs_stat_read;
    else {

        uint64_t pid = 0;
        for (const char* p = name; *p; p++) {
            if (*p < '0' || *p > '9') {
                kfree(ent);
            return NULL;
        }
        pid = pid * 10 + (*p - '0');
    }
    if (!process_get(pid)) {
        kfree(ent);
        return NULL;
    }
        ent->pid = pid;
        ent->read_fn = NULL;
    }

    vfs_node_t* child = vfs_create_node(name, ent->pid ? FS_TYPE_DIRECTORY : FS_TYPE_REGULAR);
    if (!child) {
        memory_free(ent);
        return NULL;
    }
    child->impl_data = ent;
    child->read = procfs_read;
    child->readdir = procfs_readdir;
    child->finddir = procfs_finddir;
    return child;
}

vfs_node_t* procfs_create(void)
{
    vfs_node_t* root = vfs_create_node("proc", FS_TYPE_DIRECTORY);
    if (!root) return NULL;

    root->read = procfs_read;
    root->readdir = procfs_readdir;
    root->finddir = procfs_finddir;

    return root;
}
