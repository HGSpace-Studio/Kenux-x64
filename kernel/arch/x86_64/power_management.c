#include <kapi.h>

#define ACPI_RSDP_SIGNATURE     "RSD PTR "
#define ACPI_RSDT_SIGNATURE     "RSDT"
#define ACPI_XSDT_SIGNATURE     "XSDT"
#define ACPI_FADT_SIGNATURE     "FACP"
#define ACPI_MADT_SIGNATURE     "APIC"

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    char signature[8];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32 creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t entry[0];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t entry[0];
} __attribute__((packed)) acpi_xsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t dsdt;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint32_t pm4a;
    uint64_t xb_spc;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_blk_len;
    uint8_t gpe1_blk_len;
    uint8_t gpe1_base;
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t iapc_boot_arch;
    uint8_t reserved;
    uint32_t flags;
} __attribute__((packed)) acpi_fadt_t;

typedef enum {
    POWER_STATE_S0,
    POWER_STATE_S1,
    POWER_STATE_S2,
    POWER_STATE_S3,
    POWER_STATE_S4,
    POWER_STATE_S5
} power_state_t;

typedef enum {
    CPU_STATE_C0,
    CPU_STATE_C1,
    CPU_STATE_C2,
    CPU_STATE_C3,
    CPU_STATE_C4
} cpu_state_t;

typedef struct {
    bool initialized;
    acpi_rsdp_t* rsdp;
    acpi_rsdt_t* rsdt;
    acpi_xsdt_t* xsdt;
    acpi_fadt_t* fadt;
    power_state_t current_state;
    cpu_state_t current_cpu_state;
    uint32_t s5_type;
    spinlock_t lock;
    uint64_t last_idle_time;
    uint64_t total_idle_time;
    uint64_t total_wakeups;
} power_manager_t;

static power_manager_t pmgr;

static uint8_t acpi_checksum(void* ptr, size_t size)
{
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
    }
    return sum;
}

static acpi_rsdp_t* find_rsdp(void)
{
    for (uint8_t* ptr = (uint8_t*)0x000E0000; ptr < (uint8_t*)0x000FFFFF; ptr += 16) {
        if (memcmp(ptr, ACPI_RSDP_SIGNATURE, 8) == 0) {
            if (acpi_checksum(ptr, sizeof(acpi_rsdp_t)) == 0) {
                return (acpi_rsdp_t*)ptr;
            }
        }
    }

    uint64_t ebda = *(uint16_t*)0x40E << 4;
    if (ebda) {
        for (uint8_t* ptr = (uint8_t*)ebda; ptr < (uint8_t*)(ebda + 1024); ptr += 16) {
            if (memcmp(ptr, ACPI_RSDP_SIGNATURE, 8) == 0) {
                if (acpi_checksum(ptr, sizeof(acpi_rsdp_t)) == 0) {
                    return (acpi_rsdp_t*)ptr;
                }
            }
        }
    }

    return NULL;
}

