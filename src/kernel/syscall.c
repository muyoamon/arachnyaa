#include "kernel/syscall.h"
#include <stdint.h>

extern void arch_sys_exit(int);
extern uint64_t arch_sys_putc(char);
extern void arch_sys_write(const char*);


uint64_t syscall_dispatcher(uint32_t syscode, uint64_t a0, uint64_t a1, uint64_t a2) {
  switch (syscode) {
    case SYS_EXIT:
      arch_sys_exit((int)a0);
      return 0;
    case SYS_WRITE:
      arch_sys_write((const char*)(uintptr_t)a0);
      return 0;
    case SYS_PUTC:
      return arch_sys_putc(a0);
    default:
      return -1;
  }
  (void)a0;
  (void)a1;
  (void)a2;
}
