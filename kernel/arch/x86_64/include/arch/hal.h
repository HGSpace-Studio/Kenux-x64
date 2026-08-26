

#ifndef ARCH_HAL_H
#define ARCH_HAL_H

#include <arch/types.h>

static inline void hal_irq_disable(void)
{
    __asm__ volatile ("cli");
}

static inline void hal_irq_enable(void)
{
    __asm__ volatile ("sti");
}

static inline uint64_t hal_irq_save(void)
{
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n"
        "pop %0\n"
        "cli\n"
        : "=r"(flags)
    );
    return flags;
}

static inline void hal_irq_restore(uint64_t flags)
{
    __asm__ volatile (
        "push %0\n"
        "popfq\n"
        :
        : "r"(flags)
        : "memory"
    );
}

static inline void hal_mb(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

static inline void hal_wmb(void)
{
    __asm__ volatile ("sfence" ::: "memory");
}

static inline void hal_rmb(void)
{
    __asm__ volatile ("lfence" ::: "memory");
}

static inline void hal_cpu_relax(void)
{
    __asm__ volatile ("pause");
}

static inline void hal_cpu_halt(void)
{
    __asm__ volatile ("hlt");
}

static inline void hal_cpu_stop(void)
{
    for (;;) {
        hal_irq_disable();
        hal_cpu_halt();
    }
}

#define HAL_PAGE_PRESENT    0x001
#define HAL_PAGE_WRITABLE   0x002
#define HAL_PAGE_USER       0x004
#define HAL_PAGE_WRITETHROUGH 0x008
#define HAL_PAGE_NOCACHE    0x010
#define HAL_PAGE_ACCESSED   0x020
#define HAL_PAGE_DIRTY      0x040
#define HAL_PAGE_HUGE       0x080
#define HAL_PAGE_GLOBAL     0x100
#define HAL_PAGE_NX         (1ULL << 63)

void*   hal_pgtbl_create(void);
int     hal_pgtbl_map(void* pgtbl, uint64_t vaddr, uint64_t paddr, uint64_t flags);
int     hal_pgtbl_unmap(void* pgtbl, uint64_t vaddr);
uint64_t hal_pgtbl_walk(void* pgtbl, uint64_t vaddr);
void    hal_pgtbl_switch(void* pgtbl);
void    hal_pgtbl_invalidate(uint64_t vaddr);
void    hal_pgtbl_invalidate_all(void);

void hal_context_switch(uint64_t* old_rsp, uint64_t new_rsp);
void hal_context_init(void* stack_top, void (*entry)(void), void* arg);

void hal_timer_setup(uint32_t freq_hz);
uint64_t hal_timer_get_ticks(void);

void hal_arch_init(void);
void hal_arch_early_init(void);

#endif
