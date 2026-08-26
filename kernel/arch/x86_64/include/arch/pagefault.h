

#ifndef ARCH_PAGEFAULT_H
#define ARCH_PAGEFAULT_H

#include <arch/types.h>

#define PF_PRESENT    (1ULL << 0)
#define PF_WRITE      (1ULL << 1)
#define PF_USER       (1ULL << 2)
#define PF_RESERVED   (1ULL << 3)
#define PF_FETCH      (1ULL << 4)
#define PF_PROTECT    (1ULL << 5)
#define PF_SGX        (1ULL << 15)

#define PAGE_COW_FLAG  (1ULL << 9)

typedef enum {
    PF_CAUSE_NOT_PRESENT = 0,
    PF_CAUSE_COW,
    PF_CAUSE_PROTECTION,
    PF_CAUSE_STACK_EXPAND,
    PF_CAUSE_KERNEL,
} pf_cause_t;

typedef struct {
    uint64_t fault_addr;
    uint64_t error_code;
    uint64_t rip;
    uint64_t rsp;
    uint64_t cr2;
    uint64_t cr3;
    uint32_t cpu;
    uint32_t pid;
    pf_cause_t cause;
} pf_context_t;

typedef struct {
    uint64_t total_faults;
    uint64_t cow_faults;
    uint64_t demand_paging;
    uint64_t stack_expands;
    uint64_t protection_faults;
    uint64_t kernel_faults;
} pf_stats_t;

void pagefault_init(void);

void pagefault_handler(uint64_t error_code, uint64_t rip);

int cow_mark_page(uint64_t pml4, uint64_t vaddr);
int cow_resolve_fault(uint64_t vaddr);

int demand_page_alloc(uint64_t pml4, uint64_t vaddr, uint64_t flags);

int stack_expand(uint64_t pml4, uint64_t fault_addr, uint64_t stack_bottom);

void pf_get_stats(pf_stats_t* stats);

#endif
