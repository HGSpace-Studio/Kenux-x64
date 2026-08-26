#include <arch/kvm.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/kvm_vmx.h>
#include <arch/memory.h>
#include <string.h>
#include <slab.h>

#define VMCS_EXIT_QUALIFICATION 0x6400

vmx_capability_t vmx_cap;
kvm_vm_t *kvm_vm_list = NULL;
static u32 kvm_next_vm_id = 1;
static spinlock_t kvm_global_lock = SPINLOCK_INIT;

static u64 rdmsr_safe(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static void wrmsr_safe(u32 msr, u64 val)
{
    u32 lo = (u32)(val & 0xFFFFFFFF);
    u32 hi = (u32)(val >> 32);
    __asm__ volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

static void *alloc_page_aligned(u64 size)
{
    void *p = kzalloc(size);
    return p;
}

static u64 get_physical_addr(void *vaddr)
{
    void *pml4 = pmap_get();
    if (!pml4) return (u64)vaddr;
    return pmap_get_physical(pml4, (u64)vaddr);
}

static void detect_vmx_capabilities(void)
{
    u64 feature_ctrl = rdmsr_safe(0x3A);
    if (!(feature_ctrl & (1 << 2))) {
        vmx_cap.vmx_supported = 0;
        return;
    }

    u64 cpuid_1_ecx;
    __asm__ volatile ("cpuid" : "=c"(cpuid_1_ecx) : "a"(1) : "rbx", "rdx");
    if (!(cpuid_1_ecx & (1 << 5))) {
        vmx_cap.vmx_supported = 0;
        return;
    }

    vmx_cap.vmx_supported = 1;
    vmx_cap.vmx_basic = rdmsr_safe(VMX_BASIC_MSR);
    vmx_cap.ept_vpid_cap = rdmsr_safe(VMX_EPT_VPID_CAP_MSR);
    vmx_cap.pinbased_ctls = rdmsr_safe(VMX_TRUE_PINBASED_CTLS);
    vmx_cap.procbased_ctls = rdmsr_safe(VMX_TRUE_PROCBASED_CTLS);
    vmx_cap.exit_ctls = rdmsr_safe(VMX_TRUE_EXIT_CTLS);
    vmx_cap.entry_ctls = rdmsr_safe(VMX_TRUE_ENTRY_CTLS);
    vmx_cap.cr0_fixed0 = rdmsr_safe(VMX_CR0_FIXED0);
    vmx_cap.cr0_fixed1 = rdmsr_safe(VMX_CR0_FIXED1);
    vmx_cap.cr4_fixed0 = rdmsr_safe(VMX_CR4_FIXED0);
    vmx_cap.cr4_fixed1 = rdmsr_safe(VMX_CR4_FIXED1);

    vmx_cap.ept_supported = !!(vmx_cap.ept_vpid_cap & (1 << 1));
    vmx_cap.vpid_supported = !!(vmx_cap.ept_vpid_cap & (1 << 0));
}

int vmx_init(void)
{
    memset(&vmx_cap, 0, sizeof(vmx_cap));
    detect_vmx_capabilities();

    if (!vmx_cap.vmx_supported) return -1;

    u64 cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 13);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

    void *vmxon_region = alloc_page_aligned(VMXON_REGION_SIZE);
    if (!vmxon_region) return -1;
    memset(vmxon_region, 0, VMXON_REGION_SIZE);

    u64 vmxon_pa = get_physical_addr(vmxon_region);
    u64 vmxon_revision = vmx_cap.vmx_basic & 0x7FFFFFFFFFFFFFFFULL;
    *(u64 *)vmxon_region = vmxon_revision;

    u8 err = vmxon(vmxon_pa);
    if (err) return -1;

    return 0;
}

void vmcs_load(kvm_vcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmcs) return;
    u64 vmcs_pa = vcpu->vmcs_pa;
    vmclear(vmcs_pa);
    vmptrld(vmcs_pa);
}

