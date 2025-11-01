#include "drivers/tty.h"
#include <stdint.h>
uint64_t arch_sys_putc(char c) {
  tty_putc(c);
  return 0;
}

void arch_sys_exit(int code) {
  asm volatile("cli");
  tty_writestring("exiting...\t(code:");
  tty_write_dec(code);
  tty_writestring(")\n");
  for (;;)
    asm volatile ("hlt");
}


void arch_sys_write(const char* str) {
  tty_writestring(str);
}
