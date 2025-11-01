#ifndef ARACHNYAA_KERNEL_SYSCALL_H_
#define ARACHNYAA_KERNEL_SYSCALL_H_

#include <stdint.h>

enum {
  SYS_EXIT = 0,
  SYS_WRITE = 1,
  SYS_PUTC = 2,
};

#endif // ARACHNYAA_KERNEL_SYSCALL_H_