static void vmcs_init_default(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;

    u64 pin_ctls = vmx_cap.pinbased_ctls;
    u64 proc_ctls = vmx_cap.procbased_ctls;
    u64 exit_ctls = vmx_cap.exit_ctls;
    u64 entry_ctls = vmx_cap.entry_ctls;

    pin_ctls &= 0xFFFFFFFF;
    pin_ctls |= (pin_ctls >> 32);
    proc_ctls &= 0xFFFFFFFF;
    proc_ctls |= (proc_ctls >> 32);
    exit_ctls &= 0xFFFFFFFF;
    exit_ctls |= (exit_ctls >> 32);
    entry_ctls &= 0xFFFFFFFF;
    entry_ctls |= (entry_ctls >> 32);

    pin_ctls |= VMX_CPU_BASED_HLT_EXITING;
    pin_ctls |= VMX_CPU_BASED_NMI_WINDOW;

    proc_ctls |= VMX_CPU_BASED_HLT_EXITING;
    proc_ctls |= VMX_CPU_BASED_INVLPG_EXITING;
    proc_ctls |= VMX_CPU_BASED_MWAIT_EXITING;
    proc_ctls |= VMX_CPU_BASED_RDPMC_EXITING;
    proc_ctls |= VMX_CPU_BASED_RDTSC_EXITING;
    proc_ctls |= VMX_CPU_BASED_MOV_DR_EXITING;
    proc_ctls |= VMX_CPU_BASED_UNCOND_IO_EXITING;
    proc_ctls |= VMX_CPU_BASED_USE_IO_BITMAPS;
    proc_ctls |= VMX_CPU_BASED_MONITOR_EXITING;
    proc_ctls |= VMX_CPU_BASED_PAUSE_EXITING;

    if (vmx_cap.ept_supported) {
        proc_ctls |= VMX_CPU_BASED_ACTIVATE_SECONDARY;
    }

    exit_ctls |= VMX_VM_EXIT_IA32E_MODE;
    exit_ctls |= VMX_VM_EXIT_ACK_INTR_ON_EXIT;

    entry_ctls |= VMX_VM_ENTRY_IA32E_MODE;

    vmwrite(VMCS_PIN_BASED_VM_EXEC_CONTROLS, pin_ctls);
    vmwrite(VMCS_CPU_BASED_VM_EXEC_CONTROLS, proc_ctls);
    vmwrite(VMCS_EXIT_CONTROLS, exit_ctls);
    vmwrite(VMCS_ENTRY_CONTROLS, entry_ctls);

    vmwrite(VMCS_HOST_CR0, vmx_cap.cr0_fixed1);
    vmwrite(VMCS_HOST_CR3, vcpu->host_cr3);
    vmwrite(VMCS_HOST_CR4, vmx_cap.cr4_fixed1);

    vmwrite(VMCS_HOST_CS_SEL, 0x08);
    vmwrite(VMCS_HOST_SS_SEL, 0x10);
    vmwrite(VMCS_HOST_DS_SEL, 0x18);
    vmwrite(VMCS_HOST_ES_SEL, 0x18);
    vmwrite(VMCS_HOST_FS_SEL, 0x20);
    vmwrite(VMCS_HOST_GS_SEL, 0x28);
    vmwrite(VMCS_HOST_TR_SEL, 0x30);

    vmwrite(VMCS_GUEST_CR0, vmx_cap.cr0_fixed1 & 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_CR3, vcpu->cr3);
    vmwrite(VMCS_GUEST_CR4, vmx_cap.cr4_fixed1 & 0xFFFFFFFF);

    vmwrite(VMCS_GUEST_CS_SEL, 0x08);
    vmwrite(VMCS_GUEST_SS_SEL, 0x10);
    vmwrite(VMCS_GUEST_DS_SEL, 0x18);
    vmwrite(VMCS_GUEST_ES_SEL, 0x18);
    vmwrite(VMCS_GUEST_FS_SEL, 0x20);
    vmwrite(VMCS_GUEST_GS_SEL, 0x28);

    vmwrite(VMCS_GUEST_EFER, 0);
    vmwrite(VMCS_GUEST_RIP, vcpu->rip);
    vmwrite(VMCS_GUEST_RSP, vcpu->rsp);
    vmwrite(VMCS_GUEST_RFLAGS, 0x2);

    vmwrite(VMCS_GUEST_CS_AR_BYTES, 0xA09B);
    vmwrite(VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_CS_BASE, 0);

    vmwrite(VMCS_GUEST_SS_AR_BYTES, 0xC093);
    vmwrite(VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_SS_BASE, 0);

    vmwrite(VMCS_GUEST_DS_AR_BYTES, 0xC093);
    vmwrite(VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_DS_BASE, 0);

    vmwrite(VMCS_GUEST_ES_AR_BYTES, 0xC093);
    vmwrite(VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_ES_BASE, 0);

    vmwrite(VMCS_GUEST_FS_AR_BYTES, 0xC093);
    vmwrite(VMCS_GUEST_FS_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_FS_BASE, 0);

    vmwrite(VMCS_GUEST_GS_AR_BYTES, 0xC093);
    vmwrite(VMCS_GUEST_GS_LIMIT, 0xFFFFFFFF);
    vmwrite(VMCS_GUEST_GS_BASE, 0);

    vmwrite(VMCS_GUEST_INTERRUPTIBILITY, 0);

    if (vmx_cap.ept_supported && vcpu->kvm && vcpu->kvm->ept_root_pa) {
        vmwrite(VMCS_EPT_POINTER, vcpu->kvm->ept_root_pa | EPT_TYPE_WB | EPT_ACCESSED);
    }
}

