#ifndef ARACHNYAA_ARCH_IRQ_H_
#define ARACHNYAA_ARCH_IRQ_H_

#include "arch/registers.h"
typedef enum {
  IRQ_NEED_RESCHED = 1,
} irq_flags_t;

void irq_set_flag(irq_flags_t flags);

void irq_rm_flag(irq_flags_t flags);

void irq_clear_flag(void);

void irq_exit_tail(arch_registers_t *regs);

#endif // ARACHNYAA_ARCH_IRQ_H_
