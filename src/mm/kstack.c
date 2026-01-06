#include "arch/x86/defs.h"
#include "mm/layout.h"
#include "mm/tracker.h"
#include <lib/stddef.h>
#include <mm/kstack.h>
#include <mm/vmm.h>
#include <stdint.h>

static vmm_region_t kstack_region;

static inline size_t round_up_page(size_t n) {
  return (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

void kstack_init() {
  kstack_region.base = (uintptr_t)kernel_stack_region_base;
  kstack_region.top = (uintptr_t)kernel_stack_region_top;
  tracker_add(&kstack_region.free_map, kstack_region.base,
              kstack_region.top - kstack_region.base);
}

kstack_t kstack_alloc(size_t bytes) {

  if (!bytes)
    bytes = KSTACK_DEFAULT_SIZE;

  kstack_t ks = {0};
  const size_t usable = round_up_page(bytes);
  const size_t total = usable + PAGE_SIZE; // +1 guard page

  vmm_alloc_region(&kstack_region, total, VMM_PROT_READ | VMM_PROT_WRITE,
                   VMM_MAP_ANON | VMM_PAGE_4K | VMM_GUARD_BELOW | VMM_AUTO,
                   &ks.base);
  ks.top = ks.base + total;
  ks.size = usable;

  return ks;
}

void kstack_free(kstack_t *ks) {
  vmm_free_region(&kstack_region, ks->base, ks->size + PAGE_SIZE, 1, 0);
}