int vmx_vcpu_create(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return -1;

    vcpu->vmcs = (vmcs_t *)alloc_page_aligned(VMCS_SIZE);
    if (!vcpu->vmcs) return -1;
    memset(vcpu->vmcs, 0, VMCS_SIZE);

    u64 vmcs_pa = get_physical_addr(vcpu->vmcs);
    vcpu->vmcs_pa = vmcs_pa;

    u64 vmcs_revision = vmx_cap.vmx_basic & 0x7FFFFFFFFFFFFFFFULL;
    *(u64 *)vcpu->vmcs->data = vmcs_revision;

    vmclear(vmcs_pa);
    vmptrld(vmcs_pa);

    __asm__ volatile ("mov %%cr3, %0" : "=r"(vcpu->host_cr3));
    vcpu->launched = 0;
    vcpu->active = 1;

    vmcs_init_default(vcpu);

    return 0;
}

int vmx_vcpu_run(kvm_vcpu_t *vcpu)
{
    if (!vcpu || !vcpu->active) return -1;

    vmcs_load(vcpu);

    vcpu->host_rsp = 0;
    vcpu->host_rip = 0;

    u64 rsp_tmp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp_tmp));
    vcpu->host_rsp_save = rsp_tmp;

    u8 err;
    if (!vcpu->launched) {
        err = vmlaunch();
        if (err) {
            u64 reason;
            vmread(VMCS_EXIT_REASON, &reason);
            vcpu->exit_reason = (u32)(reason & 0xFFFF);
            return -1;
        }
        vcpu->launched = 1;
    } else {
        err = vmresume();
        if (err) {
            u64 reason;
            vmread(VMCS_EXIT_REASON, &reason);
            vcpu->exit_reason = (u32)(reason & 0xFFFF);
            return -1;
        }
    }

    return 0;
}

void vmx_handle_hlt(kvm_vcpu_t *vcpu);

void vmx_exit_handler(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;

    u64 exit_reason;
    vmread(VMCS_EXIT_REASON, &exit_reason);
    vcpu->exit_reason = (u32)(exit_reason & 0xFFFF);

    u64 qualification;
    vmread(VMCS_GUEST_PHYSICAL_ADDRESS, &qualification);
    vcpu->exit_qual = (u32)(qualification & 0xFFFFFFFF);

    vmread(VMCS_EXIT_INT_INFO, &vcpu->exit_int_info);
    vmread(VMCS_EXIT_INT_ERROR_CODE, &vcpu->exit_int_error);
    u64 tmp_inst_len;
    vmread(VMCS_EXIT_INSTRUCTION_LEN, &tmp_inst_len);
    vcpu->instruction_length = (u32)tmp_inst_len;

    vmread(VMCS_GUEST_RIP, &vcpu->rip);
    vmread(VMCS_GUEST_RSP, &vcpu->rsp);
    vmread(VMCS_GUEST_RFLAGS, &vcpu->rflags);
    vmread(VMCS_GUEST_CR3, &vcpu->cr3);
    vmread(VMCS_GUEST_EFER, &vcpu->efer);

    vmx_handle_exit(vcpu);
}

