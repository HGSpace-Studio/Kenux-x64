section .text
bits 64

global _start
extern kernel_main

_start:
    ; bootx64.c calls the kernel entry with the SysV ABI (RDI, RSI).
    ; Save the handoff pointers before replacing the kernel stack.
    mov [fb_config], rdi
    mov [mem_map], rsi

    ; Set up kernel stack
    mov rsp, stack_top

    ; Reload the handoff pointers for kernel_main's SysV ABI.
    mov rdi, [fb_config]    ; SysV ABI: 1st arg in RDI
    mov rsi, [mem_map]      ; SysV ABI: 2nd arg in RSI
    call kernel_main

    cli
.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:

section .data
fb_config:
    dq 0
mem_map:
    dq 0
