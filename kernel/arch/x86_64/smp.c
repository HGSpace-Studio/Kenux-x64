

#include <arch/smp.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];
extern void ap_trampoline_gdt_ptr();
extern uint64_t ap_kernel_entry;
extern uint64_t ap_stack_top;
extern volatile uint32_t ap_booted;

static percpu_area_t percpu_areas[SMP_MAX_CPUS];
static cpu_info_t cpu_infos[SMP_MAX_CPUS];
static uint32_t cpu_count = 1;
static uint32_t bsp_apic_id = 0;

static spinlock_t smp_lock = SPINLOCK_INIT;

uint64_t ap_kernel_entry = 0;
uint64_t ap_stack_top = 0;
volatile uint32_t ap_booted = 0;
uint8_t ap_trampoline_bin_start;
uint8_t ap_trampoline_bin_end;

static volatile uint32_t* ioapic_base = (volatile uint32_t*)IOAPIC_BASE_DEFAULT;

static uint32_t ioapic_read(uint32_t reg)
{
    ioapic_base[0] = reg;
    return ioapic_base[4];
}

static void ioapic_write(uint32_t reg, uint32_t value)
{
    ioapic_base[0] = reg;
    ioapic_base[4] = value;
}

void smp_ioapic_init(void)
{
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    uint32_t entries = ((ver >> 16) & 0xFF) + 1;

    for (uint32_t i = 0; i < entries; i++) {
        uint32_t reg = IOAPIC_REG_REDIR_BASE + i * 2;

        ioapic_write(reg + 1, 0);

        ioapic_write(reg, 0x20 + i);
    }
}

void ioapic_set_irq(uint8_t irq, uint64_t redirect)
{
    uint32_t reg = IOAPIC_REG_REDIR_BASE + irq * 2;
    ioapic_write(reg + 1, (uint32_t)(redirect >> 32));
    ioapic_write(reg, (uint32_t)redirect);
}

void smp_setup_percpu(uint32_t apic_id, void* percpu_base)
{
    smp_set_gs_base(percpu_base);
}

static void smp_send_init(uint32_t apic_id)
{
    lapic_write(LAPIC_ICR1, apic_id << 24);
    lapic_write(LAPIC_ICR0, 0x00000500);

    for (volatile uint64_t i = 0; i < 10000000; i++) cpu_relax();
}

static void smp_send_sipi(uint32_t apic_id, uint8_t vector)
{
    lapic_write(LAPIC_ICR1, apic_id << 24);
    lapic_write(LAPIC_ICR0, 0x00000600 | vector);
}

static void smp_boot_ap(uint32_t apic_id, uint32_t processor_id)
{

    percpu_area_t* area = &percpu_areas[processor_id];
    cpu_info_t* info = &cpu_infos[processor_id];

    memset(area, 0, sizeof(percpu_area_t));
    memset(info, 0, sizeof(cpu_info_t));

    info->apic_id = apic_id;
    info->processor_id = processor_id;
    info->state = CPU_STATE_BOOTING;

    area->cpu = info;
    area->kernel_stack = memory_alloc_aligned(16384, 16);

    ap_kernel_entry = (uint64_t)smp_ap_main;
    ap_stack_top = (uint64_t)area->kernel_stack + 16384;
    ap_booted = 0;

    extern uint8_t ap_trampoline_bin_start;
    extern uint8_t ap_trampoline_bin_end;
    uint8_t* trampoline_dst = (uint8_t*)0x8000;
    uint8_t* trampoline_src = &ap_trampoline_bin_start;
    size_t trampoline_size = (size_t)(&ap_trampoline_bin_end - &ap_trampoline_bin_start);
    memcpy(trampoline_dst, trampoline_src, trampoline_size);

    smp_send_init(apic_id);

    smp_send_sipi(apic_id, 0x08);
    for (volatile uint64_t i = 0; i < 1000000; i++) cpu_relax();
    smp_send_sipi(apic_id, 0x08);

    uint64_t timeout = 100000000;
    while (ap_booted == 0 && timeout--) {
        cpu_relax();
    }

    if (ap_booted) {
        info->state = CPU_STATE_ONLINE;
        spin_lock(&smp_lock);
        cpu_count++;
        spin_unlock(&smp_lock);
    } else {
        info->state = CPU_STATE_OFFLINE;
    }
}