void vmx_handle_exit(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;

    switch (vcpu->exit_reason) {
    case VMX_EXIT_REASON_CPUID:
        vmx_handle_cpuid(vcpu);
        break;
    case VMX_EXIT_REASON_IO_INSTRUCTION:
        vmx_handle_io(vcpu);
        break;
    case VMX_EXIT_REASON_EPT_VIOLATION:
        vmx_handle_ept_violation(vcpu);
        break;
    case VMX_EXIT_REASON_CR_ACCESS:
        vmx_handle_cr_access(vcpu);
        break;
    case VMX_EXIT_REASON_MSR_READ:
    case VMX_EXIT_REASON_MSR_WRITE:
        vmx_handle_msr(vcpu);
        break;
    case VMX_EXIT_REASON_EXTERNAL_INT:
        vmx_handle_ext_int(vcpu);
        break;
    case VMX_EXIT_REASON_HLT:
        vmx_handle_hlt(vcpu);
        break;
    case VMX_EXIT_REASON_INVALID_GUEST_STATE:
    case VMX_EXIT_REASON_TRIPLE_FAULT:
        vmx_handle_invalid(vcpu);
        break;
    default:
        break;
    }

    vmwrite(VMCS_GUEST_RIP, vcpu->rip);
    vmwrite(VMCS_GUEST_RSP, vcpu->rsp);
}

void vmx_handle_cpuid(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    u32 eax, ebx, ecx, edx;
    u64 guest_rip;
    vmread(VMCS_GUEST_RIP, &guest_rip);

    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

    vcpu->regs[0] = eax;
    vcpu->regs[1] = ebx;
    vcpu->regs[2] = ecx;
    vcpu->regs[3] = edx;

    vcpu->rip = guest_rip + vcpu->instruction_length;
}

void vmx_handle_io(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    u64 exit_qual;
    vmread(VMCS_EXIT_INSTRUCTION_INFO, &exit_qual);

    u8 direction = (exit_qual >> 0) & 1;
    u8 size = (exit_qual >> 1) & 3;
    u16 port = (exit_qual >> 16) & 0xFFFF;

    if (vcpu->run) {
        vcpu->run->exit_reason = VMX_EXIT_REASON_IO_INSTRUCTION;
        vcpu->run->io.direction = direction;
        vcpu->run->io.size = size;
        vcpu->run->io.port = port;
        vcpu->run->io.count = 1;
    }

    vcpu->rip += vcpu->instruction_length;
}

void vmx_handle_ept_violation(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    u64 gpa;
    vmread(VMCS_GUEST_PHYSICAL_ADDRESS, &gpa);
    vcpu->guest_physical_address = gpa;

    if (vcpu->run) {
        vcpu->run->exit_reason = VMX_EXIT_REASON_EPT_VIOLATION;
        vcpu->run->ept_violation.guest_physical = gpa;
        vcpu->run->ept_violation.rip = vcpu->rip;
    }

    vcpu->rip += vcpu->instruction_length;
}

void vmx_handle_cr_access(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    u64 exit_qual;
    vmread(VMCS_EXIT_QUALIFICATION, &exit_qual);

    u32 cr_num = exit_qual & 0xF;
    u32 access_type = (exit_qual >> 4) & 0x3;
    u32 gpr = (exit_qual >> 8) & 0xF;

    if (vcpu->run) {
        vcpu->run->exit_reason = VMX_EXIT_REASON_CR_ACCESS;
        vcpu->run->cr.cr_num = cr_num;
        vcpu->run->cr.rip = vcpu->rip;
        vcpu->run->cr.cr = exit_qual;
    }

    vcpu->rip += vcpu->instruction_length;
}

