

#include <arch/acpi_pm.h>
#include <arch/acpi.h>
#include <arch/io.h>
#include <arch/system_sounds.h>
#include <arch/memory.h>
#include <string.h>

static acpi_pm_fadt_t* pm_fadt = NULL;

static acpi_pm_cpu_context_t pm_cpu_ctx;

static uint16_t pm_saved_cnt = 0;

#define FADT_OFF_SLP_TYP_A_S1     0x20
#define FADT_OFF_SLP_TYP_A_S3     0x22
#define FADT_OFF_SLP_TYP_A_S4     0x24
#define FADT_OFF_SLP_TYP_A_S5     0x26
#define FADT_OFF_PM1A_CNT          0x44
#define FADT_OFF_PM_TMR            0x54
#define FADT_OFF_RESET_REG         0x74
#define FADT_OFF_RESET_VALUE       0x80

static uint8_t fadt_read_byte(uint8_t* base, uint32_t offset)
{
    return base[offset];
}

static void acpi_pm_save_cpu_context(void)
{
    uint64_t tmp;

    __asm__ volatile ("mov %%rax, %0" : "=r"(pm_cpu_ctx.rax));
    __asm__ volatile ("mov %%rbx, %0" : "=r"(pm_cpu_ctx.rbx));
    __asm__ volatile ("mov %%rcx, %0" : "=r"(pm_cpu_ctx.rcx));
    __asm__ volatile ("mov %%rdx, %0" : "=r"(pm_cpu_ctx.rdx));
    __asm__ volatile ("mov %%rsi, %0" : "=r"(pm_cpu_ctx.rsi));
    __asm__ volatile ("mov %%rdi, %0" : "=r"(pm_cpu_ctx.rdi));
    __asm__ volatile ("mov %%rbp, %0" : "=r"(pm_cpu_ctx.rbp));
    __asm__ volatile ("mov %%rsp, %0" : "=r"(pm_cpu_ctx.rsp));
    __asm__ volatile ("mov %%r8,  %0" : "=r"(pm_cpu_ctx.r8));
    __asm__ volatile ("mov %%r9,  %0" : "=r"(pm_cpu_ctx.r9));
    __asm__ volatile ("mov %%r10, %0" : "=r"(pm_cpu_ctx.r10));
    __asm__ volatile ("mov %%r11, %0" : "=r"(pm_cpu_ctx.r11));
    __asm__ volatile ("mov %%r12, %0" : "=r"(pm_cpu_ctx.r12));
    __asm__ volatile ("mov %%r13, %0" : "=r"(pm_cpu_ctx.r13));
    __asm__ volatile ("mov %%r14, %0" : "=r"(pm_cpu_ctx.r14));
    __asm__ volatile ("mov %%r15, %0" : "=r"(pm_cpu_ctx.r15));

    __asm__ volatile ("lea 1f(%%rip), %0\n1:" : "=r"(pm_cpu_ctx.rip));

    __asm__ volatile ("pushfq; pop %0" : "=r"(pm_cpu_ctx.rflags));

    __asm__ volatile ("mov %%cr0, %0" : "=r"(tmp)); pm_cpu_ctx.cr0 = tmp;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(tmp)); pm_cpu_ctx.cr2 = tmp;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(tmp)); pm_cpu_ctx.cr3 = tmp;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(tmp)); pm_cpu_ctx.cr4 = tmp;

    __asm__ volatile ("rdmsr" : "=a"(tmp) : "c"(0xC0000080));
    pm_cpu_ctx.efer = tmp;

    __asm__ volatile ("stmxcsr %0" : "=m"(pm_cpu_ctx.mxcsr));

    __asm__ volatile ("fxsave %0" : "=m"(pm_cpu_ctx.fxsave));

    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdt_desc, idt_desc;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_desc));
    __asm__ volatile ("sidt %0" : "=m"(idt_desc));

    pm_cpu_ctx.gdtr_limit = gdt_desc.limit;
    pm_cpu_ctx.gdtr_base   = gdt_desc.base;
    pm_cpu_ctx.idtr_limit = idt_desc.limit;
    pm_cpu_ctx.idtr_base   = idt_desc.base;
}

