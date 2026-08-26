#ifndef ARCH_KVM_VMX_H
#define ARCH_KVM_VMX_H

#include <arch/types.h>

#define VMX_BASIC_MSR           0x480
#define VMX_CR0_FIXED0          0x486
#define VMX_CR0_FIXED1          0x487
#define VMX_CR4_FIXED0          0x488
#define VMX_CR4_FIXED1          0x489
#define VMX_EPT_VPID_CAP_MSR    0x48C
#define VMX_TRUE_PINBASED_CTLS  0x48D
#define VMX_TRUE_PROCBASED_CTLS 0x48E
#define VMX_TRUE_EXIT_CTLS      0x48F
#define VMX_TRUE_ENTRY_CTLS     0x490

#define VMX_CPU_BASED_NMI_WINDOW        (1U << 3)
#define VMX_CPU_BASED_HLT_EXITING       (1U << 7)
#define VMX_CPU_BASED_INVLPG_EXITING    (1U << 9)
#define VMX_CPU_BASED_MWAIT_EXITING     (1U << 10)
#define VMX_CPU_BASED_RDPMC_EXITING     (1U << 11)
#define VMX_CPU_BASED_RDTSC_EXITING     (1U << 12)
#define VMX_CPU_BASED_CR3_LOAD_EXITING  (1U << 15)
#define VMX_CPU_BASED_CR3_STORE_EXITING (1U << 16)
#define VMX_CPU_BASED_CR8_LOAD_EXITING  (1U << 19)
#define VMX_CPU_BASED_CR8_STORE_EXITING (1U << 20)
#define VMX_CPU_BASED_TPR_SHADOW        (1U << 21)
#define VMX_CPU_BASED_VIRTUAL_NMI       (1U << 22)
#define VMX_CPU_BASED_MOV_DR_EXITING    (1U << 23)
#define VMX_CPU_BASED_UNCOND_IO_EXITING (1U << 24)
#define VMX_CPU_BASED_USE_IO_BITMAPS    (1U << 25)
#define VMX_CPU_BASED_MONITOR_EXITING   (1U << 27)
#define VMX_CPU_BASED_PAUSE_EXITING     (1U << 28)
#define VMX_CPU_BASED_USE_MSR_BITMAPS   (1U << 28)
#define VMX_CPU_BASED_ACTIVATE_SECONDARY (1U << 31)

#define VMX_SEC_EXEC_ENABLE_EPT         (1U << 1)
#define VMX_SEC_EXEC_ENABLE_VPID        (1U << 5)
#define VMX_SEC_EXEC_UNRESTRICTED_GUEST (1U << 7)
#define VMX_SEC_EXEC_ENABLE_PML         (1U << 17)

#define VMX_VM_EXIT_IA32E_MODE          (1U << 9)
#define VMX_VM_EXIT_ACK_INTR_ON_EXIT    (1U << 15)

#define VMX_VM_ENTRY_IA32E_MODE         (1U << 9)
#define VMX_VM_ENTRY_LOAD_EFER          (1U << 15)