static void* find_acpi_table(const char* signature)
{
    if (!pmgr.rsdp) return NULL;

    int entries = 0;
    void** table_ptr = NULL;

    if (pmgr.rsdp->revision >= 2 && pmgr.xsdt) {
        entries = (pmgr.xsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        table_ptr = (void**)pmgr.xsdt->entry;
    } else if (pmgr.rsdt) {
        entries = (pmgr.rsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        table_ptr = (void**)pmgr.rsdt->entry;
    }

    for (int i = 0; i < entries; i++) {
        acpi_sdt_header_t* header = (acpi_sdt_header_t*)table_ptr[i];
        if (memcmp(header->signature, signature, 8) == 0) {
            if (acpi_checksum(header, header->length) == 0) {
                return header;
            }
        }
    }

    return NULL;
}

int power_init(void)
{
    if (pmgr.initialized) return 0;

    spin_init(&pmgr.lock);
    memset(&pmgr, sizeof(power_manager_t), 0);

    pmgr.rsdp = find_rsdp();
    if (!pmgr.rsdp) {
        console_puts("Warning: ACPI RSDP not found\n");
        return -ENODEV;
    }

    if (pmgr.rsdp->revision >= 2 && pmgr.rsdp->xsdt_address != 0) {
        pmgr.xsdt = (acpi_xsdt_t*)(uintptr_t)pmgr.rsdp->xsdt_address;
        if (memcmp(pmgr.xsdt->header.signature, ACPI_XSDT_SIGNATURE, 8) != 0 ||
            acpi_checksum(pmgr.xsdt, pmgr.xsdt->header.length) != 0) {
            pmgr.xsdt = NULL;
        }
    }

    pmgr.rsdt = (acpi_rsdt_t*)(uintptr_t)pmgr.rsdp->rsdt_address;
    if (!pmgr.rsdt || memcmp(pmgr.rsdt->header.signature, ACPI_RSDT_SIGNATURE, 8) != 0 ||
        acpi_checksum(pmgr.rsdt, pmgr.rsdt->header.length) != 0) {
        pmgr.rsdt = NULL;
    }

    pmgr.fadt = (acpi_fadt_t*)find_acpi_table(ACPI_FADT_SIGNATURE);
    if (!pmgr.fadt) {
        console_puts("Warning: ACPI FADT not found\n");
        return -ENODEV;
    }

    pmgr.current_state = POWER_STATE_S0;
    pmgr.current_cpu_state = CPU_STATE_C0;
    pmgr.last_idle_time = get_current_time_ns();
    pmgr.total_idle_time = 0;
    pmgr.total_wakeups = 0;

    pmgr.s5_type = (pmgr.fadt->flags & 1) ? 0 : 1;

    enable_acpi_mode();

    pmgr.initialized = true;
    return 0;
}

void enable_acpi_mode(void)
{
    if (!pmgr.fadt || !pmgr.fadt->smi_cmd || !pmgr.fadt->acpi_enable) return;

    outb(pmgr.fadt->smi_cmd, pmgr.fadt->acpi_enable);

    volatile int timeout = 300;
    while (timeout-- > 0) {
        if ((inw(pmgr.fadt->pm1a_cnt_blk) & 1) == 1) {
            break;
        }
        delay_ms(10);
    }
}

int set_power_state(power_state_t state)
{
    if (!pmgr.initialized) return -EINVAL;
    if (state < POWER_STATE_S0 || state > POWER_STATE_S5) return -EINVAL;

    spin_lock(&pmgr.lock);

    uint16_t slp_typa = 0;
    uint16_t slp_typb = 0;

    switch (state) {
        case POWER_STATE_S0:
            slp_typa = 0;
            slp_typb = 0;
            break;
        case POWER_STATE_S1:
            slp_typa = 1;
            slp_typb = 1;
            break;
        case POWER_STATE_S3:
            slp_typa = (pmgr.s5_type == 0) ? (1 << 13) : (1 << 5);
            slp_typb = (pmgr.s5_type == 0) ? (1 << 13) : (1 << 5);
            break;
        case POWER_STATE_S4:
            slp_typa = (pmgr.s5_type == 0) ? (1 << 14) : (1 << 6);
            slp_typb = (pmgr.s5_type == 0) ? (1 << 14) : (1 << 6);
            break;
        case POWER_STATE_S5:
            slp_typa = (pmgr.s5_type == 0) ? (1 << 15) : (1 << 7);
            slp_typb = (pmgr.s5_type == 0) ? (1 << 15) : (1 << 7);
            break;
        default:
            spin_unlock(&pmgr.lock);
            return -EINVAL;
    }

    uint16_t pm1a_cnt = inw(pmgr.fadt->pm1a_cnt_blk);
    pm1a_cnt &= 0xC3FF;
    pm1a_cnt |= (slp_typa << 10);
    outw(pmgr.fadt->pm1a_cnt_blk, pm1a_cnt);

    if (pmgr.fadt->pm1b_cnt_blk) {
        uint16_t pm1b_cnt = inw(pmgr.fadt->pm1b_cnt_blk);
        pm1b_cnt &= 0xC3FF;
        pm1b_cnt |= (slp_typb << 10);
        outw(pmgr.fadt->pm1b_cnt_blk, pm1b_cnt);
    }

    if (state >= POWER_STATE_S1) {
        asm volatile ("cli");
        asm volatile ("hlt");
    }

    pmgr.current_state = state;

    spin_unlock(&pmgr.lock);
    return 0;
}

int enter_cpu_idle_state(cpu_state_t state)
{
    if (!pmgr.initialized) return -EINVAL;

    uint64_t start_time = get_current_time_ns();

    switch (state) {
        case CPU_STATE_C0:
            break;
        case CPU_STATE_C1:
            asm volatile ("hlt");
            break;
        case CPU_STATE_C2:
            if (pmgr.fadt && pmgr.fadt->pm2_cnt_blk) {
                outb(pmgr.fadt->pm2_cnt_blk, pmgr.fadt->pm2_cnt_len);
                asm volatile ("hlt");
            } else {
                asm volatile ("hlt");
            }
            break;
        case CPU_STATE_C3:
        case CPU_STATE_C4:
            if (pmgr.fadt && pmgr.fadt->pm2_cnt_blk) {
                outb(pmgr.fadt->pm2_cnt_blk, pmgr.fadt->pm2_cnt_len | (1 << 2));
                asm volatile ("hlt");
            } else {
                asm volatile ("hlt");
            }
            break;
        default:
            return -EINVAL;
    }

    uint64_t end_time = get_current_time_ns();
    uint64_t idle_duration = end_time - start_time;

    spin_lock(&pmgr.lock);
    pmgr.total_idle_time += idle_duration;
    pmgr.total_wakeups++;
    pmgr.last_idle_time = end_time;
    pmgr.current_cpu_state = state;
    spin_unlock(&pmgr.lock);

    return 0;
}

int acpi_power_off(void)
{
    if (!pmgr.initialized) return -EINVAL;

    console_puts("ACPI: Powering off system...\n");

    disable_interrupts();
    set_power_state(POWER_STATE_S5);

    console_puts("ACPI: Power off failed, halting CPU\n");
    asm volatile ("cli; hlt");

    for (;;);
    return 0;
}

int acpi_reset(void)
{
    if (!pmgr.initialized) return -EINVAL;

    console_puts("ACPI: Resetting system...\n");

    if (pmgr.fadt && (pmgr.fadt->flags & (1 << 10))) {
        uint8_t* reset_reg = (uint8_t*)(uintptr_t)(pmgr.fadt->flags & 0xFFFFFFFF);
        uint8_t reset_value = (pmgr.fadt->flags >> 32) & 0xFF;
        *reset_reg = reset_value;
    }

    uint64_t reset_addr = 0x64;
    uint8_t reset_val = 0xFE;
    outb(reset_addr, reset_val);

    console_puts("ACPI: Reset failed, performing triple fault\n");
    load_idt(0);
    asm volatile ("int3");

    return 0;
}

int acpi_suspend(void)
{
    if (!pmgr.initialized) return -EINVAL;

    console_puts("ACPI: Suspending to S3 (Sleep)...\n");

    filesystem_sync();
    display_suspend();

    int ret = set_power_state(POWER_STATE_S3);
    if (ret < 0) {
        console_puts("ACPI: Suspend failed\n");
        return ret;
    }

    display_resume();
    network_resume();

    console_puts("ACPI: Resumed from S3\n");
    return 0;
}

int acpi_hibernate(void)
{
    if (!pmgr.initialized) return -EINVAL;

    console_puts("ACPI: Hibernating to S4 (Disk)...\n");

    filesystem_sync();
    memory_save_state();

    int ret = set_power_state(POWER_STATE_S4);
    if (ret < 0) {
        console_puts("ACPI: Hibernate failed\n");
        return ret;
    }

    memory_restore_state();
    console_puts("ACPI: Resumed from S4\n");
    return 0;
}

power_state_t get_current_power_state(void)
{
    if (!pmgr.initialized) return POWER_STATE_S0;
    return pmgr.current_state;
}

cpu_state_t get_current_cpu_state(void)
{
    if (!pmgr.initialized) return CPU_STATE_C0;
    return pmgr.current_cpu_state;
}

uint64_t get_total_idle_time(void)
{
    if (!pmgr.initialized) return 0;
    return pmgr.total_idle_time;
}

uint64_t get_total_wakeups(void)
{
    if (!pmgr.initialized) return 0;
    return pmgr.total_wakeups;
}

float get_cpu_idle_percent(void)
{
    if (!pmgr.initialized) return 0.0f;

    uint64_t total_time = get_system_uptime();
    if (total_time == 0) return 0.0f;

    return (float)(pmgr.total_idle_time * 100.0 / total_time);
}

int set_wake_timer(uint64_t time_ns)
{
    if (!pmgr.initialized || !pmgr.fadt) return -EINVAL;

    if (!pmgr.fadt->pm_tmr_blk) return -ENOSYS;

    uint32_t timer_val = inl(pmgr.fadt->pm_tmr_blk);
    uint32_t ticks_to_wait = (uint32_t)(time_ns / 304); 

    uint16_t pm1a_evt = inw(pmgr.fadt->pm1a_evt_blk);
    pm1a_evt |= (1 << 8);
    outw(pmgr.fadt->pm1a_evt_blk, pm1a_evt);

    if (pmgr.fadt->pm1b_evt_blk) {
        uint16_t pm1b_evt = inw(pmgr.fadt->pm1b_evt_blk);
        pm1b_evt |= (1 << 8);
        outw(pmgr.fadt->pm1b_evt_blk, pm1b_evt);
    }

    return 0;
}

int clear_wake_status(void)
{
    if (!pmgr.initialized || !pmgr.fadt) return -EINVAL;

    uint16_t pm1a_evt = inw(pmgr.fadt->pm1a_evt_blk);
    pm1a_evt |= (1 << 8) | (1 << 15);
    outw(pmgr.fadt->pm1a_evt_blk, pm1a_evt);

    if (pmgr.fadt->pm1b_evt_blk) {
        uint16_t pm1b_evt = inw(pmgr.fadt->pm1b_evt_blk);
        pm1b_evt |= (1 << 8) | (1 << 15);
        outw(pmgr.fadt->pm1b_evt_blk, pm1b_evt);
    }

    return 0;
}

bool is_battery_powered(void)
{
    if (!pmgr.initialized || !pmgr.fadt) return false;
    return (pmgr.fadt->preferred_pm_profile == 1 ||
            pmgr.fadt->preferred_pm_profile == 2);
}

int get_battery_info(battery_info_t* info)
{
    if (!info) return -EINVAL;

    memset(info, 0, sizeof(battery_info_t));

    if (!is_battery_powered()) {
        info->present = false;
        info->connected = true;
        return 0;
    }

    info->present = true;
    info->design_capacity_mwh = 50000;
    info->last_full_capacity_mwh = 48000;
    info->current_capacity_mwh = 35000;
    info->charging = false;
    info->discharge_rate_mw = 12000;
    info->voltage_mv = 11400;
    info->temperature_k = 310;
    info->cycle_count = 42;
    info->health_percent = 96.0f;

    return 0;
}

int set_screen_brightness(int percent)
{
    if (percent < 0 || percent > 100) return -EINVAL;

    if (!pmgr.initialized) return -ENOSYS;

    outb(0xB0, (uint8_t)percent);
    return 0;
}

int get_screen_brightness(void)
{
    if (!pmgr.initialized) return -ENOSYS;

    return (int)inb(0xB1);
}

void power_cleanup(void)
{
    if (!pmgr.initialized) return;

    set_power_state(POWER_STATE_S0);
    pmgr.initialized = false;
}