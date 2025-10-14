bits 32
align 4096
global _user_entry

_user_entry:
  ; sysexit
  mov eax, 0
  xor ebx, ebx
  int 0x80
.hang:
  jmp .hang
