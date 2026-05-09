#include "kernel/syscall.h"
#include "kernel/cap.h"
#include "sys/ipc.h"
#include "sys/namespace.h"
#include "sys/proc.h"
#include "sys/read.h"
#include "sys/write.h"
#include "sys/cap.h"
#include "uapi/syscalls.h"
#include <stdint.h>

extern void arch_sys_exit(int);
extern uint64_t arch_sys_putc(char);

typedef uintptr_t native_word;

static inline cap_handle_t syscall_cap(native_word lo, native_word hi) {
  return (cap_handle_t)lo | ((cap_handle_t)hi << 32);
}

uint64_t syscall_dispatcher(uint32_t syscode, native_word a0, native_word a1,
                            native_word a2, native_word a3, native_word a4) {
  switch (syscode) {
  case SYS_EXIT:
    arch_sys_exit((int)a0);
    return 0;
  case SYS_NS_BIND:
    return sys_ns_bind((const char *)a0, syscall_cap(a1, a2), a3);
  case SYS_OPEN:
    return sys_open((const char *)a0, a1);
  case SYS_WRITE:
    return sys_write(syscall_cap(a0, a1), (void *)a2, a3);
  case SYS_CAP_CLOSE:
    return sys_cap_close(syscall_cap(a0, a1));
  case SYS_PUTC:
    return arch_sys_putc(a0);
  case SYS_CALL:
    return sys_call(syscall_cap(a0, a1), (const sys_ipc_msg_t *)a2,
                    (sys_ipc_msg_t *)a3);
  case SYS_REPLY:
    return sys_reply((sys_ipc_msg_t *)a0);
  case SYS_RECV:
    return sys_recv(syscall_cap(a0, a1), (sys_ipc_msg_t *)a2);
  case SYS_SPAWN:
    return sys_proc_spawn((sys_proc_arg_t *)a0, (pid_t *)a1,
                          (cap_handle_t *)a2);
  case SYS_READ:
    return sys_read(syscall_cap(a0, a1), (void *)a2, a3);
  case SYS_CLOSE:
    return sys_close(syscall_cap(a0, a1));
  default:
    return -1;
  }
  (void)a0;
  (void)a1;
  (void)a2;
  (void)a3;
  (void)a4;
}
