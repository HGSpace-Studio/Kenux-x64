#ifndef KAPI_VIRT_EXT_H
#define KAPI_VIRT_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_VIRT_MAX_NAME        64
#define KAPI_VIRT_MAX_PATH        256
#define KAPI_VIRT_MAX_CONFIG      4096
#define KAPI_VIRT_MAX_ARGS        128

#define KAPI_VM_STATE_CREATED     0
#define KAPI_VM_STATE_RUNNING     1
#define KAPI_VM_STATE_PAUSED      2
#define KAPI_VM_STATE_SHUTDOWN    3
#define KAPI_VM_STATE_CRASHED     4
#define KAPI_VM_STATE_SUSPENDED   5

#define KAPI_VCPU_STATE_OFFLINE   0
#define KAPI_VCPU_STATE_RUNNING   1
#define KAPI_VCPU_STATE_BLOCKED   2
#define KAPI_VCPU_STATE_READY     3

#define KAPI_MEM_TYPE_RAM         0
#define KAPI_MEM_TYPE_ROM         1
#define KAPI_MEM_TYPE_MMIO        2
#define KAPI_MEM_TYPE_DEVICE      3

#define KAPI_DEV_TYPE_PCI         0
#define KAPI_DEV_TYPE_USB         1
#define KAPI_DEV_TYPE_SERIAL      2
#define KAPI_DEV_TYPE_PARALLEL    3
#define KAPI_DEV_TYPE_NETWORK     4
#define KAPI_DEV_TYPE_STORAGE     5
#define KAPI_DEV_TYPE_DISPLAY     6
#define KAPI_DEV_TYPE_INPUT       7
#define KAPI_DEV_TYPE_AUDIO       8
#define KAPI_DEV_TYPE_WATCHDOG    9
#define KAPI_DEV_TYPE_RNG         10
#define KAPI_DEV_TYPE_BALLOON     11
#define KAPI_DEV_TYPE_9P          12
#define KAPI_DEV_TYPE_VIRTIO      13

#define KAPI_NET_MODE_NONE        0
#define KAPI_NET_MODE_USER        1
#define KAPI_NET_MODE_BRIDGE      2
#define KAPI_NET_MODE_TAP         3
#define KAPI_NET_MODE_SOCKET      4
#define KAPI_NET_MODE_PASSTHROUGH 5

#define KAPI_DISK_FORMAT_RAW      0
#define KAPI_DISK_FORMAT_QCOW2    1
#define KAPI_DISK_FORMAT_VDI      2
#define KAPI_DISK_FORMAT_VMDK     3
#define KAPI_DISK_FORMAT_VHD      4

typedef uint32_t kapi_vm_id_t;
typedef uint32_t kapi_vcpu_id_t;
typedef uint64_t kapi_gpa_t;

typedef struct {
    char name[KAPI_VIRT_MAX_NAME];
    char config_file[KAPI_VIRT_MAX_PATH];
    int state;
    uint64_t memory_size;
    int vcpu_count;
    uint64_t uptime;
    uint64_t cpu_time;
    uint64_t mem_used;
    pid_t pid;
    char kernel[KAPI_VIRT_MAX_PATH];
    char initrd[KAPI_VIRT_MAX_PATH];
    char cmdline[1024];
} kapi_vm_info_t;

typedef struct {
    kapi_vcpu_id_t id;
    int vm_id;
    int state;
    int cpu_pin;
    uint64_t cpu_time;
    uint64_t halt_time;
    uint64_t exit_count;
    uint64_t io_count;
    uint64_t irq_count;
    bool is_running;
    bool is_bsp;
} kapi_vcpu_info_t;

typedef struct {
    kapi_gpa_t base_addr;
    size_t size;
    int type;
    bool is_mapped;
    bool is_readable;
    bool is_writable;
    bool is_executable;
    bool is_cached;
    void* host_addr;
} kapi_mem_region_t;

