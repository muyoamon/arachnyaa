// interrupts.c - C level interrupt handling for Arachnyaa (x86)

#include "arch/irq.h"
#include "hal.h"
#include "drivers/io.h"
#include "kernel/syscall.h"
#include "paging.h"
#include "kernel/time.h"
#include "drivers/tty.h"
#include "sys/irq.h"
#include <drivers/keyboard.h>
#include <stdint.h>
#include "register.h"
// Forward declare a simple print function (from hal.c or tty.c)
void kprint(const char *str);
void kprint_hex(uint32_t n); // You'll need to implement this
void kprint_char(char c);

volatile uint32_t system_ticks = 0;

// A very simple struct to hold register values (passed from ASM)

const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",

    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",

    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",

    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Fault",

    "Virtualization Fault"
};

#define PIC_MASTER_CMD 0x20
#define PIC_SLAVE_CMD 0xA0
#define PIC_EOI 0x20



extern uint64_t syscall_dispatcher(uint32_t syscode, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);



// C handler called by the common ASM stub
// Note: The parameters are pushed on the stack in reverse order by the ASM stub
void isr_common_stub_handler(struct registers *regs) {

  // syscall
  if (regs->int_no == 0x80) {
    uint32_t syscall_num = regs->eax;


    uint64_t res = syscall_dispatcher(syscall_num, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
    // modified eax and edi saved in stack
    regs->eax = (uint32_t)(res & 0xFFFFFFFF);
    regs->edi = (uint32_t)(res >> 32);
    return;
  }

  if (regs->int_no >= 32 && regs->int_no <= 47) {
    uint32_t irq_num = regs->int_no - 32;
    switch (regs->int_no) {
    case 32:
      timer_isr_handler();
      break;
    case 33: {
      uint8_t scancode = inb(KEYBOARD_DATA_PORT);
      irq_cap_notify_data(1, scancode);
      if (!irq_has_notify_ep(1) && scancode)
        keyboard_handle_scancode(scancode);
      break;
    }
    default:
      irq_cap_notify(irq_num);
      break;
    }

    pic_send_eoi(irq_num);
    irq_exit_tail(regs);
    return;
  }

  switch (regs->int_no) {
  case 14: {
    return page_fault_handler(regs->err_code, regs->eip, regs->useresp,
                              regs->ebp, regs->eax, regs->edi, regs->esi, regs->ebx);
  }
  }

  kprint("!! Unhandled exception !! (");
  if (regs->int_no < 21) {
    kprint(exception_messages[regs->int_no]);
  } else {
    kprint("Unknown Interrupt");
  }
  kprint(") - Int: ");
  kprint_hex(regs->int_no);
  kprint(" Err: ");
  kprint_hex(regs->err_code);
  kprint("\nEIP: ");
  kprint_hex(regs->eip);
  kprint(" CS: ");
  kprint_hex(regs->cs);
  kprint(" EFLAGS: ");
  kprint_hex(regs->eflags);
  kprint("\nSystem Halted.\n");

  // Halt the system
  for (;;) {
    asm volatile("cli; hlt");
  }
}

// We will add IRQ handling (PIC, etc.) later.
