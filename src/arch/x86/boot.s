; boot.s - Entry point and Multiboot setup for Arachnyaa (x86/NASM)

; --- Multiboot Header Constants ---
MB_MAGIC        equ 0x1BADB002  ; Multiboot magic number
MB_FLAGS        equ 0x00000000  ; Align modules, provide memory map, use ELF sections
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
gdt_end:

; GDT Pointer structure (for lgdt)
gdt_ptr:
  dw gdt_end - gdt_start - 1  ; GDT Limit
  dd gdt_start                ; GDT Base


section .text.startup exec align=4
bits 32 ; We are in 32-bit protected mode
global _start ; Make _start visible to the linker
extern kmain  ; Our C kernel entry point

_start:
    lgdt [gdt_ptr]  ; load GDT
    jmp 0x08:.load_segments ; Far jump to set CS to 0x08
.load_segments:
    mov ax, 0x10    ; Set DS, SS, ES, FS, GS to 0x10 (data segment)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Set up the stack
    mov esp, kernel_stack_top ; Point ESP to the top of our stack

    ; --- Prepare for C environment ---
    ; Push Multiboot info and magic onto the C stack
    mov eax, MB_MAGIC
    push ebx
    push eax

    ; Call C kernel main function
    call kmain

    ; If kmain returns (it shouldn't!), hang the system.
    cli ; Disable interrupts
.hang:
    hlt ; Halt the CPU
    jmp .hang

section .bss
align 16
kernel_stack_bottom:
    resb STACK_SIZE ; Reserve space for the stack
kernel_stack_top:
    ; This label points to the address *after* the reserved space,
    ; which is the top of the stack (stacks grow down).
