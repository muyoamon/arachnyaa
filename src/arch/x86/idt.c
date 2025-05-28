// idt.c - IDT setup for Arachnyaa (x86)

#include <stdint.h>
#include <stddef.h> // For NULL
#include "hal.h"

// IDT entry structure
struct idt_entry_t {
    uint16_t base_low;     // Lower 16 bits of handler address
    uint16_t sel;          // Kernel segment selector
    uint8_t  always0;      // Must be 0
    uint8_t  flags;        // Flags (Type & Attributes)
    uint16_t base_high;    // Upper 16 bits of handler address
} __attribute__((packed));

// IDT pointer structure (for lidt)
struct idt_ptr_t {
    uint16_t limit;        // Size of IDT - 1
    uint32_t base;         // Address of IDT
} __attribute__((packed));

#define NUM_IDT_ENTRIES 256
struct idt_entry_t idt_entries[NUM_IDT_ENTRIES];
struct idt_ptr_t   idt_ptr;

// Declare the ISR stubs from idt.s
extern void isr_stub_0(); extern void isr_stub_1(); extern void isr_stub_2();
extern void isr_stub_3(); extern void isr_stub_4(); extern void isr_stub_5();
extern void isr_stub_6(); extern void isr_stub_7(); extern void isr_stub_8();
extern void isr_stub_9(); extern void isr_stub_10(); extern void isr_stub_11();
extern void isr_stub_12(); extern void isr_stub_13(); extern void isr_stub_14();
extern void isr_stub_15(); extern void isr_stub_16(); extern void isr_stub_17();
extern void isr_stub_18(); extern void isr_stub_19(); extern void isr_stub_20();

extern void isr_stub_32();
extern void isr_stub_33();
// ... Add more if needed, or use an array of pointers.

// External assembly function
extern void idt_load(uint32_t idt_ptr);

// Function to set an IDT entry
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel       = sel;
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags | 0x60; // Set DPL to 0 (kernel) for now
}

// C interrupt handler (called from idt.s)
void isr_common_stub_handler(uint32_t int_num, uint32_t err_code, uint32_t eip, uint32_t cs, uint32_t eflags);

// Function to install the IDT
void idt_install() {

  pic_remap(0x20, 0x28);

  idt_ptr.limit = sizeof(struct idt_entry_t) * NUM_IDT_ENTRIES - 1;
  idt_ptr.base  = (uintptr_t)&idt_entries;

  // Zero out the IDT
  for(size_t i = 0; i < NUM_IDT_ENTRIES; ++i) {
      idt_set_gate(i, 0, 0, 0); // Clear entry
  }
  

  idt_set_gate(0, (uintptr_t)isr_stub_0, 0x08, 0x8E);
  idt_set_gate(1, (uintptr_t)isr_stub_1, 0x08, 0x8E);
  idt_set_gate(2, (uintptr_t)isr_stub_2, 0x08, 0x8E);
  idt_set_gate(3, (uintptr_t)isr_stub_3, 0x08, 0x8E);
  idt_set_gate(4, (uintptr_t)isr_stub_4, 0x08, 0x8E);
  idt_set_gate(5, (uintptr_t)isr_stub_5, 0x08, 0x8E);
  idt_set_gate(6, (uintptr_t)isr_stub_6, 0x08, 0x8E);
  idt_set_gate(7, (uintptr_t)isr_stub_7, 0x08, 0x8E);
  idt_set_gate(8, (uintptr_t)isr_stub_8, 0x08, 0x8E);
  idt_set_gate(9, (uintptr_t)isr_stub_9, 0x08, 0x8E);
  idt_set_gate(10, (uintptr_t)isr_stub_10, 0x08, 0x8E);
  idt_set_gate(11, (uintptr_t)isr_stub_11, 0x08, 0x8E);
  idt_set_gate(12, (uintptr_t)isr_stub_12, 0x08, 0x8E);
  idt_set_gate(13, (uintptr_t)isr_stub_13, 0x08, 0x8E);
  idt_set_gate(14, (uintptr_t)isr_stub_14, 0x08, 0x8E);
  idt_set_gate(15, (uintptr_t)isr_stub_15, 0x08, 0x8E);
  idt_set_gate(16, (uintptr_t)isr_stub_16, 0x08, 0x8E);
  idt_set_gate(17, (uintptr_t)isr_stub_17, 0x08, 0x8E);
  idt_set_gate(18, (uintptr_t)isr_stub_18, 0x08, 0x8E);
  idt_set_gate(19, (uintptr_t)isr_stub_19, 0x08, 0x8E);
  idt_set_gate(20, (uintptr_t)isr_stub_20, 0x08, 0x8E);
  idt_set_gate(32, (uintptr_t)isr_stub_32, 0x08, 0x8E);
  idt_set_gate(33, (uintptr_t)isr_stub_33, 0x08, 0x8E);
  // Load the IDT
  idt_load((uintptr_t)&idt_ptr);

  pic_unmask_irq(0);
  pic_unmask_irq(1);
}
