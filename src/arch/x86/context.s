section .text exec align=4
global arch_context_switch
bits 32

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

; void arch_context_switch(arch_context_t *old_ctx, arch_context_t *new_ctx);
arch_context_switch:
  push ebp 
  push ebx 
  push esi 
  push edi 

  ; get old_ctx pointer
  mov eax, [esp + 20]         ; eax = old_ctx pointer
  mov [eax + CTX_EBP], ebp
  mov [eax + CTX_EBX], ebx
  mov [eax + CTX_ESI], esi
  mov [eax + CTX_EDI], edi
  mov [eax + CTX_ESP], esp
  mov dword [eax + CTX_EIP], .return_here

  ; Get new_ctx pointer
  mov edx, [esp + 24]   ; ebx = new_ctx
  ; load next context
  mov esp, [edx + CTX_ESP]
  mov ebp, [edx + CTX_EBP]
  mov ebx, [edx + CTX_EBX]
  mov esi, [edx + CTX_ESI]
  mov edi, [edx + CTX_EDI]

  ; Jump to next->regs.eip
  jmp dword [edx + CTX_EIP]

.return_here:
  ; return to caller
  pop edi 
  pop esi 
  pop ebx 
  pop ebp
  ret


global arch_context_first_switch
bits 32

arch_context_first_switch:
  mov eax, [esp + 4]
  mov ebp, [eax + CTX_EBP]
  mov ebx, [eax + CTX_EBX]
  mov esi, [eax + CTX_ESI]
  mov edi, [eax + CTX_EDI]
  mov esp, [eax + CTX_ESP]
  jmp dword [eax + CTX_EIP]
