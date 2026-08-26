#ifndef _ARCH_KVM_H
#define _ARCH_KVM_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/kvm_vmx.h>

#define KVM_MAX_VCPUS           64
#define KVM_MAX_VMS             8
#define KVM_MEMORY_SLOTS        16
#define KVM_NR_MSRS             64
#define KVM_IO_REGION_MAX       8

#define VMCS_SIZE               4096
#define VMXON_REGION_SIZE       4096

#define EPT_PAGE_LEVELS          4
#define EPT_ENTRY_SIZE           8
#define EPT_PAGING_LEVELS         4
#define EPT_PGDIR_ENTRIES        512

#define EPT_PRESENT             (1ULL << 0)
#define EPT_WRITABLE            (1ULL << 1)
#define EPT_EXEC                (1ULL << 2)
#define EPT_IPAT                (1ULL << 6)
#define EPT_ACCESSED            (1ULL << 8)
#define EPT_DIRTY               (1ULL << 9)
#define EPT_LARGE_PAGE          (1ULL << 7)
#define EPT_TYPE_MASK           (3ULL << 3)
#define EPT_TYPE_UC             (0ULL << 3)
#define EPT_TYPE_WB             (6ULL << 3)

#define EXIT_REASON_MAX         80

typedef struct {
    u8 data[VMCS_SIZE];
} vmcs_t;

typedef struct kvm_vcpu kvm_vcpu_t;
typedef struct kvm_vm kvm_vm_t;

struct kvm_vcpu {
    u32 vcpu_id;
    kvm_vm_t *kvm;
    vmcs_t *vmcs;
    u64 vmcs_pa;
    struct kvm_run *run;
    u64 run_pa;
    u64 host_rsp;
    u64 host_rip;
    u64 regs[16];
    u64 rip;
    u64 rsp;
    u64 rflags;
    u64 cr0;
    u64 cr2;
    u64 cr3;
    u64 cr4;
    u64 dr6;
    u64 dr7;
    u64 efer;
    u64 pdptrs[4];
    u64 guest_pat;
    u64 host_cr3;
    u64 host_rsp_save;
    u64 host_rip_save;
    u64 msr_host_kernel_gs_base;
    u64 vpid;
    u32 exit_reason;
    u32 exit_qual;
    u64 guest_physical_address;
    u64 exit_int_info;
    u64 exit_int_error;
    u32 idt_vectoring_info;
    u32 idt_vectoring_error;
    u32 instruction_length;
    int launched;
    int active;
    int mp_state;
    spinlock_t lock;
};

struct kvm_run {
    u32 exit_reason;
    u32 ready_for_interrupt_injection;
    u8 padding[8];
    union {
        struct {
            u64 rip;
            u64 dr6;
            u64 dr7;
            u32 exception;
            u32 error_code;
        } ex;
        struct {
            u64 rip;
            u64 qualification;
            u64 guest_linear;
            u64 guest_physical;
        } ept_violation;
        struct {
            u8 direction;
            u8 size;
            u16 port;
            u32 count;
            u64 data_offset;
        } io;
        struct {
            u32 msr;
            u64 data;
            u8 write;
        } msr;
        struct {
            u64 rip;
            u64 cr;
            u64 qualification;
            u32 cr_num;
            u32 old_val;
            u32 new_val;
        } cr;
        struct {
            u64 gpa;
        } mmio;
    };
};

struct kvm_vm {
    u32 vm_id;
    u32 nr_vcpus;
    kvm_vcpu_t *vcpus[KVM_MAX_VCPUS];
    u64 ept_root_pa;
    void *ept_root;
    u64 ept_root_phys;
    void *vmxon_region;
    u64 vmxon_pa;
    u64 tsc_offset;
    u32 memslots;
    spinlock_t lock;
    int active;
    kvm_vm_t* next_vm;
};

typedef struct {
    u64 gpa;
    u64 size;
    void *hva;
    u64 npages;
} kvm_memory_slot_t;

int kvm_init(void);
int kvm_vm_create(kvm_vm_t *vm);
void kvm_vm_destroy(kvm_vm_t *vm);
int kvm_vcpu_create(kvm_vm_t *vm, u32 vcpu_id, kvm_vcpu_t *vcpu);
void kvm_vcpu_destroy(kvm_vcpu_t *vcpu);
int kvm_vcpu_run(kvm_vcpu_t *vcpu);
int kvm_vcpu_setup(kvm_vcpu_t *vcpu);

int vmx_init(void);
int vmx_vcpu_create(kvm_vcpu_t *vcpu);
int vmx_vcpu_run(kvm_vcpu_t *vcpu);
void vmx_exit_handler(kvm_vcpu_t *vcpu);

int ept_init(kvm_vm_t *vm);
int ept_map_page(kvm_vm_t *vm, u64 gpa, u64 hpa, u64 flags);
int ept_unmap_page(kvm_vm_t *vm, u64 gpa);
void *ept_alloc_page(void);
void ept_free_page(void *page);

void vmcs_load(kvm_vcpu_t *vcpu);
void vmx_handle_exit(kvm_vcpu_t *vcpu);
void vmx_handle_cpuid(kvm_vcpu_t *vcpu);
void vmx_handle_io(kvm_vcpu_t *vcpu);
void vmx_handle_ept_violation(kvm_vcpu_t *vcpu);
void vmx_handle_cr_access(kvm_vcpu_t *vcpu);
void vmx_handle_msr(kvm_vcpu_t *vcpu);
void vmx_handle_ext_int(kvm_vcpu_t *vcpu);
void vmx_handle_invalid(kvm_vcpu_t *vcpu);

extern vmx_capability_t vmx_cap;
extern kvm_vm_t *kvm_vm_list;

#endif
