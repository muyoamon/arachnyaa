#ifndef ARACHNYAA_KERNEL_PROTOCOL_H_
#define ARACHNYAA_KERNEL_PROTOCOL_H_

#include <stdint.h>

enum {
  KOP_OPEN = 1u << 0,
  KOP_CALL = 1u << 1,
  KOP_READ = 1u << 2,
  KOP_WRITE = 1u << 3,
  KOP_MAP = 1u << 4,
  KOP_CLOSE = 1u << 5,
};

#define PROCESS_NAMESPACE_CAPACITY 16
#define PROCESS_PROTOCOL_NAME_MAX 16

#endif // ARACHNYAA_KERNEL_PROTOCOL_H_
