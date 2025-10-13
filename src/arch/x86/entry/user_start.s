bits 32
align 4096
global _user_entry

_user_entry:
  ;
.hang:
  jmp .hang
