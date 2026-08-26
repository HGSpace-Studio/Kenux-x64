#ifndef ARCH_X86_64_ACPI_PM_H
#define ARCH_X86_64_ACPI_PM_H

#include <arch/types.h>

#define ACPI_S0_WORKING    0
#define ACPI_S1_STANDBY    1
#define ACPI_S3_SUSPEND    3
#define ACPI_S4_HIBERNATE  4
#define ACPI_S5_SHUTDOWN   5

#define ACPI_PM_STATE_NAMES { \
    "Working",             \
    "Standby",             \
    "...",                 \
    "Suspend-to-RAM",      \
    "Hibernate",           \
    "Power Off"            \
}

#define FACP_SIGNATURE  0x50434146
#define DSDT_SIGNATURE  0x54445344

typedef struct {

    uint32_t signature;
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  oem_id[6];
    uint8_t  oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;

    uint32_t facs_addr;
    uint32_t dsdt_addr;

    uint8_t  reserved1;

    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;

    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;

    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;

    uint16_t iapc_boot_arch;
    uint8_t  reserved2;
    uint8_t  flags;

    uint8_t  reset_reg_space_id;
    uint8_t  reset_reg_bit_width;
    uint8_t  reset_reg_bit_offset;
    uint8_t  reset_reg_access_size;
    uint64_t reset_reg_addr;
    uint8_t  reset_value;

    uint8_t  reserved3[3];
    uint8_t  x_pm1a_evt_blk;
    uint8_t  x_pm1b_evt_blk;

    uint16_t reserved4;
    uint64_t x_pm1a_cnt_blk;
    uint64_t x_pm1b_cnt_blk;
    uint64_t x_pm2_cnt_blk;
    uint64_t x_pm_tmr_blk;
    uint64_t x_gpe0_blk;
    uint64_t x_gpe1_blk;

    uint8_t  sleep_type_a;
    uint8_t  sleep_type_b;
    uint8_t  reserved5;
    uint8_t  sleep_type_c;
    uint8_t  sleep_type_d;
    uint8_t  reserved6;
    uint8_t  sleep_type_e;
    uint8_t  sleep_type_f;
    uint8_t  reserved7;
    uint8_t  sleep_type_g;
    uint8_t  sleep_type_h;
} __attribute__((packed)) acpi_pm_fadt_t;

#define ACPI_SLP_TYP_A(val)    (((val) >> 0) & 0x07)

#define ACPI_SLP_TYP_B(val)    (((val) >> 4) & 0x07)

#define ACPI_SLP_EN            (1U << 13)

#define ACPI_SCI_EN            (1U << 0)

#define ACPI_PM_TIMER_FREQ     3579545
#define ACPI_PM_TIMER_MAX      0x00FFFFFF

#define ACPI_FADT_RESET_SUPPORTED    (1U << 10)
#define ACPI_FADT_S4BIOS_REQ         (1U << 7)

#define ACPI_PM_OK              0
#define ACPI_PM_ERR_NO_FADT     (-1)
#define ACPI_PM_ERR_BAD_FADT    (-2)
#define ACPI_PM_ERR_NOT_SUP     (-3)

typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8,  r9,  r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cr0, cr2, cr3, cr4;
    uint64_t gdtr_base, idtr_base;
    uint16_t gdtr_limit, idtr_limit;
    uint64_t efer;
    uint64_t mxcsr;
    uint8_t  fxsave[512];
} acpi_pm_cpu_context_t;

int acpi_pm_init(void);

acpi_pm_fadt_t* acpi_pm_get_fadt(void);

int acpi_pm_suspend(void);

void acpi_pm_resume(void);

int acpi_pm_hibernate(void);

int acpi_pm_shutdown(void);

int acpi_pm_reset_system(void);

uint32_t acpi_pm_get_timer(void);

uint8_t acpi_pm_get_supported_states(void);

#endif
