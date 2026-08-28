#include <arch/usermode.h>
#include <arch/memory.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/syscall.h>
#include <arch/elf.h>
#include <arch/spinlock.h>
#include <arch/process.h>
#include <kernel/syscall.h>
#include <string.h>
#include <arch/vga.h>

#define USER_PROCESS_MAX 64
#define USER_STACK_SIZE 0x200000
#define USER_STACK_TOP  0x00007FFFFFFFFFFFULL
#define USER_HEAP_START 0x000000400000ULL
#define USER_HEAP_END   0x000000600000ULL

static user_process_t processes[USER_PROCESS_MAX];
static uint64_t next_pid = 1;
static spinlock_t process_lock = SPINLOCK_INIT;
static uint64_t current_user_pid = 0;

static inline uint64_t atomic_inc(uint64_t* val) {
    uint64_t old, new_val;
    do {
        old = *val;
        new_val = old + 1;
    } while (__sync_val_compare_and_swap(val, old, new_val) != old);
    return new_val;
}

void usermode_init(void)
{
    memset(processes, 0, sizeof(processes));
    next_pid = 1;
    current_user_pid = 0;
    spin_init(&process_lock);
    gdt_init();
    syscall_init();
}

static void* __create_user_page_table(void)
{
    void* pml4 = memory_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!pml4) return NULL;
    memset(pml4, 0, PAGE_SIZE);
    
    uint64_t* kernel_pml4 = (uint64_t*)pmap_get();
    uint64_t* new_pml4 = (uint64_t*)pml4;
    
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }
    
    return pml4;
}

static int __map_user_memory(void* pml4, uint64_t vaddr, uint64_t size, uint64_t flags)
{
    uint64_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < page_count; i++) {
        uint64_t page_addr = memory_alloc_physical(1);
        if (!page_addr) return -1;
        
        if (pmap_map_page(pml4, vaddr + i * PAGE_SIZE, page_addr, flags) != 0) {
            return -1;
        }
    }
    return 0;
}

