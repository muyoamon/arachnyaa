#ifndef ARACHNYAA_ARCH_X86_CONTEXT_H_
#define ARACHNYAA_ARCH_X86_CONTEXT_H_

#include <stdint.h>

struct arch_context {
  uint32_t ebp, ebx, esi, edi;
  uint32_t eip;
  uint32_t esp;
};

#endif // ARACHNYAA_ARCH_X86_CONTEXT_H_
