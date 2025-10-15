#ifndef ARACHNYAA_KERNEL_MM_H_
#define ARACHNYAA_KERNEL_MM_H_

#include "arch/mm.h"
#include "kernel/error.h"
#include "mm/tracker.h"
#include "mm/vmm.h"
#include <stdint.h>

typedef struct vma_struct {
  uintptr_t base;
  size_t len;
  vmm_flags_t vmm_flags;
  vmm_prot_t prot_flags;
  struct vma_struct *prev, *next;
} vma_t;

typedef struct {
  tracker_tree_t freetree;
  vma_t *vmal;
  uintptr_t *ptable;
  size_t refcnt;

} mm_t;

/**
 * @brief Create new user address space.
 *
 * @return Pointer to created address space.
 */
mm_t *mm_create(void);

kerror_t mm_map(mm_t *mm, uintptr_t virt, size_t len, vmm_flags_t vmm_flags,
                vmm_prot_t prot_flags, uintptr_t *io_addr);


static inline void mm_load_ptable(mm_t *mm) {
  return arch_load_ptable((uintptr_t)mm->ptable);
}
#endif // ARACHNYAA_KERNEL_MM_H_
