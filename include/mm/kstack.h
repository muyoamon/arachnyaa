#ifndef ARACHNYAA_MM_KSTACK_H_
#define ARACHNYAA_MM_KSTACK_H_

#include <stddef.h>

typedef struct {
  void* top;    // stack pointer start
  void* base;   // lowest address (after guard)
  size_t size;  // usable bytes (exclude guards)
} kstack_t;

kstack_t kstack_alloc(size_t size);

void kstack_free(kstack_t *ks);





#endif // ARACHNYAA_MM_KSTACK_H_
