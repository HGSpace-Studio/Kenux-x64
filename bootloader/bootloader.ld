/*
 * KEGD UEFI Bootloader Linker Script
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(efi_main)

SECTIONS {
    . = 0;
    image_base = .;
    
    .eh_frame : { *(.eh_frame) }
    
    .reloc : {
        *( .reloc )
    }
    
    .text : {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
        *(.rodata)
        *(.rodata.*)
    }
    
    .rela.text : {
        *(.rela.text)
        *(.rela.text.*)
        *(.rela.gnu.linkonce.t.*)
    }
    
    .rela.data : {
        *(.rela.data)
        *(.rela.data.*)
        *(.rela.rodata)
        *(.rela.rodata.*)
    }
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.common)
    }
}
