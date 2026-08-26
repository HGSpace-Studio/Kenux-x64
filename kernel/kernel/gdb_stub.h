

#ifndef KERNEL_GDB_STUB_H
#define KERNEL_GDB_STUB_H

#include <arch/types.h>

typedef struct gdb_regs {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cs, ss, ds, es, fs, gs;
} gdb_regs_t;

void gdb_stub_init(void);

void gdb_stub_enter(gdb_regs_t* regs);

int gdb_set_breakpoint(uint64_t addr);
int gdb_clear_breakpoint(uint64_t addr);

#endif