void vmx_handle_msr(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;

    u32 ecx = (u32)vcpu->regs[1];

    if (vcpu->exit_reason == VMX_EXIT_REASON_MSR_READ) {
        u64 val = rdmsr_safe(ecx);
        vcpu->regs[0] = (u32)(val & 0xFFFFFFFF);
        vcpu->regs[2] = (u32)(val >> 32);
        if (vcpu->run) {
            vcpu->run->exit_reason = VMX_EXIT_REASON_MSR_READ;
            vcpu->run->msr.msr = ecx;
            vcpu->run->msr.data = val;
            vcpu->run->msr.write = 0;
        }
    } else if (vcpu->exit_reason == VMX_EXIT_REASON_MSR_WRITE) {
        u64 val = ((u64)vcpu->regs[2] << 32) | (u32)vcpu->regs[0];
        wrmsr_safe(ecx, val);
        if (vcpu->run) {
            vcpu->run->exit_reason = VMX_EXIT_REASON_MSR_WRITE;
            vcpu->run->msr.msr = ecx;
            vcpu->run->msr.data = val;
            vcpu->run->msr.write = 1;
        }
    }

    vcpu->rip += vcpu->instruction_length;
}

void vmx_handle_ext_int(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
}

void vmx_handle_invalid(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    vcpu->active = 0;
}

void vmx_handle_hlt(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    vcpu->mp_state = 1;
    vcpu->rip += vcpu->instruction_length;
}

void *ept_alloc_page(void)
{
    return alloc_page_aligned(4096);
}

void ept_free_page(void *page)
{
    if (page) kfree(page);
}

int ept_init(kvm_vm_t *vm)
{
    if (!vm) return -1;

    void *ept_root = ept_alloc_page();
    if (!ept_root) return -1;
    memset(ept_root, 0, 4096);

    vm->ept_root = ept_root;
    vm->ept_root_phys = get_physical_addr(ept_root);
    vm->ept_root_pa = vm->ept_root_phys;

    return 0;
}

static u64 *ept_get_entry(u64 *table, u32 index)
{
    return &table[index];
}

int ept_map_page(kvm_vm_t *vm, u64 gpa, u64 hpa, u64 flags)
{
    if (!vm || !vm->ept_root) return -1;

    u64 *ept_root = (u64 *)vm->ept_root;
    u64 page_mask = ~(u64)0xFFF;
    u64 level_offset[EPT_PAGE_LEVELS];
    u64 addr = gpa;

    level_offset[3] = (addr >> 39) & 0x1FF;
    level_offset[2] = (addr >> 30) & 0x1FF;
    level_offset[1] = (addr >> 21) & 0x1FF;
    level_offset[0] = (addr >> 12) & 0x1FF;

    for (int level = 3; level >= 1; level--) {
        u64 *entry = ept_get_entry(ept_root, level_offset[level]);
        if (!((*entry) & EPT_PRESENT)) {
            void *new_table = ept_alloc_page();
            if (!new_table) return -1;
            memset(new_table, 0, 4096);
            *entry = get_physical_addr(new_table) | EPT_PRESENT | EPT_WRITABLE | EPT_EXEC | EPT_TYPE_WB | EPT_ACCESSED;
        }
        ept_root = (u64 *)(*entry & page_mask);
    }

    u64 *leaf = ept_get_entry(ept_root, level_offset[0]);
    *leaf = (hpa & page_mask) | flags | EPT_PRESENT | EPT_ACCESSED;

    return 0;
}

int ept_unmap_page(kvm_vm_t *vm, u64 gpa)
{
    if (!vm || !vm->ept_root) return -1;

    u64 *ept_root = (u64 *)vm->ept_root;
    u64 page_mask = ~(u64)0xFFF;

    u64 idx3 = (gpa >> 39) & 0x1FF;
    u64 idx2 = (gpa >> 30) & 0x1FF;
    u64 idx1 = (gpa >> 21) & 0x1FF;
    u64 idx0 = (gpa >> 12) & 0x1FF;

    if (!(ept_root[idx3] & EPT_PRESENT)) return -1;
    ept_root = (u64 *)(ept_root[idx3] & page_mask);
    if (!ept_root) return -1;

    if (!(ept_root[idx2] & EPT_PRESENT)) return -1;
    ept_root = (u64 *)(ept_root[idx2] & page_mask);
    if (!ept_root) return -1;

    if (!(ept_root[idx1] & EPT_PRESENT)) return -1;
    ept_root[idx0] = 0;

    return 0;
}

