#include "kernel/syscall.h"
#include "sys/write.h"
#include <stdint.h>

extern void arch_sys_exit(int);
extern uint64_t arch_sys_putc(char);

typedef uintptr_t native_word;

uint64_t syscall_dispatcher(uint32_t syscode, native_word a0, native_word a1, native_word a2, 
                            native_word a3, native_word a4) {
  switch (syscode) {
    case SYS_EXIT:
      arch_sys_exit((int)a0);
      return 0;
    case SYS_WRITE:
      sys_write(a0, (void*) a1, a2);
      return 0;
    case SYS_PUTC:
      return arch_sys_putc(a0);
    default:
      return -1;
  }
  (void)a0;
  (void)a1;
  (void)a2;
  (void)a3;
  (void)a4;
}
