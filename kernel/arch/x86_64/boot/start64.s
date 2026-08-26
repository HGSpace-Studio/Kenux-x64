section .text
bits 64

global _start
extern kernel_main

_start:
    ; bootloader (UEFI ms_abi) passes args in RCX, RDX
    ; Save them to memory first
    mov [fb_config], rcx
    mov [mem_map], rdx

    ; Set up kernel stack
    mov rsp, stack_top

    ; Reload args and set up for BOTH ABIs so it works regardless of
    ; whether kernel_main was compiled with SysV (RDI/RSI) or ms_abi (RCX/RDX)
    mov rdi, [fb_config]    ; SysV ABI: 1st arg in RDI
    mov rsi, [mem_map]      ; SysV ABI: 2nd arg in RSI
    mov rcx, [fb_config]    ; ms_abi:   1st arg in RCX
    mov rdx, [mem_map]      ; ms_abi:   2nd arg in RDX
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
