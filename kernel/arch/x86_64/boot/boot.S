%define MULTIBOOT_HEADER_MAGIC 0x1BADB002
%define MULTIBOOT_HEADER_FLAGS 0x0

section .text
global _start
extern kernel_main
_start:
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

    cli
    ; Debug: output 'B' to COM1
    mov dx, 0x3F8
    mov al, 'B'
    out dx, al
    mov rsp, stack_top
    ; Debug: output 'S' to COM1 (stack set)
    mov al, 'S'
    out dx, al
    call kernel_main
    ; Debug: output 'M' to COM1 (kernel_main returned - should not happen)
    mov dx, 0x3F8
    mov al, 'M'
    out dx, al

hlt_loop:
    hlt
    jmp hlt_loop

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
