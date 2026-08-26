#include <arch/gdt.h>
#include <arch/memory.h>
#include <string.h>

gdt_entry_t gdt[GDT_ENTRIES];
gdt_register_t gdt_register;

void gdt_init(void)
{
    gdt_register.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_register.base = (uint64_t)gdt;

    memset(gdt, 0, sizeof(gdt));

    gdt_set_gate(0, 0, 0, 0, 0);
    /* Code segments: flags=0xA0 (G=1, D=0, L=1 → 64-bit long mode) */
    gdt_set_gate(1, 0, 0xFFFFFFFFFFFF, 0x9A, 0xA0);  /* kernel code 64-bit */
    gdt_set_gate(2, 0, 0xFFFFFFFFFFFF, 0x92, 0xCF);  /* kernel data */
    gdt_set_gate(3, 0, 0xFFFFFFFFFFFF, 0xFA, 0xA0);  /* user code 64-bit */
    gdt_set_gate(4, 0, 0xFFFFFFFFFFFF, 0xF2, 0xCF);  /* user data */
    gdt_set_gate(5, 0, 0xFFFFFFFFFFFF, 0xFA, 0xA0);  /* user code 64-bit */
    gdt_set_gate(6, 0, 0xFFFFFFFFFFFF, 0xF2, 0xCF);  /* user data */

    gdt_flush((uint64_t)&gdt_register);
}

void gdt_set_gate(uint8_t index, uint64_t base, uint64_t limit, uint8_t access, uint8_t flags)
{
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;

    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].flags_limit = ((limit >> 16) & 0x0F) | (flags & 0xF0);

    gdt[index].access = access;
}

void gdt_flush(uint64_t addr)
{
    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :: "r" (addr) : "rax"
    );
    /* Reload CS via far return — push target CS:RIP and retf */
    __asm__ volatile (
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        ::: "rax"
    );
}