static void acpi_pm_restore_cpu_context(void)
{

    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdt_desc, idt_desc;

    gdt_desc.limit = pm_cpu_ctx.gdtr_limit;
    gdt_desc.base  = pm_cpu_ctx.gdtr_base;
    idt_desc.limit = pm_cpu_ctx.idtr_limit;
    idt_desc.base  = pm_cpu_ctx.idtr_base;

    __asm__ volatile ("lgdt %0" :: "m"(gdt_desc));
    __asm__ volatile ("lidt %0" :: "m"(idt_desc));

    __asm__ volatile ("mov %0, %%cr4" :: "r"(pm_cpu_ctx.cr4));
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pm_cpu_ctx.cr3));
    __asm__ volatile ("mov %0, %%cr2" :: "r"(pm_cpu_ctx.cr2));
    __asm__ volatile ("mov %0, %%cr0" :: "r"(pm_cpu_ctx.cr0));

    __asm__ volatile ("wrmsr" :: "a"(pm_cpu_ctx.efer & 0xFFFFFFFF),
                                    "d"(pm_cpu_ctx.efer >> 32),
                                    "c"(0xC0000080));

    __asm__ volatile ("fxrstor %0" :: "m"(pm_cpu_ctx.fxsave));

    __asm__ volatile ("ldmxcsr %0" :: "m"(pm_cpu_ctx.mxcsr));

    __asm__ volatile ("mov %0, %%r15" :: "r"(pm_cpu_ctx.r15));
    __asm__ volatile ("mov %0, %%r14" :: "r"(pm_cpu_ctx.r14));
    __asm__ volatile ("mov %0, %%r13" :: "r"(pm_cpu_ctx.r13));
    __asm__ volatile ("mov %0, %%r12" :: "r"(pm_cpu_ctx.r12));
    __asm__ volatile ("mov %0, %%r11" :: "r"(pm_cpu_ctx.r11));
    __asm__ volatile ("mov %0, %%r10" :: "r"(pm_cpu_ctx.r10));
    __asm__ volatile ("mov %0, %%r9"  :: "r"(pm_cpu_ctx.r9));
    __asm__ volatile ("mov %0, %%r8"  :: "r"(pm_cpu_ctx.r8));
    __asm__ volatile ("mov %0, %%rbp" :: "r"(pm_cpu_ctx.rbp));
    __asm__ volatile ("mov %0, %%rsp" :: "r"(pm_cpu_ctx.rsp));
    __asm__ volatile ("mov %0, %%rdi" :: "r"(pm_cpu_ctx.rdi));
    __asm__ volatile ("mov %0, %%rsi" :: "r"(pm_cpu_ctx.rsi));

    __asm__ volatile ("push %0; popfq" :: "r"(pm_cpu_ctx.rflags));

    __asm__ volatile ("mov %0, %%rax" :: "r"(pm_cpu_ctx.rax));
    __asm__ volatile ("mov %0, %%rbx" :: "r"(pm_cpu_ctx.rbx));
    __asm__ volatile ("mov %0, %%rcx" :: "r"(pm_cpu_ctx.rcx));
    __asm__ volatile ("mov %0, %%rdx" :: "r"(pm_cpu_ctx.rdx));
    __asm__ volatile ("jmp *%0" :: "r"(pm_cpu_ctx.rip));
}

static void acpi_pm_write_sleep(uint16_t sleep_type)
{
    uint16_t cnt;
    uint32_t pm1_cnt_port;

    if (!pm_fadt) return;

    if (pm_fadt->x_pm1a_cnt_blk) {

        volatile uint16_t* reg = (volatile uint16_t*)(uint64_t)pm_fadt->x_pm1a_cnt_blk;
        cnt = *reg;
        cnt &= ~(0x070FU);
        cnt |= (sleep_type & 0x07);
        cnt |= ((sleep_type & 0x07) << 4);
        cnt |= ACPI_SLP_EN;
        *reg = cnt;
    } else if (pm_fadt->pm1a_cnt_blk) {

        pm1_cnt_port = pm_fadt->pm1a_cnt_blk;
        cnt = inw(pm1_cnt_port);
        cnt &= ~(0x070FU);
        cnt |= (sleep_type & 0x07);
        cnt |= ((sleep_type & 0x07) << 4);
        cnt |= ACPI_SLP_EN;
        outw(pm1_cnt_port, cnt);
    }
}

static uint16_t acpi_pm_get_slp_typ(uint8_t* fadt_raw, uint32_t offset)
{
    if (!fadt_raw) return 0;
    return (uint16_t)(fadt_raw[offset] & 0x07);
}

int acpi_pm_init(void)
{
    acpi_header_t* fadt_table;

    fadt_table = acpi_get_table(FACP_SIGNATURE);
    if (!fadt_table) {
        return ACPI_PM_ERR_NO_FADT;
    }

    if (fadt_table->length < 0x80) {
        return ACPI_PM_ERR_BAD_FADT;
    }

    memset(&pm_cpu_ctx, 0, sizeof(pm_cpu_ctx));

    pm_fadt = (acpi_pm_fadt_t*)fadt_table;

    {
        uint8_t* raw = (uint8_t*)fadt_table;

        pm_fadt->sleep_type_a = fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S1) & 0x07;
        pm_fadt->sleep_type_c = fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S3) & 0x07;
        pm_fadt->sleep_type_e = fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S4) & 0x07;
        pm_fadt->sleep_type_g = fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S5) & 0x07;
    }

    if (pm_fadt->pm1a_cnt_blk) {
        uint16_t cnt = inw(pm_fadt->pm1a_cnt_blk);
        if (!(cnt & ACPI_SCI_EN)) {
            cnt |= ACPI_SCI_EN;
            outw(pm_fadt->pm1a_cnt_blk, cnt);
        }
    }

    return ACPI_PM_OK;
}

