

#include <arch/types.h>
#include <arch/vga.h>
#include <string.h>
#include <kapi_kprobe.h>   /* kprobe_regs_t 定义 */

/* 内核符号表（由 kernel.c 或链接脚本提供）
 * 符号表是 ksym_entry_t 数组，按地址升序排列
 */
typedef struct {
    uint64_t addr;
    const char* name;
} ksym_entry_t;

uint64_t kernel_symbols_start[2] = {0, 0};
uint64_t kernel_symbols_end[1] = {0};

/* 串口输出（COM1）—— panic 时同步输出到串口便于调试 */
static void serial_putc(char c)
{
    __asm__ volatile ("outb %0, %1" : : "a"(c), "d"((unsigned short)0x3F8));
}

static void serial_puts(const char* s)
{
    while (*s) serial_putc(*s++);
}

/* 同时输出到 VGA 和串口 */
static void console_putc(char c)
{
    vga_putchar(c);
    serial_putc(c);
}

static void console_puts(const char* s)
{
    while (*s) console_putc(*s++);
}

/* 十六进制输出 */
static void console_hex64(uint64_t v)
{
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int n = (int)((v >> (i * 4)) & 0xF);
        buf[15 - i] = (n < 10) ? ('0' + n) : ('a' + n - 10);
    }
    buf[16] = '\0';
    console_puts(buf);
}

static void console_hex32(uint32_t v)
{
    char buf[9];
    for (int i = 7; i >= 0; i--) {
        int n = (int)((v >> (i * 4)) & 0xF);
        buf[7 - i] = (n < 10) ? ('0' + n) : ('a' + n - 10);
    }
    buf[8] = '\0';
    console_puts(buf);
}

static void console_dec(uint64_t v)
{
    char buf[24];
    int i = 0;
    if (v == 0) { console_putc('0'); return; }
    while (v > 0 && i < 23) {
        buf[i++] = '0' + (int)(v % 10);
        v /= 10;
    }
    while (i-- > 0) console_putc(buf[i]);
}

/* 注册内核符号表 */
void kernel_register_symbols(const void* start, const void* end)
{
    kernel_symbols_start[0] = (uint64_t)start;
    kernel_symbols_end[0] = (uint64_t)end;
}

/* 通过地址反查符号名（最近的前驱符号） */
const char* ksym_lookup_addr(uint64_t addr)
{
    if (!kernel_symbols_start[0] || kernel_symbols_start[0] == kernel_symbols_end[0]) {
        return NULL;
    }
    ksym_entry_t* sym = (ksym_entry_t*)kernel_symbols_start[0];
    ksym_entry_t* end = (ksym_entry_t*)kernel_symbols_end[0];
    const char* best = NULL;
    uint64_t best_addr = 0;

    while (sym < end) {
        if (sym->addr <= addr && sym->addr > best_addr && sym->name) {
            best_addr = sym->addr;
            best = sym->name;
        }
        sym++;
    }
    return best;
}

/* 通过符号名查找地址 */
void* ksym_lookup_name(const char* name)
{
    if (!name || !kernel_symbols_start[0] || kernel_symbols_start[0] == kernel_symbols_end[0]) {
        return NULL;
    }
    ksym_entry_t* sym = (ksym_entry_t*)kernel_symbols_start[0];
    ksym_entry_t* end = (ksym_entry_t*)kernel_symbols_end[0];
    while (sym < end) {
        if (sym->name && strcmp(sym->name, name) == 0) {
            return (void*)sym->addr;
        }
        sym++;
    }
    return NULL;
}

/* ===== 寄存器转储 ===== */

typedef struct {
    const char* name;
    uint64_t value;
} reg_dump_t;

