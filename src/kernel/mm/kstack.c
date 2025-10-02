#include "arch/x86/defs.h"
#include <mm/vmm.h>
#include <mm/kstack.h>
#include <stddef.h>




static inline size_t round_up_page(size_t n) {
  return (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

kstack_t kstack_alloc(size_t bytes) {
  
  if (!bytes) bytes = KSTACK_DEFAULT_SIZE;

  kstack_t ks = {0};
  // const size_t usable = round_up_page(bytes);
  // const size_t total = usable + PAGE_SIZE; // +1 guard page
  
  
  
  return ks;
}
