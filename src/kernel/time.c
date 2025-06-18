#include <stdint.h>
#include <kernel/time.h>

uint32_t timer_get_ticks() {
  return system_ticks;
}

void timer_sleep_ticks(uint32_t ticks) {
  uint32_t eticks = system_ticks + ticks;
  while(system_ticks < eticks) {
    asm volatile ("hlt");
  }
}

