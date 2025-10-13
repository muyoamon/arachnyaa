#ifndef ARACHNYAA_ARCH_X86_REGISTER_H_
#define ARACHNYAA_ARCH_X86_REGISTER_H_

#include <stdint.h>
struct registers {
  uint32_t gs, fs, es, ds; // segment selector

  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
  uint32_t int_no;
  uint32_t err_code; // Pushed by our stubs.
  uint32_t eip, cs, eflags, useresp,
      ss; // Pushed by the processor automatically.
};


#endif // ARACHNYAA_ARCH_X86_REGISTER_H_
