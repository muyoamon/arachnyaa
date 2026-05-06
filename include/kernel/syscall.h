#ifndef ARACHNYAA_KERNEL_SYSCALL_H_
#define ARACHNYAA_KERNEL_SYSCALL_H_

#include <stdint.h>

enum {
  SYS_EXIT = 0,
  SYS_NS_BIND = 1,
  SYS_OPEN = 2,
  SYS_WRITE = 3,
  SYS_CAP_CLOSE = 4,
  SYS_PUTC = 5,
};

#endif // ARACHNYAA_KERNEL_SYSCALL_H_
