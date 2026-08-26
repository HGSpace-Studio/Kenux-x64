/* Standard EFI Application Linker Script */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  . = 0x100000;  /* 1MB - typical EFI load address */
  ImageBase = .;
  
  /* .hash and/or .gnu.hash MUST come first! */
  .hash : { *(.hash) }
  .gnu.hash : { *(.gnu.hash) }
  
  . = ALIGN(4096);
  .eh_frame : { *(.eh_frame) }
  .gcc_except_table : { *(.gcc_except_table*) }
  
  . = ALIGN(4096);
  .text :
  {
   _text = .;
   *(.text)
   *(.text.*)
   *(.gnu.linkonce.t.*)
   *(.plt)
   . = ALIGN(16);
  }
  _etext = .;
  _text_size = _etext - _text;
  
  . = ALIGN(4096);
  .reloc :
  {
   KEEP (*(.reloc))
  }
  
  . = ALIGN(4096);
  .data :
  {
   *(.data)
   *(.data.*)
   *(.gnu.linkonce.d.*)
  }
  _edata = .;
  
  . = ALIGN(4096);
  .sdata :
  {
   *(.sdata)
   *(.sdata.*)
   *(.gnu.linkonce.s.*)
  }
  
  . = ALIGN(4096);
  .bss :
  {
   *(.bss)
   *(.bss.*)
   *(.gnu.linkonce.b.*)
   *(COMMON)
  }
  _end = .;
}