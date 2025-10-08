#ifndef ARACHNYAA_MM_KSTACK_H_
#define ARACHNYAA_MM_KSTACK_H_

#include "mm/vmm.h"
#include <lib/stddef.h>
#include <stdint.h>

typedef struct {
  uintptr_t top;    // stack pointer start
  uintptr_t base;   // lowest address (after guard)
  size_t size;  // usable bytes (exclude guards)
} kstack_t;




void kstack_init();

kstack_t kstack_alloc(size_t size);

void kstack_free(kstack_t *ks);





#endif // ARACHNYAA_MM_KSTACK_H_
