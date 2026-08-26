

#include <arch/pagefault.h>
#include <arch/memory.h>
#include <arch/buddy.h>
#include <arch/process.h>
#include <arch/spinlock.h>
#include <arch/smp.h>
#include <string.h>

extern void printk_err(const char* fmt, ...);
extern void printk_emerg(const char* fmt, ...);
extern void panic(const char* msg);

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void* alloc_pages_virt(uint32_t order);
extern void free_pages_virt(void* virt);
extern int pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);
extern void* pmap_get(void);
extern uint64_t pmap_get_physical(void* pml4, uint64_t vaddr);

static pf_stats_t pf_stats;
static spinlock_t pf_lock = SPINLOCK_INIT;

#define PF_LOG_BUF_SIZE  512
static char pf_log_buf[PF_LOG_BUF_SIZE];

void pagefault_init(void)
{
    memset(&pf_stats, 0, sizeof(pf_stats));
}

static inline uint64_t __read_cr2(void)
{
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline uint64_t __read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline int __is_user_addr(uint64_t vaddr)
{
    return (vaddr < 0x0000800000000000ULL);
}

static inline uint64_t __pte_flags(uint64_t pte)
{
    return pte & 0xFFF0000000000FFFULL;
}

static inline void __pte_set_flags(uint64_t* pte, uint64_t flags)
{
    *pte = (*pte & ~0xFFF0000000000FFFULL) | (flags & 0xFFF0000000000FFFULL);
}

int cow_mark_page(uint64_t pml4, uint64_t vaddr)
{

    uint64_t* pml4e = (uint64_t*)(pml4 + ((vaddr >> 39) & 0x1FF) * 8);
    if (!(*pml4e & 1)) return -1;
    uint64_t* pdpte = (uint64_t*)((*pml4e & ~0xFFFULL) + ((vaddr >> 30) & 0x1FF) * 8);
    if (!(*pdpte & 1)) return -1;
    uint64_t* pde = (uint64_t*)((*pdpte & ~0xFFFULL) + ((vaddr >> 21) & 0x1FF) * 8);
    if (!(*pde & 1)) return -1;
    uint64_t* pte = (uint64_t*)((*pde & ~0xFFFULL) + ((vaddr >> 12) & 0x1FF) * 8);
    if (!(*pte & 1)) return -1;

    *pte &= ~(uint64_t)PAGE_WRITABLE;
    *pte |= PAGE_COW_FLAG;

    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

int cow_resolve_fault(uint64_t vaddr)
{
    process_t* proc = &processes[current_process];
    void* pml4 = (void*)(uint64_t)__read_cr3();

    uint64_t* pml4e = (uint64_t*)((uint64_t)pml4 + ((vaddr >> 39) & 0x1FF) * 8);
    if (!(*pml4e & 1)) return -1;
    uint64_t* pdpte = (uint64_t*)((*pml4e & ~0xFFFULL) + ((vaddr >> 30) & 0x1FF) * 8);
    if (!(*pdpte & 1)) return -1;
    uint64_t* pde = (uint64_t*)((*pdpte & ~0xFFFULL) + ((vaddr >> 21) & 0x1FF) * 8);
    if (!(*pde & 1)) return -1;
    uint64_t* pte = (uint64_t*)((*pde & ~0xFFFULL) + ((vaddr >> 12) & 0x1FF) * 8);

    uint64_t old_pte = *pte;
    if (!(old_pte & PAGE_PRESENT)) return -1;
    if (!(old_pte & PAGE_COW_FLAG)) return -1;

    /* 修复: alloc_pages_virt 返回虚拟地址, 不能直接当物理地址写入 PTE.
       在 identity-mapped 低区, 虚拟地址==物理地址, 但一旦打破 identity 映射
       就会崩溃. 这里用 page_to_virt/pfn 模型: alloc 拿到 page, pfn 即物理页号. */
    extern page_t* alloc_pages(uint32_t order);
    extern uint64_t page_to_pfn(const page_t* page);
    page_t* new_page_desc = alloc_pages(0);
    if (!new_page_desc) return -1;
    uint64_t new_paddr = page_to_pfn(new_page_desc) * PAGE_SIZE;

    uint64_t old_paddr = old_pte & ~0xFFFULL;
    /* 用物理地址做 memcpy 会触发缺页, 应通过临时映射或 identity 区访问.
       在内核低半区 identity mapping 下, old_paddr 可直接解引用. */
    memcpy((void*)new_paddr, (void*)old_paddr, PAGE_SIZE);

    *pte = new_paddr | (old_pte & 0xFFFULL);
    *pte |= PAGE_WRITABLE;
    *pte &= ~PAGE_COW_FLAG;

    if (proc) {
        proc->rss += PAGE_SIZE;
    }

    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

int demand_page_alloc(uint64_t pml4, uint64_t vaddr, uint64_t flags)
{
    (void)pml4;
    /* 修复: 用 page_to_pfn 获取物理地址, 不能把虚拟地址当物理地址. */
    extern page_t* alloc_pages(uint32_t order);
    extern uint64_t page_to_pfn(const page_t* page);
    page_t* page_desc = alloc_pages(0);
    if (!page_desc) return -1;

    uint64_t paddr = page_to_pfn(page_desc) * PAGE_SIZE;
    memset((void*)paddr, 0, PAGE_SIZE);
    flags |= PAGE_PRESENT;

    if (pmap_map_page((void*)(uint64_t)__read_cr3(), vaddr, paddr, flags) != 0) {
        extern void free_pages(page_t* page);
        free_pages(page_desc);
        return -1;
    }

    return 0;
}

int stack_expand(uint64_t pml4, uint64_t fault_addr, uint64_t stack_bottom)
{

    #define USER_STACK_TOP  0x00007FFFFFFFFFFFULL
    #define MAX_STACK_SIZE  (8 * 1024 * 1024)

    if (fault_addr > USER_STACK_TOP) return -1;
    if (USER_STACK_TOP - fault_addr > MAX_STACK_SIZE) return -1;
    if (fault_addr >= stack_bottom) return -1;

    uint64_t page_addr = fault_addr & ~(PAGE_SIZE - 1);
    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    /* 修复: 同上, 用物理页号而非虚拟地址. */
    extern page_t* alloc_pages(uint32_t order);
    extern uint64_t page_to_pfn(const page_t* page);
    extern void free_pages(page_t* page);
    page_t* page_desc = alloc_pages(0);
    if (!page_desc) return -1;
    uint64_t paddr = page_to_pfn(page_desc) * PAGE_SIZE;

    memset((void*)paddr, 0, PAGE_SIZE);

    if (pmap_map_page((void*)pml4, page_addr, paddr, flags) != 0) {
        free_pages(page_desc);
        return -1;
    }

    return 0;
}

void pagefault_handler(uint64_t error_code, uint64_t rip)
{
    uint64_t cr2 = __read_cr2();
    uint64_t cr3 = __read_cr3();
    pf_cause_t cause;
    int user = error_code & PF_USER;

    spin_lock(&pf_lock);
    pf_stats.total_faults++;

    if (!(error_code & PF_PRESENT)) {

        if (user) {

            process_t* proc = &processes[current_process];
            if (proc && cr2 > (uint64_t)proc->stack_top - MAX_STACK_SIZE &&
                cr2 <= 0x00007FFFFFFFFFFFULL) {
                cause = PF_CAUSE_STACK_EXPAND;
                pf_stats.stack_expands++;
            } else {
                cause = PF_CAUSE_NOT_PRESENT;
                pf_stats.demand_paging++;
            }
        } else {
            cause = PF_CAUSE_KERNEL;
            pf_stats.kernel_faults++;
        }
    } else {

        if (error_code & PF_WRITE) {

            cause = PF_CAUSE_COW;
            pf_stats.cow_faults++;
        } else {
            cause = PF_CAUSE_PROTECTION;
            pf_stats.protection_faults++;
        }
    }

    spin_unlock(&pf_lock);

    int handled = 0;

    switch (cause) {
        case PF_CAUSE_COW:
            if (cow_resolve_fault(cr2) == 0) {
                handled = 1;
            }
            break;

        case PF_CAUSE_NOT_PRESENT:
            if (user) {
                if (demand_page_alloc(cr3, cr2 & ~(PAGE_SIZE - 1),
                                       PAGE_PRESENT | PAGE_USER) == 0) {
                    handled = 1;
                }
            }
            break;

        case PF_CAUSE_STACK_EXPAND:
            {
                process_t* proc = &processes[current_process];
                if (proc) {
                    if (stack_expand(cr3, cr2, (uint64_t)proc->stack_base) == 0) {

                        proc->stack_base = (void*)(cr2 & ~(PAGE_SIZE - 1));
                        handled = 1;
                    }
                }
            }
            break;

        case PF_CAUSE_KERNEL:

            break;

        case PF_CAUSE_PROTECTION:

            break;
    }

    if (!handled) {

        if (user) {

            printk_err("Page fault at %p, rip=%p, error=%p, pid=%lu - killing process\n",
                       (void*)cr2, (void*)rip, (void*)error_code,
                       processes[current_process].id);
            processes[current_process].state = PROCESS_DEAD;
        } else {

            printk_emerg("KERNEL PAGE FAULT at %p, rip=%p, error=%p\n",
                         (void*)cr2, (void*)rip, (void*)error_code);
            panic("kernel page fault");
        }
    }
}

void pf_get_stats(pf_stats_t* stats)
{
    spin_lock(&pf_lock);
    if (stats) *stats = pf_stats;
    spin_unlock(&pf_lock);
}
