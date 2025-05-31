; arachnyaa/src/arch/x86/tss_asm.s 
section .text exec align=4
global tss_flush
tss_flush:
  mov ax, [esp + 4]
  ltr ax
  ret
