#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <arch/types.h>

#define PROCESS_MAX 64
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_READY = 0,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) process_context_t;

typedef struct process {
    uint64_t id;
    char name[32];
    process_state_t state;
    uint64_t priority;
    void* entry;
    void* stack;
    process_context_t context;
} process_t;

void process_init(void);
uint64_t process_create(const char* name, void* entry, uint64_t priority);
void process_exit(uint64_t code);
void process_yield(void);
void process_switch(process_context_t* old, process_context_t* new);
uint64_t process_get_current_id(void);

#endif