uint64_t usermode_create_process(const char* name, void* entry, uint64_t* pid)
{
    spin_lock(&process_lock);
    
    int slot = -1;
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (!processes[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        spin_unlock(&process_lock);
        return 0;
    }
    
    user_process_t* proc = &processes[slot];
    memset(proc, 0, sizeof(user_process_t));
    
    proc->used = 1;
    proc->pid = atomic_inc(&next_pid);
    proc->parent_pid = current_user_pid;
    proc->entry_point = (uint64_t)entry;
    proc->stack_top = USER_STACK_TOP;
    proc->heap_start = USER_HEAP_START;
    proc->heap_end = USER_HEAP_END;
    proc->state = 1;
    proc->priority = 2;
    proc->fd_count = 0;
    
    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    }
    
    proc->cr3 = (uint64_t)__create_user_page_table();
    if (!proc->cr3) {
        proc->used = 0;
        spin_unlock(&process_lock);
        return 0;
    }
    
    if (__map_user_memory((void*)proc->cr3, proc->heap_start,
                          proc->heap_end - proc->heap_start,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        proc->used = 0;
        spin_unlock(&process_lock);
        return 0;
    }
    
    uint64_t stack_bottom = proc->stack_top - USER_STACK_SIZE;
    if (__map_user_memory((void*)proc->cr3, stack_bottom, USER_STACK_SIZE,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        proc->used = 0;
        spin_unlock(&process_lock);
        return 0;
    }
    
    proc->fd_table[0] = 0;
    proc->fd_table[1] = 1;
    proc->fd_table[2] = 2;
    proc->fd_count = 3;
    
    if (pid) *pid = proc->pid;
    
    spin_unlock(&process_lock);
    return proc->pid;
}

uint64_t usermode_create_process_from_elf(const char* path, uint64_t* pid)
{
    elf_load_info_t info;
    if (elf_load_from_file(path, &info) != 0) {
        return 0;
    }
    
    spin_lock(&process_lock);
    
    int slot = -1;
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (!processes[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        spin_unlock(&process_lock);
        return 0;
    }
    
    user_process_t* proc = &processes[slot];
    memset(proc, 0, sizeof(user_process_t));
    
    proc->used = 1;
    proc->pid = atomic_inc(&next_pid);
    proc->parent_pid = current_user_pid;
    proc->entry_point = info.entry;
    proc->stack_top = info.stack_top;
    proc->heap_start = USER_HEAP_START;
    proc->heap_end = USER_HEAP_END;
    proc->cr3 = (uint64_t)info.pml4;
    proc->state = 1;
    proc->priority = 2;
    proc->fd_count = 3;
    proc->fd_table[0] = 0;
    proc->fd_table[1] = 1;
    proc->fd_table[2] = 2;
    
    if (pid) *pid = proc->pid;
    
    spin_unlock(&process_lock);
    return proc->pid;
}

static void __switch_to_user(user_process_t* proc)
{
    current_user_pid = proc->pid;
    
    void* old_pml4 = pmap_get();
    pmap_switch((void*)proc->cr3);
    
    uint64_t rsp = proc->stack_top;
    uint64_t rip = proc->entry_point;
    
    __asm__ volatile (
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "pushq $0x23\n\t"
        "pushq %0\n\t"
        "pushfq\n\t"
        "orq $0x200, (%%rsp)\n\t"
        "pushq $0x1b\n\t"
        "pushq %1\n\t"
        "mov %%rsp, %%rdi\n\t"
        "call syscall_entry_trampoline\n\t"
        : : "r"(rsp), "r"(rip) : "rax", "rdi", "memory"
    );
}

uint64_t usermode_switch_to_user(void* entry, void* stack)
{
    spin_lock(&process_lock);
    
    int slot = -1;
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (processes[i].used && processes[i].entry_point == (uint64_t)entry) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        slot = 0;
        if (!processes[slot].used) {
            processes[slot].used = 1;
            processes[slot].pid = atomic_inc(&next_pid);
            processes[slot].entry_point = (uint64_t)entry;
            processes[slot].stack_top = (uint64_t)stack;
            processes[slot].cr3 = (uint64_t)__create_user_page_table();
            processes[slot].state = 1;
        }
    }
    
    user_process_t* proc = &processes[slot];
    proc->stack_top = (uint64_t)stack;
    
    spin_unlock(&process_lock);
    
    __switch_to_user(proc);
    return 0;
}

void usermode_syscall_entry(void)
{
    __asm__ volatile (
        "pushq %rax\n\t"
        "pushq %rbx\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %rbp\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, %rdi\n\t"
        "call usermode_syscall_handler\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rbp\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rbx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "sysret"
    );
}

void usermode_page_fault_entry(void)
{
    __asm__ volatile (
        "pushq %rax\n\t"
        "pushq %rbx\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %rbp\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %cr2, %rdi\n\t"
        "movq 120(%rsp), %rsi\n\t"
        "movq %rsp, %rdx\n\t"
        "call usermode_page_fault_handler\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rbp\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rbx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "iretq"
    );
}

long sys_exit(long status) {
    spin_lock(&process_lock);
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (processes[i].used && processes[i].pid == current_user_pid) {
            processes[i].state = 0;
            processes[i].used = 0;
            break;
        }
    }
    spin_unlock(&process_lock);
    return 0;
}

long sys_getpid(void) {
    return current_user_pid;
}

long sys_getppid(void) {
    spin_lock(&process_lock);
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (processes[i].used && processes[i].pid == current_user_pid) {
            long ppid = processes[i].parent_pid;
            spin_unlock(&process_lock);
            return ppid;
        }
    }
    spin_unlock(&process_lock);
    return 0;
}

long sys_write(long fd, const void* buf, long count) {
    if (!buf) return -14;
    
    if (fd == 1 || fd == 2) {
        extern void vga_print(const char*);
        char tmp[512];
        size_t to_write = (size_t)count > 511 ? 511 : (size_t)count;
        memcpy(tmp, buf, to_write);
        tmp[to_write] = '\0';
        vga_print(tmp);
        return (long)to_write;
    }
    
    return -9;
}

long sys_read(long fd, void* buf, long count) {
    if (!buf) return -14;
    if (fd == 0) {
        return 0;
    }
    return -9;
}

long sys_open(const char* pathname, int flags, int mode) {
    (void)pathname; (void)flags; (void)mode;
    return -2;
}

long sys_close(long fd) {
    (void)fd;
    return 0;
}

long sys_brk(long addr) {
    return USER_HEAP_END;
}