acpi_pm_fadt_t* acpi_pm_get_fadt(void)
{
    return pm_fadt;
}

int acpi_pm_suspend(void)
{
    if (!pm_fadt) {
        return ACPI_PM_ERR_NO_FADT;
    }

    if (pm_fadt->sleep_type_c == 0) {
        return ACPI_PM_ERR_NOT_SUP;
    }

    acpi_pm_save_cpu_context();

    if (pm_fadt->pm1a_cnt_blk) {
        pm_saved_cnt = inw(pm_fadt->pm1a_cnt_blk);
    }

    __asm__ volatile ("cli");

    acpi_pm_write_sleep(pm_fadt->sleep_type_c);

    __asm__ volatile (
        "wbinvd\n"
        "hlt"
    );

    return ACPI_PM_OK;
}

void acpi_pm_resume(void)
{

    acpi_pm_restore_cpu_context();

    __asm__ volatile ("sti");
}

int acpi_pm_hibernate(void)
{
    if (!pm_fadt) {
        return ACPI_PM_ERR_NO_FADT;
    }

    if (pm_fadt->sleep_type_e == 0) {
        return ACPI_PM_ERR_NOT_SUP;
    }

    acpi_pm_save_cpu_context();

    __asm__ volatile ("cli");

    acpi_pm_write_sleep(pm_fadt->sleep_type_e);

    __asm__ volatile (
        "wbinvd\n"
        "hlt"
    );

    return ACPI_PM_OK;
}

int acpi_pm_shutdown(void)
{
    system_sound_play_shutdown();

    if (!pm_fadt) {
        return ACPI_PM_ERR_NO_FADT;
    }

    if (pm_fadt->sleep_type_g == 0) {
        return ACPI_PM_ERR_NOT_SUP;
    }

    __asm__ volatile ("cli");

    outw(0x604, 0x2000);

    acpi_pm_write_sleep(pm_fadt->sleep_type_g);

    __asm__ volatile ("cli; hlt");

    return ACPI_PM_OK;
}

int acpi_pm_reset_system(void)
{
    if (!pm_fadt) {
        return ACPI_PM_ERR_NO_FADT;
    }

    if (!(pm_fadt->flags & ACPI_FADT_RESET_SUPPORTED)) {

        __asm__ volatile ("cli");

        outb(0x64, 0xFE);

        __asm__ volatile ("hlt");
        return ACPI_PM_OK;
    }

    if (pm_fadt->reset_reg_space_id == 0x00) {

        outb((uint16_t)pm_fadt->reset_reg_addr, pm_fadt->reset_value);
    } else if (pm_fadt->reset_reg_space_id == 0x01) {

        volatile uint8_t* reg = (volatile uint8_t*)(uint64_t)pm_fadt->reset_reg_addr;
        *reg = pm_fadt->reset_value;
    }

    __asm__ volatile ("cli; hlt");

    return ACPI_PM_OK;
}

uint32_t acpi_pm_get_timer(void)
{
    uint32_t timer_val = 0;

    if (!pm_fadt) {
        return 0;
    }

    if (pm_fadt->x_pm_tmr_blk) {
        volatile uint32_t* reg = (volatile uint32_t*)(uint64_t)pm_fadt->x_pm_tmr_blk;
        timer_val = *reg & ACPI_PM_TIMER_MAX;
    } else if (pm_fadt->pm_tmr_blk) {

        timer_val = inl(pm_fadt->pm_tmr_blk) & ACPI_PM_TIMER_MAX;
    }

    return timer_val;
}

uint8_t acpi_pm_get_supported_states(void)
{
    uint8_t supported = 0;

    if (!pm_fadt) {
        return 0;
    }

    uint8_t* raw = (uint8_t*)pm_fadt;

    if (fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S1) & 0x07) {
        supported |= (1 << 0);
    }

    if (fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S3) & 0x07) {
        supported |= (1 << 1);
    }

    if (fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S4) & 0x07) {
        supported |= (1 << 2);
    }

    if (fadt_read_byte(raw, FADT_OFF_SLP_TYP_A_S5) & 0x07) {
        supported |= (1 << 3);
    }

    return supported;
}
