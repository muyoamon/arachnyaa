#include "kernel/cap.h"
#include "kernel/syscall.h"
#include "sys/namespace.h"
#include "sys/write.h"
#include <stdint.h>

extern void arch_sys_exit(int);
extern uint64_t arch_sys_putc(char);

typedef uintptr_t native_word;

static inline cap_handle_t syscall_cap(native_word lo, native_word hi) {
  return (cap_handle_t)lo | ((cap_handle_t)hi << 32);
}

uint64_t syscall_dispatcher(uint32_t syscode, native_word a0, native_word a1, native_word a2, 
                            native_word a3, native_word a4) {
  switch (syscode) {
    case SYS_EXIT:
      arch_sys_exit((int)a0);
      return 0;
    case SYS_NS_BIND:
      return sys_ns_bind((const char *)a0, syscall_cap(a1, a2), a3);
    case SYS_OPEN:
      return sys_open((const char *)a0, a1);
    case SYS_WRITE:
      return sys_write(syscall_cap(a0, a1), (void*) a2, a3);
    case SYS_CAP_CLOSE:
      return sys_cap_close(syscall_cap(a0, a1));
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
