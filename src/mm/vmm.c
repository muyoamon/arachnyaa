#include "arch/cpu.h"
#include "kernel/error.h"
#include "mm/tracker.h"
#include <lib/stddef.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdint.h>

static void crit_enter() { arch_local_irq_disable(); }
static void crit_exit() { arch_local_irq_enable(); }

void vmm_free(uintptr_t virt, size_t pages) {
  for (size_t i = 0; i < pages; i++) {
    vmm_unmap(virt + (i * PMM_PAGE_SIZE));
  }
}

uintptr_t vmm_reserve(vmm_region_t *r, size_t size, size_t align) {
  uintptr_t cover = 0u;
  size_t need = (size_t)(size);
  bool ok = tracker_reserve(&r->free_map, need, align, &cover);
  if (!ok || cover != r->base) {
    return (uintptr_t)NULL;
  }
  return cover;
}

bool vmm_release(vmm_region_t *r, uintptr_t base, size_t size) {
  bool ok = tracker_add(&r->free_map, base, size);
  return ok;
}

#define VMMF_PLACE_MASK (VMM_FIXED | VMM_HINT | VMM_AUTO)
#define VMMF_GUARD_MASK (VMM_GUARD_BELOW | VMM_GUARD_ABOVE)
#define VMMF_PAGE_MASK (VMM_PAGE_4K | VMM_PAGE_2M)
#define VMMF_BACKING_MASK (VMM_MAP_ANON | VMM_MAP_FILE)
#define VMMF_SHARE_MASK (VMM_MAP_PRIVATE | VMM_MAP_SHARED)

static inline int vmm_has(vmm_flags_t f, vmm_flags_t bits) {
  return (f & bits) == bits;
}
static inline int vmm_any(vmm_flags_t f, vmm_flags_t bits) {
  return (f & bits) != 0;
}
static inline int vmm_exactly_one(vmm_flags_t f, vmm_flags_t mask) {
  vmm_flags_t x = f & mask;
  return x && !(x & (x - 1));
}

static kerror_t vmm_validate(vmm_flags_t vmm_flags) {
  // Exactly one placement policy
  if (!vmm_exactly_one(vmm_flags, VMMF_PLACE_MASK))
    return KERR_INVAL;

  // Exactly one backing
  if (!vmm_exactly_one(vmm_flags, VMMF_BACKING_MASK))
    return KERR_INVAL;

  // private & share is mutually exclusive
  if ((vmm_flags & VMM_MAP_FILE) &&
      !vmm_exactly_one(vmm_flags, VMMF_SHARE_MASK))
    return KERR_INVAL;
  if ((vmm_flags & VMM_MAP_ANON) && (vmm_flags & VMMF_SHARE_MASK))
    return KERR_INVAL;

  // page is default to 4K and are mutually exclusive
  if (!vmm_any(vmm_flags, VMMF_PAGE_MASK))
    vmm_flags |= VMM_PAGE_4K;
  if (!vmm_exactly_one(vmm_flags, VMMF_PAGE_MASK))
    return KERR_INVAL;

  if ((vmm_flags & VMMF_GUARD_MASK) && !(vmm_flags & VMM_MAP_ANON))
    return KERR_INVAL;

  return 0;
}

typedef struct {
  uint64_t pte_flag;
  size_t page_size;
} vmm_pt_cfg;

static inline vmm_pt_cfg vmm_decode(vmm_prot_t prot, vmm_flags_t fl) {
  vmm_pt_cfg cfg = {0};
  if (prot & VMM_PROT_WRITE)
    cfg.pte_flag |= PTE_WRITABLE;
  if (prot & VMM_PROT_USER)
    cfg.pte_flag |= PTE_USER;
  if (prot & VMM_PROT_GLOBAL)
    cfg.pte_flag |= PTE_GLOBAL;

  // if (!(prot & VMM_PROT_EXEC)) cfg.pte_flag |= PTE_NX;

  if (fl & VMM_PAGE_2M) {
    cfg.page_size = 2048;
  } else {
    cfg.page_size = PAGE_SIZE;
  }

  if (fl & VMM_MAP_LAZY_COMMIT) {
    cfg.pte_flag &= ~PTE_PRESENT;
  } else {
    cfg.pte_flag |= PTE_PRESENT;
  }

  return cfg;
}

