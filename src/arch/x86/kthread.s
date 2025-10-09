section .text exec align=4
global switch_to

%define CTX_EBP 0
%define CTX_EBX 4
%define CTX_ESI 8
%define CTX_EDI 12
%define CTX_EIP 16
%define CTX_ESP 20

; cdecl stack on entry
; [esp + 0] return address
; [esp + 4] prev
; [esp + 8] next

switch_to:
  push ebp 
  push ebx 
  push esi 
  push edi 

  mov eax, [esp + 20]   ; eax = prev
  mov edx, [esp + 24]   ; ebx = next
  mov ecx, [esp + 16]   ; ecx = return address

  mov [eax + CTX_EBP], ebp
  mov [eax + CTX_EBX], ebx
  mov [eax + CTX_ESI], esi
  mov [eax + CTX_EDI], edi
  mov [eax + CTX_EIP], ecx

  lea ecx, [esp + 20]   ; ESP if switch_to had returned
  mov [eax + CTX_ESP], ecx

  ; load next context
  mov esp, [edx + CTX_ESP]
  mov ebp, [edx + CTX_EBP]
  mov ebx, [edx + CTX_EBX]
  mov esi, [edx + CTX_ESI]
  mov edi, [edx + CTX_EDI]

  ; Jump to next->regs.eip
  jmp dword [edx + CTX_EIP]
