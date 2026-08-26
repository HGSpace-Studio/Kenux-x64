; AP Trampoline - 应用处理器启动代码
; 此代码会被 NASM 编译为 flat binary (ap_trampoline.bin)，
; 然后通过 objcopy -I binary 嵌入到内核 .rodata 段中。
; 运行时 BSP 将其 memcpy 到物理地址 0x8000。
; AP 从实模式 0x0000:0x8000 (CS=0, IP=0x8000) 启动，对应物理地址 0x8000。
; 因此此文件使用 org 0x8000，所有内部符号地址都是相对 0x8000 的。

BITS 16
org 0x8000

; ========== 调试宏：写一个字符到串口 0x3F8 ==========
; 根据当前 BITS 模式选择寄存器宽度
%macro dbgch 1
%if __BITS__ == 64
    push    rax
    push    rcx
    push    rdx
    mov     al, %1
    mov     dx, 0x3F8
    out     dx, al
    pop     rdx
    pop     rcx
    pop     rax
%elif __BITS__ == 32
    push    eax
    push    ecx
    push    edx
    mov     al, %1
    mov     dx, 0x3F8
    out     dx, al
    pop     edx
    pop     ecx
    pop     eax
%else   ; 16-bit
    push    ax
    push    cx
    push    dx
    mov     al, %1
    mov     dx, 0x3F8
    out     dx, al
    pop     dx
    pop     cx
    pop     ax
%endif
%endmacro

global ap_trampoline_bin_start
global ap_trampoline_bin_end
ap_trampoline_bin_start:

; ============ 实模式入口 (物理地址 0x8000) ============
trampoline_start:
    dbgch   '1'         ; [1] = 实模式入口到达
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; 临时栈，位于 0x0000:0x7C00 (安全区域)

    ; 启用 A20 地址线（通过 8042 键盘控制器）
    call    enable_a20
    dbgch   'a'         ; [a] = A20 启用成功

    ; 加载临时 GDT (物理地址 = 0x8000 + offset)
    lgdt    [temp_gdt_ptr]
    dbgch   'g'         ; [g] = temp GDT 加载成功

    ; 进入保护模式：设置 CR0.PE
    mov     eax, cr0
    or      eax, 0x01
    mov     cr0, eax

    ; 长跳转刷新流水线，进入 32 位保护模式
    jmp     0x08:dword pm32_entry

; ============ 启用 A20 ============
enable_a20:
    in      al, 0x64
    test    al, 0x02
    jnz     enable_a20
    mov     al, 0xD1
    out     0x64, al
.a20_wait1:
    in      al, 0x64
    test    al, 0x02
    jnz     .a20_wait1
    mov     al, 0xDF
    out     0x60, al
.a20_wait2:
    in      al, 0x64
    test    al, 0x02
    jnz     .a20_wait2
    ret

; ============ 临时 GDT (位于 0x8000 + offset) ============
align 16, db 0
temp_gdt:
    ; NULL descriptor (0x00)
    dq 0x0000000000000000
    ; 32-bit code (0x08): base=0, limit=4GB, DPL0, P, S, Type=CodeXR, G=1, DB=1, L=0
    dq 0x00CF9A000000FFFF
    ; 32-bit data (0x10): base=0, limit=4GB, DPL0, P, S, Type=DataRW, G=1, DB=1
    dq 0x00CF92000000FFFF
    ; 64-bit code (0x18): base=0, limit=4GB, DPL0, P, S, Type=CodeXR, G=1, L=1, D=0
    dq 0x00AF9A000000FFFF
    ; 64-bit data (0x20): base=0, limit=4GB, DPL0, P, S, Type=DataRW, G=1
    dq 0x00AF92000000FFFF

temp_gdt_ptr:
    dw      temp_gdt_ptr - temp_gdt - 1   ; limit
    dd      0x8000 + (temp_gdt - trampoline_start)  ; 物理地址

; ============ 32 位保护模式 ============
BITS 32
pm32_entry:
    dbgch   'p'         ; [p] = 进入 32 位保护模式
    mov     ax, 0x10        ; 32-bit data selector
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, 0x7C00     ; 临时栈

    ; ---- 开启 PAE 模式 (CR4.PAE) ----
    mov     eax, cr4
    or      eax, (1 << 5)
    mov     cr4, eax
    dbgch   '4'         ; [4] = PAE 模式开启

    ; ---- 设置 PML4 基址到 CR3 ----
    ; 直接使用 BSP 的 CR3（写入 scratch 0x8F20），确保 AP 与 BSP 共享相同页表
    ; 这样 identity mapping 和内核高半区映射都在。
    mov     eax, dword [0x8F20]         ; CR3 低 4 字节 (物理地址 <= 4G，高 4B 为 0)
    mov     cr3, eax
    dbgch   'P'         ; [P] = CR3 (PML4) 加载完成

    ; ---- 开启 EFER.LME (长模式使能) ----
    mov     ecx, 0xC0000080  ; IA32_EFER MSR
    rdmsr
    or      eax, (1 << 8)   ; LME bit
    wrmsr
    dbgch   'e'         ; [e] = EFER.LME 设置完成

    ; ---- 开启分页 (CR0.PG) ----
    mov     eax, cr0
    or      eax, (1 << 31)
    mov     cr0, eax
    dbgch   'L'         ; [L] = 开分页 (CR0.PG)

    ; ---- 长跳转进入 64 位长模式 (使用 64-bit code selector = 0x18) ----
    jmp     dword 0x18:lm64_entry