typedef struct {
    int type;
    union {
        struct {
            char ifname[16];
            char bridge[16];
            char tap[16];
            int mode;
            uint8_t mac[6];
            bool vhost;
            bool is_up;
        } net;
        struct {
            char path[KAPI_VIRT_MAX_PATH];
            int format;
            bool readonly;
            bool is_cdrom;
            bool is_floppy;
            uint64_t size;
        } disk;
        struct {
            int vendor_id;
            int device_id;
            bool passthrough;
            int bus;
            int slot;
            int func;
        } pci;
        struct {
            char serial_path[KAPI_VIRT_MAX_PATH];
            int mode;
            int baud_rate;
        } serial;
        struct {
            uint8_t type;
            bool enabled;
            uint32_t resolution_x;
            uint32_t resolution_y;
            uint32_t bpp;
        } display;
        struct {
            char input_type[32];
            bool absolute;
        } input;
    } u;
} kapi_device_config_t;

typedef struct {
    kapi_vm_id_t id;
    char name[KAPI_VIRT_MAX_NAME];
    uint64_t memory_size;
    int vcpu_count;
    char kernel[KAPI_VIRT_MAX_PATH];
    char initrd[KAPI_VIRT_MAX_PATH];
    char cmdline[1024];
    int device_count;
    kapi_device_config_t devices[32];
    bool enable_kvm;
    bool enable_nested_virt;
    bool enable_debugger;
    bool enable_trace;
    char log_path[KAPI_VIRT_MAX_PATH];
    char save_path[KAPI_VIRT_MAX_PATH];
    char snapshot_dir[KAPI_VIRT_MAX_PATH];
} kapi_vm_config_t;

typedef struct {
    kapi_vm_id_t vm_id;
    kapi_vcpu_id_t vcpu_id;
    uint64_t reason;
    uint64_t error_code;
    union {
        struct {
            uint64_t rip;
            uint64_t rax;
            uint64_t rcx;
            uint64_t rdx;
            uint64_t rbx;
            uint64_t rsp;
            uint64_t rbp;
            uint64_t rsi;
            uint64_t rdi;
            uint64_t r8;
            uint64_t r9;
            uint64_t r10;
            uint64_t r11;
            uint64_t r12;
            uint64_t r13;
            uint64_t r14;
            uint64_t r15;
            uint64_t rflags;
            uint64_t cs;
            uint64_t ds;
            uint64_t es;
            uint64_t fs;
            uint64_t gs;
            uint64_t ss;
            uint64_t cr0;
            uint64_t cr2;
            uint64_t cr3;
            uint64_t cr4;
            uint64_t cr8;
            uint64_t dr0;
            uint64_t dr1;
            uint64_t dr2;
            uint64_t dr3;
            uint64_t dr6;
            uint64_t dr7;
            uint64_t efer;
        } regs;
        struct {
            uint64_t gpa;
            uint64_t gva;
            uint64_t error_code;
            bool is_write;
            bool is_present;
            bool is_user;
            bool is_fetch;
        } mmio;
        struct {
            uint32_t vector;
            uint32_t err_code;
            bool injected;
            bool pending;
        } irq;
    } u;
} kapi_exit_info_t;

typedef void (*kapi_vm_exit_handler_t)(const kapi_exit_info_t* info, void* user_data);

kapi_vm_id_t kapi_vm_create(const kapi_vm_config_t* config);

int kapi_vm_destroy(kapi_vm_id_t vm);

int kapi_vm_start(kapi_vm_id_t vm);

int kapi_vm_pause(kapi_vm_id_t vm);

int kapi_vm_resume(kapi_vm_id_t vm);

int kapi_vm_shutdown(kapi_vm_id_t vm, bool force);

int kapi_vm_reset(kapi_vm_id_t vm);

int kapi_vm_save(kapi_vm_id_t vm, const char* path);

int kapi_vm_restore(kapi_vm_id_t vm, const char* path);

int kapi_vm_snapshot(kapi_vm_id_t vm, const char* name);

int kapi_vm_revert_snapshot(kapi_vm_id_t vm, const char* name);

int kapi_vm_delete_snapshot(kapi_vm_id_t vm, const char* name);

int kapi_vm_list_snapshots(kapi_vm_id_t vm, char** names, int max_names);

int kapi_vm_get_info(kapi_vm_id_t vm, kapi_vm_info_t* info);

int kapi_vm_set_memory(kapi_vm_id_t vm, uint64_t size);

uint64_t kapi_vm_get_memory_usage(kapi_vm_id_t vm);

int kapi_vm_add_device(kapi_vm_id_t vm, const kapi_device_config_t* dev);

