#ifndef ARACHNYAA_ARCH_X86_TSS_H_
#define ARACHNYAA_ARCH_X86_TSS_H_

#include <stdint.h>
typedef struct tss_entry {
  uint32_t prev_tss; // Previous TSS link; 0 if no previous TSS
  uint32_t esp0;     // Stack pointer to load when switching to CPL 0
  uint32_t ss0;      // Stack segment to load when switching to CPL 0
  uint32_t esp1;     // Stack pointer for CPL 1 (unused by us)
  uint32_t ss1;      // Stack segment for CPL 1 (unused by us)
  uint32_t esp2;     // Stack pointer for CPL 2 (unused by us)
  uint32_t ss2;      // Stack segment for CPL 2 (unused by us)
  uint32_t cr3;      // Page directory base register (CR3)
  uint32_t eip;      // Instruction pointer
  uint32_t eflags;   // EFLAGS register
  uint32_t eax;      // General purpose registers
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp; // Stack pointer
  uint32_t ebp; // Base pointer
  uint32_t esi; // Source index
  uint32_t edi; // Destination index
  uint32_t es;  // Segment selectors
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;  // LDT segment selector
  uint16_t trap; // Trap bit (for hardware task switching, unused)
  uint16_t iomap_base; // I/O map base address (offset from TSS base, >= sizeof(TSS))
} __attribute__((packed)) tss_entry_t;

/**
 * @brief Initialize tss
 */
void tss_init(void);

/**
 * @brief Set esp0
 *
 * @param stack_top_phys address to set to.
 */
void tss_set_kernel_stack(uint32_t stack_top_phys);

#endif // ARACHNYAA_ARCH_X86_TSS_H_
