#include "drivers/tty.h"
#include "process/thread.h"
#include <stdint.h>

uint64_t arch_sys_putc(char c) {
  tty_putc(c);
  return 0;
}

void arch_sys_exit(int code) {
  thread_exit(code);
}


void arch_sys_write(const char* str) {
  tty_writestring(str);
}
