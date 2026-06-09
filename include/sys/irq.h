#ifndef ARACHNYAA_SYS_IRQ_H_
#define ARACHNYAA_SYS_IRQ_H_

#include "kernel/cap.h"
#include <stdbool.h>
#include <stdint.h>

cap_handle_t sys_irq_claim(uint32_t irq_num);

int sys_irq_wait(cap_handle_t irq_cap);

/* Called from interrupt handler when hardware IRQ irq_num fires. */
void irq_cap_notify(uint32_t irq_num);

/* Called from interrupt handler with raw data (e.g. scancode). */
void irq_cap_notify_data(uint32_t irq_num, uint8_t data);

/* Register an endpoint to receive IPC notifications for irq_cap. */
int sys_irq_notify(cap_handle_t irq_cap, cap_handle_t ep_cap);

/* Returns true if an endpoint is registered to receive notifications for irq_num. */
bool irq_has_notify_ep(uint32_t irq_num);

#endif // ARACHNYAA_SYS_IRQ_H_
