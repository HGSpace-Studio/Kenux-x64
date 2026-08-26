OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

KERNEL_VMA    = 0x2000000;
KERNEL_PHYS   = 0x2000000;

MEMORY
{
    kernel (rwx) : ORIGIN = KERNEL_VMA, LENGTH = 64M
}

SECTIONS
{
    . = KERNEL_VMA;
    kernel_start = .;

    .text : AT(ADDR(.text) - KERNEL_VMA + KERNEL_PHYS)
    {
        *(.multiboot2)
        *(.text)
        *(.text.*)
        . = ALIGN(4096);
    } > kernel

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA + KERNEL_PHYS)
    {
        *(.rodata*)
        *(.rdata*)
        . = ALIGN(4096);
    } > kernel

    .data : AT(ADDR(.data) - KERNEL_VMA + KERNEL_PHYS)
    {
        *(.data*)
        . = ALIGN(4096);
    } > kernel

    .bss : AT(ADDR(.bss) - KERNEL_VMA + KERNEL_PHYS)
    {
        bss_start = .;
        *(COMMON)
        *(.bss*)
        *(.lbss*)
        . = ALIGN(4096);
        bss_end = .;
    } > kernel

    /DISCARD/ :
    {
        *(.pdata*)
        *(.xdata*)
        *(.rdata*)
        *(.reloc*)
    }

    kernel_end = .;
}