#define VMX_EXIT_REASON_EXCEPTION_NMI   0
#define VMX_EXIT_REASON_EXTERNAL_INT    1
#define VMX_EXIT_REASON_TRIPLE_FAULT    2
#define VMX_EXIT_REASON_INIT            3
#define VMX_EXIT_REASON_SIPI            4
#define VMX_EXIT_REASON_IO_SMI          5
#define VMX_EXIT_REASON_OTHER_SMI       6
#define VMX_EXIT_REASON_PENDING_INT     7
#define VMX_EXIT_REASON_NMI_WINDOW      8
#define VMX_EXIT_REASON_TASK_SWITCH     9
#define VMX_EXIT_REASON_CPUID           10
#define VMX_EXIT_REASON_HLT             12
#define VMX_EXIT_REASON_INVD            13
#define VMX_EXIT_REASON_INVLPG          14
#define VMX_EXIT_REASON_RDPMC           15
#define VMX_EXIT_REASON_RDTSC           16
#define VMX_EXIT_REASON_RSM             17
#define VMX_EXIT_REASON_VMCALL          18
#define VMX_EXIT_REASON_VMCLEAR        19
#define VMX_EXIT_REASON_VMLAUNCH       20
#define VMX_EXIT_REASON_VMPTRLD        21
#define VMX_EXIT_REASON_VMPTRST        22
#define VMX_EXIT_REASON_VMREAD         23
#define VMX_EXIT_REASON_VMRESUME       24
#define VMX_EXIT_REASON_VMWRITE        25
#define VMX_EXIT_REASON_VMXOFF         26
#define VMX_EXIT_REASON_VMXON          27
#define VMX_EXIT_REASON_CR_ACCESS      28
#define VMX_EXIT_REASON_DR_ACCESS      29
#define VMX_EXIT_REASON_IO_INSTRUCTION 30
#define VMX_EXIT_REASON_MSR_READ       31
#define VMX_EXIT_REASON_MSR_WRITE      32
#define VMX_EXIT_REASON_INVALID_GUEST_STATE 33
#define VMX_EXIT_REASON_MSR_LOADING    34
#define VMX_EXIT_REASON_MWAIT          36
#define VMX_EXIT_REASON_MONITOR        39
#define VMX_EXIT_REASON_PAUSE          40
#define VMX_EXIT_REASON_MACHINE_CHECK  41
#define VMX_EXIT_REASON_TPR_BELOW_THRESHOLD 43
#define VMX_EXIT_REASON_APIC_ACCESS    44
#define VMX_EXIT_REASON_EOI_VIOLATION  45
#define VMX_EXIT_REASON_APIC_WRITE     56
#define VMX_EXIT_REASON_EPT_VIOLATION  48
#define VMX_EXIT_REASON_EPT_MISCONFIG  49
#define VMX_EXIT_REASON_INVEPT         50
#define VMX_EXIT_REASON_RDTSCP         51
#define VMX_EXIT_REASON_VMX_PREEMPT    52
#define VMX_EXIT_REASON_INVVPID        53
#define VMX_EXIT_REASON_WBINVD         54
#define VMX_EXIT_REASON_XSETBV         55

#define VMCS_FIELD(width, type, index) (((width) << 13) | ((type) << 10) | (index))

