; idt.s - Assembly helpers for IDT and ISR stubs (x86/NASM)

bits 32

global idt_load     ; Loads the IDT pointer
extern isr_handler  ; C handler for interrupts

; --- ISR Stubs ---
global isr_stub_common ; A common stub
extern isr_common_stub_handler ; C handler

%macro ISR_NOERRCODE 1
    global isr_stub_%1
    isr_stub_%1:
        cli            ; Disable interrupts
        push 0         ; Push a dummy error code (0)
        push %1        ; Push the interrupt number
        jmp isr_stub_common
%endmacro

%macro ISR_ERRCODE 1
    global isr_stub_%1
    isr_stub_%1:
        cli            ; Disable interrupts
        push %1        ; Push the interrupt number (error code is already there)
        jmp isr_stub_common
%endmacro

; Exceptions without error codes (0-7, 9, 15, 16, 18-31)
ISR_NOERRCODE 0   ; Divide by zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; NMI
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack-Segment Fault
ISR_ERRCODE   13  ; General Protection Fault
ISR_ERRCODE   14  ; Page Fault
ISR_NOERRCODE 15  ; Reserved
ISR_NOERRCODE 16  ; x87 Floating-Point Exception
ISR_ERRCODE   17  ; Alignment Check
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating-Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
; ... up to 31
ISR_NOERRCODE 32  ; IRQ0
ISR_NOERRCODE 33  ; IRQ1

; --- syscall
global isr_stub_syscall
extern syscall_dispatcher
isr_stub_syscall:
  push 0          ; error code 0 
  push 0x80       ; interrupt number 0x80 
 
  jmp isr_stub_common

; Common stub called by all ISRs
isr_stub_common:
    pusha             ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    push ds 
    push es 
    push fs 
    push gs
    
    mov ax, 0x10      ; Load kernel data segment (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_common_stub_handler ; Call our C handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa              ; Pop EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    add esp, 8        ; Clean up error code and interrupt number
    iret              ; Return from interrupt

; Loads the IDT Register (IDTR)
; C signature: void idt_load(uint32_t idt_ptr);
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
