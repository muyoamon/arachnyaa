; boot.s - Entry point and Multiboot setup for Arachnyaa (x86/NASM)

; --- Multiboot Flags bits
MB_FLAG_ALIGN_MODULES equ (1 << 0)
MB_FLAG_MEMORY_INFO   equ (1 << 1)
MB_FLAG_MEMORY_MAP    equ (1 << 6)


; --- Multiboot Header Constants ---
MB_MAGIC        equ 0x1BADB002  ; Multiboot magic number
MB_FLAGS        equ MB_FLAG_ALIGN_MODULES | MB_FLAG_MEMORY_INFO 
;MB_FLAGS        equ 0x00000000
MB_CHECKSUM     equ -(MB_MAGIC + MB_FLAGS)

; --- GDT Constants (will match GDT in gdt.c) ---
GDT_CODE_SEG    equ 0x08
GDT_DATA_SEG    equ 0x10

; --- Kernel Stack ---
STACK_SIZE      equ 16384       ; 16KB stack

; --- Sections ---

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    ; ELF header info for GRUB - needed if MB_FLAGS has bit 16 set (0x10000)
    ; dd 0 ; header_addr (GRUB fills this)
    ; dd 0 ; load_addr (GRUB fills this)
    ; dd 0 ; load_end_addr (GRUB fills this)
    ; dd 0 ; bss_end_addr (GRUB fills this)
    ; dd _start ; entry_addr (GRUB fills this)

section .data align=16 
global gdt_start
gdt_start:
  ; Null Segment
  dq 0x0000000000000000

  ; Kernel Code Segment (0x08)
  ; Base=0, Limit=4GB, Access=0x9A, Gran=0xCF
  dw 0xFFFF   ; Limit (low)
  dw 0x0000   ; Base (low)
  db 0x00     ; Base (mid)
  db 0x9A     ; Access (P=1 DPL=0 S=1, Type=Code,R,A)
  db 0xCF     ; Granularity (G=1, D=1) + Limit (high)
  db 0x00     ; Base (high)

  ; Kernel Data Segment (0x10)
  ; Base=0, Limit=4GB, Access=0x92, Gran=0xCF
  dw 0xFFFF   ; Limit (low)
  dw 0x0000   ; Base (low)
  db 0x00     ; Base (mid)
  db 0x92     ; Access (P=1 DPL=0 S=1, Type=Data,W,A)
  db 0xCF     ; Granularity (G=1, D=1) + Limit (high)
  db 0x00     ; Base (high)

  ; User Code Segment (0x18 - DPL 3)
  ; Base=0, Limit=4G, Access0xFA (P=1,DPL=3,S=1,Type=Code,R,A), Gran=0xCF
  dw 0xFFFF   ; Limit (low)
  dw 0x0000   ; Base (low)
  db 0x00     ; Base (mid)
  db 0xFA     ; Access: Present, DPL=3, Code/Data, Type=Execute/Read
  db 0xCF     ; Granularity: 4KB pages, 32-bit default
  db 0x00     ; Base (high)

  ; User Data Segment (0x20 - DPL 3)
  ; Base=0, Limit=4G, Access0xFA (P=1,DPL=3,S=1,Type=Code,W,A), Gran=0xCF
  dw 0xFFFF   ; Limit (low)
  dw 0x0000   ; Base (low)
  db 0x00     ; Base (mid)
  db 0xF2     ; Access: Present, DPL=3, Code/Data, Type=Read/Write
  db 0xCF     ; Granularity: 4KB pages, 32-bit default
  db 0x00     ; Base (high)

  ; TSS Segment (0x28 - DPL 0)
  dw 0x0067   
  dw 0x0000 
  db 0x00 
  db 0x00 
  db 0x00 
  db 0x00


gdt_end:

; GDT Pointer structure (for lgdt)
gdt_ptr:
  dw gdt_end - gdt_start - 1  ; GDT Limit
  dd gdt_start                ; GDT Base

align 32
global pdpt
pdpt: dq 0x0000000000000000

section .text.startup exec align=4
bits 32 ; We are in 32-bit protected mode
global _start ; Make _start visible to the linker
extern kmain  ; Our C kernel entry point

_start:
  ; === physical memory setup ===
  cli
  mov esp, kernel_stack_top - 0xC0000000

  push eax

  call setup_temp_paging

  ; === Jump to Higher-Half Virtual Address ===
  lea eax, [higher_half_entry]
  jmp eax
  ; === set up temporary page tables ===