#define VMCS_VPID                       0x0000
#define VMCS_GUEST_ES_SEL               0x0800
#define VMCS_GUEST_CS_SEL               0x0802
#define VMCS_GUEST_SS_SEL               0x0804
#define VMCS_GUEST_DS_SEL               0x0806
#define VMCS_GUEST_FS_SEL               0x0808
#define VMCS_GUEST_GS_SEL               0x080A
#define VMCS_GUEST_LDTR_SEL             0x080C
#define VMCS_GUEST_TR_SEL               0x080E
#define VMCS_HOST_ES_SEL                0x0C00
#define VMCS_HOST_CS_SEL                0x0C02
#define VMCS_HOST_SS_SEL                0x0C04
#define VMCS_HOST_DS_SEL                0x0C06
#define VMCS_HOST_FS_SEL                0x0C08
#define VMCS_HOST_GS_SEL                0x0C0A
#define VMCS_HOST_TR_SEL                0x0C0C
#define VMCS_IO_BITMAP_A                0x2000
#define VMCS_IO_BITMAP_B                0x2002
#define VMCS_MSR_BITMAP                 0x2004
#define VMCS_EXIT_MSR_STORE_ADDR        0x2006
#define VMCS_EXIT_MSR_LOAD_ADDR         0x2008
#define VMCS_ENTRY_MSR_LOAD_ADDR        0x200A
#define VMCS_TSC_OFFSET                 0x2010
#define VMCS_VIRTUAL_APIC_PAGE_ADDR     0x2012
#define VMCS_APIC_ACCESS_ADDR           0x2014
#define VMCS_EPT_POINTER                0x201A
#define VMCS_EPTP_LIST_ADDRESS          0x2024
#define VMCS_GUEST_PHYSICAL_ADDRESS     0x2400
#define VMCS_VM_INSTRUCTION_ERROR       0x4400
#define VMCS_EXIT_REASON                0x4402
#define VMCS_EXIT_INT_INFO              0x4404
#define VMCS_EXIT_INT_ERROR_CODE        0x4406
#define VMCS_IDT_VECTORING_INFO         0x4408
#define VMCS_IDT_VECTORING_ERROR_CODE   0x440A
#define VMCS_EXIT_INSTRUCTION_LEN       0x440C
#define VMCS_EXIT_INSTRUCTION_INFO      0x440E
#define VMCS_GUEST_ES_LIMIT             0x4800
#define VMCS_GUEST_CS_LIMIT             0x4802
#define VMCS_GUEST_SS_LIMIT             0x4804
#define VMCS_GUEST_DS_LIMIT             0x4806
#define VMCS_GUEST_FS_LIMIT             0x4808
#define VMCS_GUEST_GS_LIMIT             0x480A
#define VMCS_GUEST_LDTR_LIMIT           0x480C
#define VMCS_GUEST_TR_LIMIT             0x480E
#define VMCS_GUEST_GDTR_LIMIT           0x4810
#define VMCS_GUEST_IDTR_LIMIT           0x4812
#define VMCS_GUEST_ES_AR_BYTES          0x4814
#define VMCS_GUEST_CS_AR_BYTES          0x4816
#define VMCS_GUEST_SS_AR_BYTES          0x4818
#define VMCS_GUEST_DS_AR_BYTES          0x481A
#define VMCS_GUEST_FS_AR_BYTES          0x481C
#define VMCS_GUEST_GS_AR_BYTES          0x481E
#define VMCS_GUEST_LDTR_AR_BYTES        0x4820
#define VMCS_GUEST_TR_AR_BYTES          0x4822
#define VMCS_GUEST_INTERRUPTIBILITY     0x4824
#define VMCS_GUEST_ACTIVITY_STATE       0x4826
#define VMCS_GUEST_SYSENTER_CS          0x482A
#define VMCS_GUEST_CR0                  0x6800
#define VMCS_GUEST_CR3                  0x6802
#define VMCS_GUEST_CR4                  0x6804
#define VMCS_GUEST_ES_BASE              0x6806
#define VMCS_GUEST_CS_BASE              0x6808
#define VMCS_GUEST_SS_BASE              0x680A
#define VMCS_GUEST_DS_BASE              0x680C
#define VMCS_GUEST_FS_BASE              0x680E
#define VMCS_GUEST_GS_BASE              0x6810
#define VMCS_GUEST_LDTR_BASE            0x6812
#define VMCS_GUEST_TR_BASE              0x6814
#define VMCS_GUEST_GDTR_BASE            0x6816
#define VMCS_GUEST_IDTR_BASE            0x6818
#define VMCS_GUEST_DR7                  0x681A
#define VMCS_GUEST_RSP                  0x681C
#define VMCS_GUEST_RIP                  0x681E
#define VMCS_GUEST_RFLAGS               0x6820
#define VMCS_GUEST_PENDING_DBG_EXCS     0x6822
#define VMCS_GUEST_SYSENTER_ESP         0x6824
#define VMCS_GUEST_SYSENTER_EIP         0x6826
#define VMCS_GUEST_EFER                 0x2806
#define VMCS_HOST_CR0                   0x6C00
#define VMCS_HOST_CR3                   0x6C02
#define VMCS_HOST_CR4                   0x6C04
#define VMCS_HOST_FS_BASE               0x6C06
#define VMCS_HOST_GS_BASE               0x6C08
#define VMCS_HOST_TR_BASE               0x6C0A
#define VMCS_HOST_GDTR_BASE             0x6C0C
#define VMCS_HOST_IDTR_BASE             0x6C0E
#define VMCS_HOST_SYSENTER_ESP          0x6C10
#define VMCS_HOST_SYSENTER_EIP          0x6C12
#define VMCS_HOST_RSP                   0x6C14
#define VMCS_HOST_RIP                   0x6C16
#define VMCS_ENTRY_CONTROLS             0x4012
#define VMCS_EXIT_CONTROLS              0x400C
#define VMCS_PIN_BASED_VM_EXEC_CONTROLS 0x4000
#define VMCS_CPU_BASED_VM_EXEC_CONTROLS 0x4002
#define VMCS_EXCEPTION_BITMAP           0x4004
#define VMCS_PAGE_FAULT_ERROR_CODE_MASK 0x4006
#define VMCS_PAGE_FAULT_ERROR_CODE_MATCH 0x4008
#define VMCS_CR3_TARGET_COUNT           0x400A
#define VMCS_VM_EXIT_MSR_STORE_COUNT    0x400E
#define VMCS_VM_EXIT_MSR_LOAD_COUNT     0x4010
#define VMCS_VM_ENTRY_MSR_LOAD_COUNT    0x4014
#define VMCS_VM_ENTRY_INTR_INFO         0x4016
#define VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE 0x4018
#define VMCS_VM_ENTRY_INSTRUCTION_LEN   0x401A
#define VMCS_TPR_THRESHOLD              0x401C
#define VMCS_SECONDARY_VM_EXEC_CONTROL  0x401E
#define VMCS_CR0_GUEST_HOST_MASK        0x6000
#define VMCS_CR4_GUEST_HOST_MASK        0x6002
#define VMCS_CR0_READ_SHADOW            0x6004
#define VMCS_CR4_READ_SHADOW            0x6006
#define VMCS_CR3_TARGET_VALUE0          0x6008
#define VMCS_CR3_TARGET_VALUE1          0x600A
#define VMCS_CR3_TARGET_VALUE2          0x600C
#define VMCS_CR3_TARGET_VALUE3          0x600E

