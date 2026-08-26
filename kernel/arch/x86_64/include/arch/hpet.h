#ifndef ARCH_X86_64_HPET_H
#define ARCH_X86_64_HPET_H

#include <arch/types.h>

#define HPET_BASE_DEFAULT 0xFED00000ULL

#define HPET_GEN_CAP_ID      0x000
#define HPET_GEN_CONFIG      0x010
#define HPET_GEN_INT_STATUS  0x020
#define HPET_MAIN_COUNTER    0x0F0
#define HPET_TMRN_CONFIG(n)  (0x100 + (n) * 0x20)
#define HPET_TMRN_COMPARATOR(n) (0x108 + (n) * 0x20)

#define HPET_CFG_ENABLE      (1ULL << 0)
#define HPET_CFG_LEGACY_RT   (1ULL << 1)

#define HPET_TMR_CFG_INT_TYPE_LEVEL  (1ULL << 1)
#define HPET_TMR_CFG_INT_ENABLE      (1ULL << 2)
#define HPET_TMR_CFG_PERIODIC        (1ULL << 3)
#define HPET_TMR_CFG_PERIODIC_CAP    (1ULL << 4)
#define HPET_TMR_CFG_64BIT_CAP       (1ULL << 5)
#define HPET_TMR_CFG_VAL_SET         (1ULL << 6)

typedef struct {
    uint64_t gen_cap_id;
    uint64_t reserved0;
    uint64_t gen_config;
    uint64_t reserved1;
    uint64_t gen_int_status;
    uint64_t reserved2[25];
    uint64_t main_counter_value;
    uint64_t reserved3;
} hpet_regs_t;

void hpet_init(void);
uint64_t hpet_get_ticks(void);
uint64_t hpet_get_frequency(void);
uint64_t hpet_get_ns(void);
uint64_t hpet_get_boot_ns(void);
void hpet_sleep(uint64_t ms);
void hpet_nsleep(uint64_t ns);
void hpet_set_periodic(uint32_t hz);
void hpet_enable(void);
void hpet_disable(void);

#endif