long sys_mmap(long addr, long length, int prot, int flags, int fd, long offset) {
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    if (length <= 0) return -22;
    
    spin_lock(&process_lock);
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (processes[i].used && processes[i].pid == current_user_pid) {
            uint64_t base = processes[i].heap_end;
            processes[i].heap_end += (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            spin_unlock(&process_lock);
            return (long)base;
        }
    }
    spin_unlock(&process_lock);
    return -12;
}

long sys_clone(long flags, void* stack, long arg, long fn) {
    (void)flags; (void)stack; (void)arg; (void)fn;
    spin_lock(&process_lock);
    int slot = -1;
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (!processes[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        spin_unlock(&process_lock);
        return -11;
    }
    
    user_process_t* parent = NULL;
    for (int i = 0; i < USER_PROCESS_MAX; i++) {
        if (processes[i].used && processes[i].pid == current_user_pid) {
            parent = &processes[i];
            break;
        }
    }
    
    if (!parent) {
        spin_unlock(&process_lock);
        return -11;
    }
    
    user_process_t* child = &processes[slot];
    memset(child, 0, sizeof(user_process_t));
    child->used = 1;
    child->pid = atomic_inc(&next_pid);
    child->parent_pid = current_user_pid;
    child->entry_point = parent->entry_point;
    child->stack_top = parent->stack_top;
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->cr3 = (uint64_t)__create_user_page_table();
    child->state = 1;
    child->priority = parent->priority;
    child->fd_count = parent->fd_count;
    for (int j = 0; j < child->fd_count && j < 32; j++) {
        child->fd_table[j] = parent->fd_table[j];
    }
    
    spin_unlock(&process_lock);
    return (long)child->pid;
}

void usermode_syscall_handler(uint64_t* context)
{
    uint64_t syscall_num = context[0];
    uint64_t arg1 = context[1];
    uint64_t arg2 = context[2];
    uint64_t arg3 = context[3];
    uint64_t arg4 = context[4];
    uint64_t arg5 = context[5];
    uint64_t arg6 = context[6];
    
    long ret = 0;
    
    switch (syscall_num) {
        case 0:
            ret = sys_read((int)arg1, (void*)arg2, (long)arg3);
            break;
        case 1:
            ret = sys_write((int)arg1, (const void*)arg2, (long)arg3);
            break;
        case 2:
            ret = sys_open((const char*)arg1, (int)arg2, (int)arg3);
            break;
        case 3:
            ret = sys_close((int)arg1);
            break;
        case 12:
            ret = sys_brk((long)arg1);
            break;
        case 9:
            ret = sys_mmap((long)arg1, (long)arg2, (int)arg3, (int)arg4, (int)arg5, (long)arg6);
            break;
        case 57:
            ret = sys_clone((long)arg1, (void*)arg2, (long)arg3, (long)arg4);
            break;
        case 60:
            ret = sys_exit((long)arg1);
            break;
        case 39:
            ret = sys_getpid();
            break;
        case 61:
            ret = sys_getppid();
            break;
        case 231:
            ret = sys_exit(0);
            break;
        default:
            ret = -38;
            break;
    }
    
    context[0] = (uint64_t)ret;
}

void usermode_page_fault_handler(uint64_t fault_addr, uint64_t error_code, uint64_t* stack_frame)
{
    /* Page fault handler — advance RIP to skip the faulting instruction.
     *
     * Stack layout (from bottom):
     *   stack_frame[0..14]  = saved r15, r14, ..., rax (15 registers)
     *   stack_frame[15]     = error code (pushed by CPU)
     *   stack_frame[16]     = RIP (from iretq frame)
     *   stack_frame[17]     = CS
     *   stack_frame[18]     = RFLAGS
     *   stack_frame[19]     = RSP
     *   stack_frame[20]     = SS
     *
     * We advance RIP by a fixed amount to skip past the faulting instruction.
     * Most x86-64 memory access instructions are 2-8 bytes. We use 8 bytes
     * as a reasonable default. This may skip part of the next instruction,
     * but it prevents the system from hanging on unmapped memory accesses. */

    /* Output a marker so we can see page faults in serial log */
    __asm__ volatile ("outb %0, %1" : : "a"((char)'!'), "d"((unsigned short)0x3F8));

    /* Advance RIP by 8 bytes to skip the faulting instruction */
    stack_frame[16] += 8;
}