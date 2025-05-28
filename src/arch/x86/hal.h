#ifndef ARACHNYAA_HAL_H
#define ARACHNYAA_HAL_H

// --- PIC ---
#include <stdint.h>
void pic_remap(int offset1, int offset2);

void pic_send_eoi(uint8_t irq);

void pic_mask_irq(uint8_t irq);

void pic_unmask_irq(uint8_t irq);


#endif  // ARACHNYAA_HAL_H
