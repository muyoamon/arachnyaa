/*
 * linker.ld - Linker script for arachnyaa OS (x86)
 * Load the kernel at 1MB and sets up sections
 * */

ENTRY(_start)

MEMORY {
  ram (rwx) : ORIGIN = 0x00100000, LENGTH = 16M
}

SECTIONS
{
  . = 0x00100000;
  _kernel_start = .;
  .multiboot ALIGN(4) :
  {
    *(.multiboot)
  }
  .text.startup ALIGN(4K) :
  {
    *(.text.startup)
  }

  . += 0xC0000000;
  .text ALIGN(4K) : AT (ADDR (.text) - 0xC0000000 + 0x00100000)
  {
    *(.text)
  }

  .rodata ALIGN(4K) : AT (ADDR (.rodata) - 0xC0000000 + 0x00100000)
  {
    *(.rodata*)
  }

  .data ALIGN(4K) : AT (ADDR (.data) - 0xC0000000 + 0x00100000)
  {
    *(.data)
  }

  .bss ALIGN(4K) : AT (ADDR (.bss) - 0xC0000000 + 0x00100000)
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
