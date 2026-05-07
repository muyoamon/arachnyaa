#ifndef ARACHNYAA_KERNEL_SYSCALL_H_
#define ARACHNYAA_KERNEL_SYSCALL_H_

#include <stdint.h>



enum {
  SYS_EXIT = 0,
  SYS_NS_BIND = 0x1,
  SYS_OPEN = 0x2,
  SYS_WRITE = 0x3,
  SYS_CAP_CLOSE = 0x4,
  SYS_PUTC = 0x5,
  SYS_CALL = 0x6,
  SYS_REPLY = 0x7,
  SYS_RECV = 0x8,
  SYS_SPAWN = 0x9,
};

#endif // ARACHNYAA_KERNEL_SYSCALL_H_
