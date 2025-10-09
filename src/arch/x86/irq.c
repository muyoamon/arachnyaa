#include "arch/irq.h"

void local_irq_enable(void) {
  asm volatile ("sti" ::: "memory");
}

void local_irq_disable(void) {
  asm volatile ("cli" ::: "memory");
}

void cpu_idle(void) {
  asm volatile ("sti; hlt" ::: "memory");
}
