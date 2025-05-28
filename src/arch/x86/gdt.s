; gdt.s - Assembly helpers for GDT management (x86/NASM)

bits 32

global gdt_load    ; Loads the GDT pointer
global gdt_flush   ; Flushes segment registers

; --- GDT Constants ---
GDT_CODE_SEG    equ 0x08
GDT_DATA_SEG    equ 0x10

; Loads the GDT Register (GDTR)
; C signature: void gdt_load(uint32_t gdt_ptr);
gdt_load:
    mov eax, [esp + 4]  ; Get the GDT pointer from the stack
    lgdt [eax]          ; Load the GDT register
    ret

; Flushes the segment registers after loading a new GDT
; This is done with a far jump to the code segment and reloading
; data segment registers.
; C signature: void gdt_flush(void);
gdt_flush:
    ; Reload CS register using a far jump
    jmp GDT_CODE_SEG:.flush  ; Jump to .flush label in code segment 0x08

.flush:
    ; Reload data segment registers
    mov ax, GDT_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
