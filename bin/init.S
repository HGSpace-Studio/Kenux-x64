.section .text
.global _start
_start:
    movq $stack_top, %rsp
    
    movq $0x23, %ax
    movq %ax, %ds
    movq %ax, %es
    movq %ax, %fs
    movq %ax, %gs
    
    movq $0x1B, %ax
    movq %ax, %ss
    
    call init_main
    
.hlt_loop:
    hlt
    jmp .hlt_loop

.section .bss
.align 16
stack_bottom:
    .skip 32768
stack_top:
