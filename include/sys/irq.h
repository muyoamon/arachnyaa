#ifndef ARACHNYAA_SYS_IRQ_H_
#define ARACHNYAA_SYS_IRQ_H_

#include "kernel/cap.h"

cap_handle_t sys_irq_claim(uint32_t irq_num);

int sys_irq_wait(cap_handle_t irq_cap);

/* Called from interrupt handler when hardware IRQ irq_num fires. */
void irq_cap_notify(uint32_t irq_num);

#endif // ARACHNYAA_SYS_IRQ_H_