kerror_t vmm_alloc_region(vmm_region_t *r, size_t size, vmm_prot_t prot_flags,
                          vmm_flags_t vmm_flags, uintptr_t *out_addr) {
  kerror_t err_code = vmm_validate(vmm_flags);
  if (err_code)
    return err_code;

  vmm_pt_cfg cfg = vmm_decode(prot_flags, vmm_flags);

  uintptr_t base = 0;
  if (vmm_flags & VMM_FIXED) {
    // currently unsupported
    return KERR_INVAL;
  } else {
    // hint is currently unsupported; will allocate anywhere
    if (!tracker_reserve(&r->free_map, size, cfg.page_size, &base)) {
      return KERR_INVAL;
    }
  }

  kerror_t err = vmm_alloc(base, size, vmm_flags, prot_flags, NULL, NULL);
  if (err) {
    tracker_add(&r->free_map, base, size);
    return err;
  }
  *out_addr = base;
  return 0;
}

kerror_t vmm_free_region(vmm_region_t *r, uintptr_t base, size_t size,
                         size_t guard_below, size_t guard_above) {
  uintptr_t base_to_free = base + guard_below * PAGE_SIZE;
  size_t size_to_free = size - guard_above * PAGE_SIZE;

  vmm_free(base_to_free, size_to_free);
  vmm_release(r, base, size);

  return 0;
}

kerror_t vmm_map_user_range(uintptr_t utable, uintptr_t virt, size_t size,
                            uint64_t flags) {
  if (size == 0) {
    return KERR_INVAL; // invalid size
  }
  size_t page = (size - 1) / PAGE_SIZE + 1;
  for (size_t i = 0; i < page; i++) {
    uintptr_t phys = (uintptr_t)pmm_alloc_frame();
    if (!phys)
      return KERR_NOMEM;
    vmm_map_user(utable, virt + i * PAGE_SIZE, phys, flags);
  }

  return 0;
}

kerror_t vmm_unmap_user_range(uintptr_t utable, uintptr_t virt, size_t size) {
  if (size == 0) {
    return KERR_INVAL;
  }

  size_t page = (size - 1) / PAGE_SIZE + 1;
  for (size_t i = 0; i < page; i++) {
    vmm_unmap_user(utable, virt + i * PAGE_SIZE);
  }

  return 0;
}

typedef void (*_vmm_mmapf_t)(uintptr_t, uintptr_t, size_t, uint64_t);
static uintptr_t _vmm_utable;
static inline void _vmm_map_u(uintptr_t v, uintptr_t p, size_t pg,
                              uint64_t fl) {
  for (size_t i = 0; i < pg; i++) {
    vmm_map_user(_vmm_utable, v + i * PAGE_SIZE, p + i * PAGE_SIZE, fl);
  }
  return;
}
static inline void _vmm_map_s(uintptr_t v, uintptr_t p, size_t pg,
                              uint64_t fl) {
  vmm_map(v, p, pg, fl);
}

kerror_t vmm_alloc(uintptr_t virt, size_t bytes, vmm_flags_t vmm_flags,
                   vmm_prot_t prot_flags, uintptr_t *io_addr,
                   uintptr_t *utable) {

  crit_enter();
  kerror_t err_code = vmm_validate(vmm_flags);
  if (err_code) {
    crit_exit();
    return err_code;
  }

  vmm_pt_cfg cfg = vmm_decode(prot_flags, vmm_flags);

  if (bytes == 0) {
    crit_exit();
    return KERR_INVAL;
  };

  // if utable
  _vmm_mmapf_t _mmapf = _vmm_map_s;
  if (utable) {
    _mmapf = _vmm_map_u;
    _vmm_utable = (uintptr_t)utable;
  }

  size_t guard_low = vmm_has(vmm_flags, VMM_GUARD_BELOW) ? (cfg.page_size) : 0;
  size_t guard_high = vmm_has(vmm_flags, VMM_GUARD_ABOVE) ? (cfg.page_size) : 0;

  uintptr_t map_low = virt + guard_low;
  uintptr_t map_high = virt + bytes - guard_high;

  if (vmm_flags & VMM_MAP_ANON) {
    // map from [virt, guard_high)
    for (size_t i = virt; i < map_high; i += cfg.page_size) {
      uintptr_t phys;
      phys = (uintptr_t)pmm_alloc_frame();
      if (!phys) {
        crit_exit();
        return KERR_NOMEM;
      }
      if (i < map_low && guard_low) {
        _mmapf(map_low, phys, 1, cfg.pte_flag & ~PTE_PRESENT);
      } else {
        if (prot_flags) {
          _mmapf(i, phys, 1, cfg.pte_flag);
        }
      }
    }
    if (guard_high) {
      uintptr_t phys = (uintptr_t)pmm_alloc_frame();
      if (!phys) {
        crit_exit();
        return KERR_NOMEM;
      }
      _mmapf(guard_high, phys, 1, cfg.pte_flag & ~PTE_PRESENT);
    }
  } else if (vmm_flags & VMM_MAP_FILE) {
    //
    _mmapf(virt, (uintptr_t)io_addr, 1, cfg.pte_flag);
  }

  crit_exit();
  return 0;
}
