#include "kernel/syscall.h"
#include "kernel/cap.h"
#include "sys/cap.h"
#include "sys/device.h"
#include "sys/ep.h"
#include "sys/ipc.h"
#include "sys/irq.h"
#include "sys/namespace.h"
#include "sys/pipe.h"
#include "sys/proc.h"
#include "sys/read.h"
#include "sys/vspace.h"
#include "sys/write.h"
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
  (void)a4;  /* a4=edi is the return-high register; not a user argument */
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

  /* Phase 0 additions */
  case SYS_EP_CREATE:
    return sys_ep_create();
  case SYS_CAP_RESTRICT:
    return sys_cap_restrict(syscall_cap(a0, a1), (uint32_t)a2);
  case SYS_IRQ_CLAIM:
    return sys_irq_claim((uint32_t)a0);
  case SYS_PROC_WAIT:
    return sys_proc_wait(syscall_cap(a0, a1));
  case SYS_IRQ_WAIT:
    return sys_irq_wait(syscall_cap(a0, a1));
  case SYS_IO_IN:
    return sys_io_in((uint32_t)a0);
  case SYS_IO_OUT:
    return sys_io_out((uint32_t)a0, (uint32_t)a1);
  case SYS_VSPACE_CREATE:
    return sys_vspace_create();
  case SYS_VSPACE_SELF:
    return sys_vspace_self();
  case SYS_PAGE_ALLOC:
    return sys_page_alloc((size_t)a0, (uint32_t)a1, (uintptr_t)a2);
  case SYS_VSPACE_MAP:
    return sys_vspace_map((const sys_vspace_map_args_t *)a0);
  case SYS_VSPACE_UNMAP:
    return sys_vspace_unmap(syscall_cap(a0, a1), (uintptr_t)a2, (size_t)a3);

  /* Phase 3 additions */
  case SYS_IRQ_NOTIFY:
    return sys_irq_notify(syscall_cap(a0, a1), syscall_cap(a2, a3));
  case SYS_DEFER_CALL:
    return sys_defer_call();
  case SYS_REPLY_TO:
    return sys_reply_to((uint32_t)a0, (const sys_ipc_msg_t *)a1);
  case SYS_BOOTSTRAP_CAP:
    return sys_bootstrap_cap((uint32_t)a0);
  case SYS_PIPE:
    return sys_pipe((sys_pipe_result_t *)a0);

  default:
    return -1;
  }
}