static void dump_registers(kprobe_regs_t* regs)
{
    console_puts("\nRegisters:\n");

    reg_dump_t dump[] = {
        {"RAX", regs->rax}, {"RBX", regs->rbx}, {"RCX", regs->rcx}, {"RDX", regs->rdx},
        {"RSI", regs->rsi}, {"RDI", regs->rdi}, {"RBP", regs->rbp}, {"RSP", regs->rsp},
        {"R8 ", regs->r8},  {"R9 ", regs->r9},  {"R10", regs->r10}, {"R11", regs->r11},
        {"R12", regs->r12}, {"R13", regs->r13}, {"R14", regs->r14}, {"R15", regs->r15},
        {"RIP", regs->rip}, {"RFL", regs->rflags},
    };

    for (size_t i = 0; i < sizeof(dump) / sizeof(dump[0]); i++) {
        console_puts("  ");
        console_puts(dump[i].name);
        console_puts("=");
        console_hex64(dump[i].value);
        if ((i + 1) % 4 == 0) console_putc('\n');
        else console_puts("  ");
    }

    /* 控制寄存器 */
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("movq %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("movq %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("movq %%cr4, %0" : "=r"(cr4));

    console_puts("\n  CR0=");
    console_hex64(cr0);
    console_puts("  CR2=");
    console_hex64(cr2);
    console_puts("\n  CR3=");
    console_hex64(cr3);
    console_puts("  CR4=");
    console_hex64(cr4);
    console_putc('\n');
}

/* 仅用当前寄存器值构造 dump（无 ISR 上下文） */
static void dump_current_registers(void)
{
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags;

    __asm__ volatile (
        "movq %%rax, %0\n"
        "movq %%rbx, %1\n"
        "movq %%rcx, %2\n"
        "movq %%rdx, %3\n"
        "movq %%rsi, %4\n"
        "movq %%rdi, %5\n"
        "movq %%rbp, %6\n"
        "movq %%rsp, %7\n"
        "movq %%r8, %8\n"
        "movq %%r9, %9\n"
        "movq %%r10, %10\n"
        "movq %%r11, %11\n"
        "movq %%r12, %12\n"
        "movq %%r13, %13\n"
        "movq %%r14, %14\n"
        "movq %%r15, %15\n"
        : "=m"(rax), "=m"(rbx), "=m"(rcx), "=m"(rdx),
          "=m"(rsi), "=m"(rdi), "=m"(rbp), "=m"(rsp),
          "=m"(r8), "=m"(r9), "=m"(r10), "=m"(r11),
          "=m"(r12), "=m"(r13), "=m"(r14), "=m"(r15)
    );

    /* 获取 RIP（通过栈上返回地址）和 RFLAGS */
    __asm__ volatile ("pushfq\n popq %0" : "=r"(rflags));
    /* RIP 取调用者地址（近似） */
    __asm__ volatile ("leaq (%%rip), %0" : "=r"(rip));

    kprobe_regs_t regs;
    memset(&regs, 0, sizeof(regs));
    regs.rax = rax; regs.rbx = rbx; regs.rcx = rcx; regs.rdx = rdx;
    regs.rsi = rsi; regs.rdi = rdi; regs.rbp = rbp; regs.rsp = rsp;
    regs.r8 = r8; regs.r9 = r9; regs.r10 = r10; regs.r11 = r11;
    regs.r12 = r12; regs.r13 = r13; regs.r14 = r14; regs.r15 = r15;
    regs.rip = rip; regs.rflags = rflags;
    dump_registers(&regs);
}

/* 前向声明 kprobe_regs_t（这里复用 stacktrace 与 kprobe 一致的寄存器布局） */

/* ===== 调用栈回溯 ===== */

static void print_frame(uint64_t addr, int frame_no)
{
    const char* sym = ksym_lookup_addr(addr);

    console_puts("  [");
    console_dec(frame_no);
    console_puts("] ");
    console_hex64(addr);
    if (sym) {
        console_puts(" <");
        console_puts(sym);
        console_puts(">");
    }
    console_putc('\n');
}

void stack_trace(void)
{
    uint64_t* rbp;
    __asm__ volatile ("movq %%rbp, %0" : "=r"(rbp));

    console_puts("Call Trace:\n");

    int frame = 0;
    while (rbp && frame < 64) {
        uint64_t ret_addr = *(rbp + 1);
        if (!ret_addr) break;

        print_frame(ret_addr, frame);

        uint64_t next_rbp = *rbp;
        if (next_rbp <= (uint64_t)rbp) break;
        rbp = (uint64_t*)next_rbp;
        frame++;
    }
}

void dump_stack(void)
{
    stack_trace();
}

/* 带 ISR 寄存器上下文的 panic（用于 page fault / GP fault 等） */
void panic_with_regs(const char* msg, kprobe_regs_t* regs)
{
    __asm__ volatile ("cli");

    console_puts("\n\n");
    console_puts("!!! KERNEL PANIC !!!\n");
    console_puts("Message: ");
    console_puts(msg ? msg : "unknown");
    console_putc('\n');

    if (regs) {
        dump_registers(regs);
    } else {
        dump_current_registers();
    }

    console_putc('\n');
    stack_trace();

    console_puts("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic(const char* msg)
{
    panic_with_regs(msg, NULL);
}

void __assert_fail(const char* expr, const char* file, int line)
{
    char buf[256];
    int i = 0;
    const char* p = "Assertion failed: ";
    while (*p && i < 255) buf[i++] = *p++;
    p = expr;
    while (*p && i < 255) buf[i++] = *p++;
    p = " at ";
    while (*p && i < 255) buf[i++] = *p++;
    p = file;
    while (*p && i < 255) buf[i++] = *p++;
    if (i < 255) buf[i++] = ':';

    int l = line;
    char num[8];
    int n = 0;
    do {
        num[n++] = '0' + (l % 10);
        l /= 10;
    } while (l > 0);
    while (n-- > 0 && i < 255) buf[i++] = num[n];
    buf[i] = '\0';

    panic(buf);
}