setup_temp_paging:
  push ebx
  
  mov edi, pdpt
  sub edi, 0xC0000000
  add edi, 0x100000
  mov ecx, 32/4 
  xor eax, eax
  rep stosd

  mov edi, pdir0
  sub edi, 0xC0000000
  add edi, 0x100000
  mov ecx, 4096/4 
  xor eax, eax
  rep stosd

  mov edi, ptable_low
  sub edi, 0xC0000000
  add edi, 0x100000
  mov ecx, 4096 / 4 
  xor eax, eax
  rep stosd

  mov edi, pdir3
  sub edi, 0xC0000000
  add edi, 0x100000
  mov ecx, 4096/4 
  xor eax, eax
  rep stosd

  mov edi, ptable_high
  sub edi, 0xC0000000
  add edi, 0x100000
  mov ecx, 4096 / 4 
  xor eax, eax
  rep stosd

  lea eax, [pdir0]
  or eax, 0x03
  sub eax, 0xC0000000
  add eax, 0x100000
  mov [pdpt + 0*8 - 0xC0000000 + 0x100000], eax

  lea eax, [pdir3]
  or eax, 0x03
  sub eax, 0xC0000000
  add eax, 0x100000
  mov [pdpt + 3*8 - 0xC0000000 + 0x100000], eax

  lea eax, [ptable_low]
  or eax, 0x03
  sub eax, 0xC0000000
  add eax, 0x100000
  mov [pdir0 + 0*8 - 0xC0000000 + 0x100000], eax

  lea eax, [ptable_high]
  or eax, 0x03
  sub eax, 0xC0000000
  add eax, 0x100000
  mov [pdir3 + 0*8 - 0xC0000000 + 0x100000], eax

  xor ecx, ecx
.map_loop:
  mov eax, ecx
  shl eax, 12
  or eax, 0x003
  mov [ptable_low + ecx * 8 - 0xC0000000 + 0x100000], eax
  add eax, 0x100000
  mov [ptable_high + ecx * 8 - 0xC0000000 + 0x100000], eax
  inc ecx
  cmp ecx, 512
  jl .map_loop

  ; --- map VGA video memory to last entry of high table (0xC03FF000)
  mov eax, 0xb8000
  or eax, 0x003
  mov [ptable_high + 511 * 8 - 0xC0000000 + 0x100000], eax
  
  ; --- map multiboot
  pop eax,
  and eax, 0xFFFFF000
  or eax, 0x003
  mov ecx, 510 * 8 
  mov [ptable_high + ecx - 0xC0000000 + 0x100000], eax
  

  lea eax, [pdpt]
  sub eax, 0xC0000000
  add eax, 0x100000
  mov cr3, eax

  mov eax, cr4
  or eax, 1 << 5
  mov cr4, eax

  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax

  ret
  
; === remove identity mapping after jump
 
section .text align=4 exec
higher_half_entry:
  ; === Now in Virtual Memory space ===
  
  pop eax

  ; === initialize GDT ===
  lgdt [gdt_ptr]  ; load GDT
  jmp 0x08:.load_segments ; Far jump to set CS to 0x08
.load_segments:
  push ax
  mov ax, 0x10    ; Set DS, SS, ES, FS, GS to 0x10 (data segment)
  mov ds, ax
  mov ss, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  pop ax
  
  ; === Set up the stack ===
  mov esp, kernel_stack_top ; Point ESP to the top of our stack
  mov ebp, esp

  ; --- Prepare for C environment ---
  ; Push Multiboot info and magic onto the C stack
  push 0xC01FE000
  push eax

  ; === Clean Up Identity Mapping ===
  ; call clear_identity_map
  
  ; Call C kernel main function
  call kmain

  ; If kmain returns (it shouldn't!), hang the system.
  cli ; Disable interrupts
.hang:
  hlt ; Halt the CPU
  jmp .hang
;
; global clear_identity_map
; clear_identity_map:
;   mov eax, 0 
;   mov dword [pdir0 + 0*8], eax
;   mov dword [pdir0 + 0*8 + 4], eax
;   mov dword [pdir0 + 1*8], eax
;   mov dword [pdir0 + 1*8 + 4], eax
;
;   mov ecx, 0
; .flush_loop:
;   invlpg [ecx]
;   add ecx, 0x1000 
;   cmp ecx, 0x00400000
;   jl .flush_loop
;
;   ret
;

section .bss
align 4096
pdir0: resq 4096  ; identity
align 4096
pdir3: resq 4096  ; higher half
align 4096
ptable_low: resq 4096
align 4096
ptable_high: resq 4096

align 16
kernel_stack_bottom:
    resb STACK_SIZE ; Reserve space for the stack
kernel_stack_top:
    ; This label points to the address *after* the reserved space,
    ; which is the top of the stack (stacks grow down).
