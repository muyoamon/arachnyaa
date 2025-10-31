#ifndef ARACHNYAA_TIME_H
#define ARACHNYAA_TIME_H

#include "arch/registers.h"
#include <stdint.h>

extern volatile uint32_t system_ticks;
extern volatile int system_clock_hz;

// initialize kernel time-related system
void timer_init_system(uint32_t frequency);

// get the current value of the system ticks counter
uint32_t timer_get_ticks(void);

// a simple blocking delay function;
void timer_sleep_ticks(uint32_t ticks);


void timer_set_slice(int slice);

// 
void timer_isr_handler(void);


#endif  // ARACHNYAA_TIME_H
