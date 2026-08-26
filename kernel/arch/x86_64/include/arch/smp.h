

#ifndef ARCH_SMP_H
#define ARCH_SMP_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SMP_MAX_CPUS        64

#define IPI_VECTOR_RESCHED  0xF0
#define IPI_VECTOR_TLBFLUSH 0xF1
#define IPI_VECTOR_HALT     0xF2
#define IPI_VECTOR_CALL     0xF3

#define CPU_STATE_OFFLINE   0
#define CPU_STATE_BOOTING   1
#define CPU_STATE_ONLINE    2

typedef struct {
    uint32_t apic_id;
    uint32_t processor_id;
    uint32_t state;
    uint64_t idle_ticks;
    uint64_t irq_count;
    uint64_t nmi_count;

    uint64_t current_pid;
    uint64_t sched_ticks;

    void* interrupt_stack;

    volatile uint32_t tlb_flush_pending;

    void (*call_func)(void* arg);
    void* call_arg;
    volatile uint32_t call_done;
} cpu_info_t;

typedef struct {
    cpu_info_t* cpu;
    void*       kernel_stack;
    uint64_t    current_task;
    uint64_t    irq_stack_top;
    uint8_t     padding[4096 - 32];
} percpu_area_t;

void smp_init(void);
void smp_ap_main(void);

uint32_t smp_cpu_count(void);
uint32_t smp_cpu_id(void);
cpu_info_t* smp_cpu_info(uint32_t apic_id);
int smp_cpu_online(uint32_t apic_id);

void smp_ipi_resched(uint32_t target_apic_id);
void smp_ipi_resched_all(void);
void smp_ipi_tlbflush(uint32_t target_apic_id);
void smp_ipi_tlbflush_all(void);
void smp_ipi_halt(uint32_t target_apic_id);

void smp_ipi_call(uint32_t target_apic_id, void (*func)(void*), void* arg);

void smp_setup_percpu(uint32_t apic_id, void* percpu_base);

static inline void* smp_percpu_base(void)
{
    uint32_t base_lo, base_hi;
    __asm__ volatile ("rdmsr" : "=a"(base_lo), "=d"(base_hi) : "c"(0xC0000101));
    return (void*)(((uint64_t)base_hi << 32) | base_lo);
}

static inline void smp_set_gs_base(void* base)
{
    uint64_t addr = (uint64_t)base;
    uint32_t addr_lo = (uint32_t)addr;
    uint32_t addr_hi = (uint32_t)(addr >> 32);
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(0xC0000101), "a"(addr_lo), "d"(addr_hi)
    );
}

static inline cpu_info_t* smp_current_cpu(void)
{
    cpu_info_t* cpu;
    __asm__ volatile (
        "movq %%gs:0, %0\n"
        : "=r"(cpu)
    );
    return cpu;
}

#define LAPIC_BASE_DEFAULT  0xFEE00000ULL

#define LAPIC_ID            0x020
#define LAPIC_VER           0x030
#define LAPIC_TPR           0x080
#define LAPIC_EOI           0x0B0
#define LAPIC_SVR           0x0F0
#define LAPIC_ESR           0x280
#define LAPIC_ICR0          0x300
#define LAPIC_ICR1          0x310
#define LAPIC_LVT_TIMER     0x320
#define LAPIC_LVT_THERMAL   0x330
#define LAPIC_LVT_PERF      0x340
#define LAPIC_LVT_LINT0     0x350
#define LAPIC_LVT_LINT1     0x360
#define LAPIC_LVT_ERROR     0x370
#define LAPIC_TIMER_INIT    0x380
#define LAPIC_TIMER_CURR    0x390
#define LAPIC_TIMER_DIV     0x3E0

#define LAPIC_TIMER_PERIODIC  0x20000
#define LAPIC_TIMER_MASKED    0x10000
#define LAPIC_SVR_ENABLE      0x100

#define IOAPIC_BASE_DEFAULT   0xFEC00000ULL
#define IOAPIC_REG_ID         0x00
#define IOAPIC_REG_VER        0x01
#define IOAPIC_REG_REDIR_BASE 0x10

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);
void lapic_eoi(void);
void lapic_send_ipi(uint32_t target, uint32_t vector);
void lapic_setup_timer(uint32_t vector, uint32_t ms);
uint32_t lapic_id(void);

void ioapic_init(void);
void ioapic_set_irq(uint8_t irq, uint64_t redirect);

static inline void cpu_relax(void)
{
    __asm__ volatile ("pause");
}

static inline void cpu_halt(void)
{
    __asm__ volatile ("hlt");
}

static inline void mb(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

static inline void wmb(void)
{
    __asm__ volatile ("sfence" ::: "memory");
}

static inline void rmb(void)
{
    __asm__ volatile ("lfence" ::: "memory");
}

#endif