int kapi_vm_remove_device(kapi_vm_id_t vm, int dev_index);

int kapi_vm_set_kernel(kapi_vm_id_t vm, const char* kernel_path, const char* cmdline);

int kapi_vm_set_initrd(kapi_vm_id_t vm, const char* initrd_path);

int kapi_vm_register_exit_handler(kapi_vm_id_t vm, kapi_vm_exit_handler_t handler, void* data);

int kapi_vm_unregister_exit_handler(kapi_vm_id_t vm);

kapi_vcpu_id_t kapi_vcpu_create(kapi_vm_id_t vm, int cpu_pin);

int kapi_vcpu_destroy(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu);

int kapi_vcpu_run(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu);

int kapi_vcpu_pause(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu);

int kapi_vcpu_resume(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu);

int kapi_vcpu_get_info(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu, kapi_vcpu_info_t* info);

int kapi_vcpu_get_regs(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu, kapi_exit_info_t* info);

int kapi_vcpu_set_regs(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu, const kapi_exit_info_t* info);

int kapi_vcpu_inject_irq(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu, uint32_t vector);

int kapi_vcpu_inject_exception(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu,
                               uint32_t vector, uint32_t error_code, bool has_error);

int kapi_vcpu_set_cpuid(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu,
                        uint32_t function, uint32_t index,
                        uint32_t *eax, uint32_t *ebx,
                        uint32_t *ecx, uint32_t *edx);

int kapi_vcpu_get_cpuid(kapi_vm_id_t vm, kapi_vcpu_id_t vcpu,
                        uint32_t function, uint32_t index,
                        uint32_t *eax, uint32_t *ebx,
                        uint32_t *ecx, uint32_t *edx);

int kapi_mem_map(kapi_vm_id_t vm, kapi_gpa_t gpa, void* hva, size_t size, int prot);

int kapi_mem_unmap(kapi_vm_id_t vm, kapi_gpa_t gpa, size_t size);

int kapi_mem_protect(kapi_vm_id_t vm, kapi_gpa_t gpa, size_t size, int prot);

void* kapi_mem_get_host_addr(kapi_vm_id_t vm, kapi_gpa_t gpa);

kapi_gpa_t kapi_mem_get_guest_addr(kapi_vm_id_t vm, void* hva);

int kapi_mem_read(kapi_vm_id_t vm, kapi_gpa_t gpa, void* buf, size_t size);

int kapi_mem_write(kapi_vm_id_t vm, kapi_gpa_t gpa, const void* buf, size_t size);

int kapi_mem_read_phys(uint64_t paddr, void* buf, size_t size);

int kapi_mem_write_phys(uint64_t paddr, const void* buf, size_t size);

int kapi_io_create_port(kapi_vm_id_t vm, uint16_t port, bool is_write,
                        void (*handler)(kapi_vm_id_t vm, uint16_t port,
                                       void* data, int len, void* user),
                        void* user_data);

int kapi_io_destroy_port(kapi_vm_id_t vm, uint16_t port);

int kapi_io_inb(kapi_vm_id_t vm, uint16_t port, uint8_t* value);

int kapi_io_inw(kapi_vm_id_t vm, uint16_t port, uint16_t* value);

int kapi_io_inl(kapi_vm_id_t vm, uint16_t port, uint32_t* value);

int kapi_io_outb(kapi_vm_id_t vm, uint16_t port, uint8_t value);

int kapi_io_outw(kapi_vm_id_t vm, uint16_t port, uint16_t value);

int kapi_io_outl(kapi_vm_id_t vm, uint16_t port, uint32_t value);

int kapi_msi_route(kapi_vm_id_t vm, int gsi, int virq);

int kapi_msi_dequeue(kapi_vm_id_t vm, int gsi);

int kapi_container_create(const char* name, const char* config);

int kapi_container_start(const char* name);

int kapi_container_stop(const char* name);

int kapi_container_destroy(const char* name);

int kapi_container_exec(const char* name, const char* command);

int kapi_container_attach(const char* name, int pid);

int kapi_container_list(char** names, int max_names);

bool kapi_is_in_container(void);

const char* kapi_get_container_name(void);

#ifdef __cplusplus
}
#endif

#endif