; ============ 64 位长模式入口 ============
BITS 64
lm64_entry:
    dbgch   '6'         ; [6] = 进入 64 位长模式
    ; 加载 64 位数据段 selector = 0x20
    mov     ax, 0x20
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     ss, ax
    ; gs 将在 C 代码 smp_setup_percpu() 中设置

    ; ---- 加载内核真正的 GDT ----
    ; 由于使用 identity mapping，gdt_register 虚拟地址 = 物理地址
    ; 我们需要用绝对地址访问（这里先写一个简单的 movabs）
    ; 但为了不依赖链接，我们在 C 代码中把 gdt_register 的物理地址写入下面的插槽
    ; 这里使用一个固定位置的插槽：gdt_ptr_slot
    mov     rax, [gdt_reg_addr_slot]
    lgdt    [rax]
    dbgch   'G'         ; [G] = 内核 GDT 加载完成

    ; 重新加载 CS 为内核 code selector 0x08，通过远返回
    push    qword 0x08
    mov     rax, .reload_cs
    push    rax
    retfq
.reload_cs:
    dbgch   'C'         ; [C] = CS 切换到 kernel code 完成
    mov     ax, 0x10        ; kernel data selector
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     ss, ax

    ; ---- 加载内核真正的 IDT ----
    mov     rax, [idt_reg_addr_slot]
    lidt    [rax]
    dbgch   'I'         ; [I] = 内核 IDT 加载完成

    ; ---- 设置 SSE / SIMD (CR4.OSFXSR|OSXMMEXCPT, CR0.MP, ~CR0.EM) ----
    mov     rax, cr4
    or      rax, (3 << 9)       ; OSFXSR | OSXMMEXCPT
    mov     cr4, rax
    mov     rax, cr0
    and     rax, ~(1 << 2)      ; 清除 CR0.EM
    or      rax, (1 << 1)       ; CR0.MP
    mov     cr0, rax
    dbgch   'S'         ; [S] = SSE/CR0 setup 完成

    ; ---- 设置内核栈：从 slot 读取（BSP 填 TRAMPOLINE_SLOTS_BASE 处） ----
    mov     rax, qword [trampoline_slots_base + 0]   ; slot 0 = stack top
    mov     rsp, rax
    xor     rbp, rbp
    dbgch   'k'         ; [k] = 内核栈加载完成

    ; ---- 启动握手：写 ap_booted = 1，等待 BSP 回写 2 ----
    mov     dword [trampoline_slots_base + 16], 1    ; slot 2 = ap_booted ← 1
    dbgch   'H'         ; [H] = AP 写 handshake=1
.hshake_wait:
    mov     eax, dword [trampoline_slots_base + 16]
    cmp     eax, 2
    jne     .hshake_wait
    dbgch   'W'         ; [W] = AP 收到 BSP ack=2

    ; ---- 调用 C 入口点 smp_ap_main() ----
    mov     rax, qword [trampoline_slots_base + 8]    ; slot 1 = kernel_entry
    dbgch   'K'         ; [K] = 即将跳转到 smp_ap_main()
    call    rax

    ; 正常不会返回，万一返回就 hlt
.halt:
    cli
    hlt
    jmp     .halt

; ========================================================================
; 强制数据槽区位于 trampoline 页 (0x8000..0x8FFF) 最后 48 字节 (0x8FD0)
; TRAMPOLINE_SLOTS_BASE = 0x8000 + 4096 - 48 = 0x8FD0
; ========================================================================
trampoline_slots_base equ 0x8FD0
TRAMP_SLOT_SECTION_OFF equ (trampoline_slots_base - 0x8000)   ; 节内偏移 = 0xFD0

; 填充到节内偏移 0xFD0
align 1, db 0
times (TRAMP_SLOT_SECTION_OFF - ($ - $$)) db 0xCC   ; 填 INT3 到 slot 起始位置

; ============ 数据槽区：固定在 0x8FD0，共 48 字节 (6×8) ============
; 0x8FD0 + 0*8 = stack_top_slot       (slot 0)
; 0x8FD0 + 1*8 = kernel_entry_slot    (slot 1)
; 0x8FD0 + 2*8 = ap_booted_flag_slot  (slot 2, 低 4B 使用)
; 0x8FD0 + 3*8 = gdt_reg_addr_slot    (slot 3, → 指向 0x8F00  scratch GDT desc)
; 0x8FD0 + 4*8 = idt_reg_addr_slot    (slot 4, → 指向 0x8F10  scratch IDT desc)
; 0x8FD0 + 5*8 = pad_slot             (保留)
align 8, db 0
global ap_trampoline_data_slots
ap_trampoline_data_slots equ trampoline_slots_base
stack_top_slot       equ trampoline_slots_base + 0
kernel_entry_slot    equ trampoline_slots_base + 8
ap_booted_flag_slot  equ trampoline_slots_base + 16
gdt_reg_addr_slot    equ trampoline_slots_base + 24
idt_reg_addr_slot    equ trampoline_slots_base + 32
pad_slot             equ trampoline_slots_base + 40

; 填充到页末尾
align 4096, db 0
ap_trampoline_bin_end:
