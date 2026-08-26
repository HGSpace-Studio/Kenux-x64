#ifndef ARCH_X86_64_USERMODE_H
#define ARCH_X86_64_USERMODE_H

#include <arch/types.h>

#define USER_MODE_CS 0x1B
#define KERNEL_MODE_CS 0x08
#define USER_MODE_DS 0x23
#define KERNEL_MODE_DS 0x10

#define USER_STACK_SIZE 0x10000
#define USER_HEAP_SIZE 0x100000

#define USER_FLAGS 0x200

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} user_context_t;

typedef struct {
    uint64_t entry_point;
    uint64_t stack_top;
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t cr3;
    uint64_t page_table;
} user_process_t;

void usermode_init(void);
uint64_t usermode_create_process(const char* name, void* entry, uint64_t* pid);
uint64_t usermode_switch_to_user(void* entry, void* stack);
void usermode_syscall_handler(uint64_t* context);
void usermode_page_fault_handler(uint64_t fault_addr, uint64_t error_code, uint64_t* stack_frame);

#endif
