#include "mm/tracker.h"
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <stdint.h>




int vmm_alloc(uintptr_t virt, size_t pages, uint64_t flags) {
  for (size_t i = 0; i < pages; i++) {
    uintptr_t phys = (uintptr_t)pmm_alloc_frame();
    if (!phys) {
      return -1;
    }
    vmm_map(virt + (i * PMM_PAGE_SIZE), phys, 
          1, flags);
  }
  return 0;
}

void vmm_free(uintptr_t virt, size_t pages) {
  for (size_t i = 0; i < pages; i++) {
    vmm_unmap(virt + (i * PMM_PAGE_SIZE));
  }
}


// WIP

bool vmm_reserve(vmm_region_t *r, uintptr_t base, size_t size) {
  uintptr_t cover = 0u;
  size_t need  = (size_t)((base + size) - r->base);
  bool ok = tracker_alloc(&r->free_map, need, PAGE_SIZE, &cover);
  if (!ok || cover != r->base) {
    return false;
  }
  return true;
}

bool vmm_release(vmm_region_t *r, uintptr_t base, size_t size);
