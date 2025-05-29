/*
 * linker.ld - Linker script for arachnyaa OS (x86)
 * Load the kernel at 1MB and sets up sections
 * */

ENTRY(_start)

SECTIONS
{
  . = 0x100000;
  _kernel_start = .;
  .multiboot ALIGN(4) :
  {
    *(.multiboot)
}
  .text.startup ALIGN(4K) :
  {
    *(.text.startup)
  }

  .text ALIGN(4K) :
  {
    *(.text)
  }

  .rodata ALIGN(4K) :
  {
    *(.rodata*)
  }

  .data ALIGN(4K) :
  {
    *(.data)
  }

  .bss ALIGN(4K) :
  {
    _bss_start = .;
    *(COMMON)
    *(.bss)
    _bss_end = .;
  }

  _kernel_end = ALIGN(4K);

  /DISCARD/ :
  {
    *(.comment)
    *(.eh_frame)
  }
}
