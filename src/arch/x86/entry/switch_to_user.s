; src/arch/x86/entry/switch_to_user.s 
; extern void arch_enter_user_mode(uintptr_t entry, 
;                                 uintptr_t user_stack_top, uint16_t user_ds, uint16_t user_cs)

bits 32

global arch_enter_user_mode
arch_enter_user_mode:
  mov ax, [esp + 12]            ; user_ds    
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  pushfd
  or  dword [esp], 0x200
  popfd

  push dword [esp + 12]         ; SS = user_ds
  push dword [esp + 8 + 4]          ; ESP = user_stack_top
  pushfd
  push dword [esp + 16 + 12]         ; CS = user_cs
  push dword [esp + 20]          ; EIP = entry
  iretd