void smp_init(void)
{

    bsp_apic_id = lapic_id();

    memset(percpu_areas, 0, sizeof(percpu_areas));
    memset(cpu_infos, 0, sizeof(cpu_infos));

    cpu_infos[0].apic_id = bsp_apic_id;
    cpu_infos[0].processor_id = 0;
    cpu_infos[0].state = CPU_STATE_ONLINE;
    percpu_areas[0].cpu = &cpu_infos[0];
    percpu_areas[0].kernel_stack = memory_alloc_aligned(16384, 16);

    smp_setup_percpu(bsp_apic_id, &percpu_areas[0]);

    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE);

    ioapic_init();

    (void)smp_boot_ap;
}

void smp_ap_main(void)
{
    uint32_t apic_id = lapic_id();

    percpu_area_t* area = NULL;
    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++) {
        if (cpu_infos[i].apic_id == apic_id) {
            area = &percpu_areas[i];
            break;
        }
    }
    if (!area) {

        while (1) cpu_halt();
    }

    smp_setup_percpu(apic_id, area);

    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE);

    ap_booted = 1;
    mb();

    while (1) {
        cpu_halt();
    }
}

uint32_t smp_cpu_count(void)
{
    return cpu_count;
}

uint32_t smp_cpu_id(void)
{
    return lapic_id();
}

cpu_info_t* smp_cpu_info(uint32_t apic_id)
{
    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++) {
        if (cpu_infos[i].apic_id == apic_id) {
            return &cpu_infos[i];
        }
    }
    return NULL;
}

int smp_cpu_online(uint32_t apic_id)
{
    cpu_info_t* info = smp_cpu_info(apic_id);
    return info && info->state == CPU_STATE_ONLINE;
}

void smp_ipi_resched(uint32_t target_apic_id)
{
    lapic_send_ipi(target_apic_id, IPI_VECTOR_RESCHED);
}

void smp_ipi_resched_all(void)
{
    lapic_send_ipi(0xFF, IPI_VECTOR_RESCHED);
}

void smp_ipi_tlbflush(uint32_t target_apic_id)
{
    lapic_send_ipi(target_apic_id, IPI_VECTOR_TLBFLUSH);
}

void smp_ipi_tlbflush_all(void)
{
    lapic_send_ipi(0xFF, IPI_VECTOR_TLBFLUSH);
}

void smp_ipi_halt(uint32_t target_apic_id)
{
    lapic_send_ipi(target_apic_id, IPI_VECTOR_HALT);
}

void smp_ipi_call(uint32_t target_apic_id, void (*func)(void*), void* arg)
{
    cpu_info_t* info = smp_cpu_info(target_apic_id);
    if (!info || info->state != CPU_STATE_ONLINE) return;

    info->call_func = func;
    info->call_arg = arg;
    info->call_done = 0;
    mb();

    lapic_send_ipi(target_apic_id, IPI_VECTOR_CALL);

    while (!info->call_done) {
        cpu_relax();
    }
}

void smp_ipi_handler_resched(void)
{
    lapic_eoi();

    cpu_info_t* cpu = smp_current_cpu();
    if (cpu) {
        cpu->sched_ticks = 1;
    }
}

void smp_ipi_handler_tlbflush(void)
{
    lapic_eoi();

    __asm__ volatile ("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

void smp_ipi_handler_halt(void)
{
    lapic_eoi();
    while (1) cpu_halt();
}

void smp_ipi_handler_call(void)
{
    cpu_info_t* cpu = smp_current_cpu();
    lapic_eoi();

    if (cpu && cpu->call_func) {
        cpu->call_func(cpu->call_arg);
        cpu->call_func = NULL;
    }
    cpu->call_done = 1;
    mb();
}
