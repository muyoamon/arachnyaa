

#include "arch/irq.h"
#include "arch/cpu.h"
#include "arch/registers.h"
#include "process/scheduler.h"

static volatile irq_flags_t irq_flags;

void irq_set_flag(irq_flags_t flags) {
  irq_flags |= flags;
}

void irq_rm_flag(irq_flags_t flags) {
  irq_flags = ~(~irq_flags | flags);
}
void irq_clear_flag(void) {
  irq_flags = 0;
}

void irq_exit_tail(arch_registers_t *regs) {
  arch_local_irq_disable();
  (void)regs;
  if (irq_flags & IRQ_NEED_RESCHED) {
    irq_flags &= ~IRQ_NEED_RESCHED;
    scheduler_reschedule();
  }
  arch_local_irq_enable();
}