#define EPT_PAGE_LEVELS         4
#define EPT_PRESENT             (1ULL << 0)
#define EPT_WRITABLE            (1ULL << 1)
#define EPT_EXEC                (1ULL << 2)
#define EPT_MT_WB               (6ULL << 3)
#define EPT_LARGE_PAGE          (1ULL << 7)
#define EPT_ACCESSED            (1ULL << 8)
#define EPT_DIRTY               (1ULL << 9)

typedef struct vmx_capability {
    u64 vmx_basic;
    u64 ept_vpid_cap;
    u64 pinbased_ctls;
    u64 procbased_ctls;
    u64 exit_ctls;
    u64 entry_ctls;
    u64 cr0_fixed0;
    u64 cr0_fixed1;
    u64 cr4_fixed0;
    u64 cr4_fixed1;
    int vmx_supported;
    int ept_supported;
    int vpid_supported;
} vmx_capability_t;

static inline uint8_t vmread(uint64_t field, uint64_t* value)
{
    uint8_t error;
    uint64_t val;
    __asm__ volatile (
        "vmread %2, %1\n"
        "setna %0\n"
        : "=r"(error), "=r"(val)
        : "r"(field)
        : "cc", "memory"
    );
    *value = val;
    return error;
}

static inline uint8_t vmwrite(uint64_t field, uint64_t value)
{
    uint8_t error;
    __asm__ volatile (
        "vmwrite %1, %2\n"
        "setna %0\n"
        : "=r"(error)
        : "r"(value), "r"(field)
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmclear(uint64_t addr)
{
    uint8_t error;
    __asm__ volatile (
        "vmclear %1\n"
        "setna %0\n"
        : "=r"(error)
        : "m"(addr)
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmptrld(uint64_t addr)
{
    uint8_t error;
    __asm__ volatile (
        "vmptrld %1\n"
        "setna %0\n"
        : "=r"(error)
        : "m"(addr)
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmlaunch(void)
{
    uint8_t error;
    __asm__ volatile (
        "vmlaunch\n"
        "setna %0\n"
        : "=r"(error)
        :
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmresume(void)
{
    uint8_t error;
    __asm__ volatile (
        "vmresume\n"
        "setna %0\n"
        : "=r"(error)
        :
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmxoff(void)
{
    uint8_t error;
    __asm__ volatile (
        "vmxoff\n"
        "setna %0\n"
        : "=r"(error)
        :
        : "cc", "memory"
    );
    return error;
}

static inline uint8_t vmxon(uint64_t addr)
{
    uint8_t error;
    __asm__ volatile (
        "vmxon %1\n"
        "setna %0\n"
        : "=r"(error)
        : "m"(addr)
        : "cc", "memory"
    );
    return error;
}

static inline uint64_t vmx_read_cr_fixed(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t adjust_vmx_controls(uint64_t ctl, uint64_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    uint64_t msr_val = ((uint64_t)hi << 32) | lo;
    uint64_t allowed_1 = msr_val & 0xFFFFFFFF;
    uint64_t allowed_0 = msr_val >> 32;
    ctl &= allowed_0;
    ctl |= allowed_1;
    return ctl;
}

#endif