int kvm_init(void)
{
    kvm_vm_list = NULL;
    kvm_next_vm_id = 1;
    spin_init(&kvm_global_lock);

    memset(&vmx_cap, 0, sizeof(vmx_cap));
    detect_vmx_capabilities();

    return vmx_cap.vmx_supported ? 0 : -1;
}

int kvm_vm_create(kvm_vm_t *vm)
{
    if (!vm) return -1;

    memset(vm, 0, sizeof(kvm_vm_t));

    spin_lock(&kvm_global_lock);
    vm->vm_id = kvm_next_vm_id++;
    spin_unlock(&kvm_global_lock);

    vm->nr_vcpus = 0;
    vm->active = 0;
    vm->tsc_offset = 0;
    spin_init(&vm->lock);

    if (ept_init(vm) != 0) {
        return -1;
    }

    vm->active = 1;

    spin_lock(&kvm_global_lock);
    vm->next_vm = kvm_vm_list;
    kvm_vm_list = vm;
    spin_unlock(&kvm_global_lock);
    (void)vm->next_vm;

    return 0;
}

void kvm_vm_destroy(kvm_vm_t *vm)
{
    if (!vm) return;

    for (u32 i = 0; i < vm->nr_vcpus; i++) {
        if (vm->vcpus[i]) {
            kvm_vcpu_destroy(vm->vcpus[i]);
            vm->vcpus[i] = NULL;
        }
    }

    if (vm->ept_root) {
        ept_free_page(vm->ept_root);
        vm->ept_root = NULL;
    }

    vm->active = 0;
}

int kvm_vcpu_create(kvm_vm_t *vm, u32 vcpu_id, kvm_vcpu_t *vcpu)
{
    if (!vm || !vcpu) return -1;
    if (vm->nr_vcpus >= KVM_MAX_VCPUS) return -1;

    memset(vcpu, 0, sizeof(kvm_vcpu_t));
    vcpu->vcpu_id = vcpu_id;
    vcpu->kvm = vm;
    vcpu->rip = 0;
    vcpu->rsp = 0;
    vcpu->rflags = 0x2;
    vcpu->cr0 = vmx_cap.cr0_fixed1 & 0xFFFFFFFF;
    vcpu->cr3 = 0;
    vcpu->cr4 = vmx_cap.cr4_fixed1 & 0xFFFFFFFF;
    vcpu->efer = 0;
    vcpu->launched = 0;
    vcpu->active = 0;
    spin_init(&vcpu->lock);

    vcpu->run = (struct kvm_run *)alloc_page_aligned(4096);
    if (!vcpu->run) return -1;
    memset(vcpu->run, 0, 4096);

    if (vmx_vcpu_create(vcpu) != 0) {
        if (vcpu->run) kfree(vcpu->run);
        return -1;
    }

    vm->vcpus[vm->nr_vcpus] = vcpu;
    vm->nr_vcpus++;
    return 0;
}

void kvm_vcpu_destroy(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return;
    if (vcpu->vmcs) {
        vmclear(vcpu->vmcs_pa);
        ept_free_page(vcpu->vmcs);
        vcpu->vmcs = NULL;
    }
    if (vcpu->run) {
        kfree(vcpu->run);
        vcpu->run = NULL;
    }
    vcpu->active = 0;
}

int kvm_vcpu_run(kvm_vcpu_t *vcpu)
{
    if (!vcpu || !vcpu->active) return -1;

    vmcs_load(vcpu);

    int ret = vmx_vcpu_run(vcpu);

    if (ret == 0) {
        vmx_exit_handler(vcpu);
    }

    return ret;
}

int kvm_vcpu_setup(kvm_vcpu_t *vcpu)
{
    if (!vcpu) return -1;
    vmcs_load(vcpu);
    vmcs_init_default(vcpu);
    return 0;
}
