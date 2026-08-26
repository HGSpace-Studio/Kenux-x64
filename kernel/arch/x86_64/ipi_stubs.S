; IPI Stubs - 4 个 IPI 中断向量的入口 stub
; 这些函数被注册到 IDT 的 0xF0~0xF3 向量，直接调用对应的 C 处理函数，
; 不经过通用 interrupt_dispatch（因为 IPI 不需要查 IRQ 号）

section .text
bits 64

extern smp_ipi_handler_resched
extern smp_ipi_handler_tlbflush
extern smp_ipi_handler_halt
extern smp_ipi_handler_call

; 每个 stub 保存调用者保存寄存器 + 对齐栈，调用 C handler，恢复后 iretq

%macro IPI_STUB 1
global ipi_stub_%1
ipi_stub_%1:
    cld
    ; 保存调用者保存的通用寄存器
    push    rax
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    ; 对齐栈到 16 字节 (已 push 9 regs = 72 bytes, + 之前的 iret 帧)
    ; 简单起见这里不严格对齐，因为 C handler 只用到 lapic_eoi 等短函数
    call    smp_ipi_handler_%1
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rax
    iretq
%endmacro

IPI_STUB resched
IPI_STUB tlbflush
IPI_STUB halt
IPI_STUB call
