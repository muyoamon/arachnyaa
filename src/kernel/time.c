#include "arch/cpu.h"
#include "arch/irq.h"
#include <stdint.h>
#include <kernel/time.h>

static int time_slice = 0;

volatile int system_clock_hz;

uint32_t timer_get_ticks() {
  return system_ticks;
}

void timer_sleep_ticks(uint32_t ticks) {
  uint32_t eticks = system_ticks + ticks;
  while(system_ticks < eticks) {
    arch_cpu_idle();
  }
}

void timer_set_slice(int slice) {
  time_slice = slice;
}


void timer_isr_handler(void) {
  static int current_time_slice = 0;
  system_ticks++;

  if (--current_time_slice <= 0) {
    current_time_slice = time_slice;
    irq_set_flag(IRQ_NEED_RESCHED);
  }
}



