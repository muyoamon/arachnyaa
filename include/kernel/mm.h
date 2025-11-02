#ifndef ARACHNYAA_KERNEL_MM_H_
#define ARACHNYAA_KERNEL_MM_H_

#include "arch/mm.h"
#include "kernel/error.h"
#include "mm/tracker.h"
#include "mm/vmm.h"
#include <lib/stddef.h>
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

} as_t;

/**
 * @brief Create new user address space.
 *
 * @return Pointer to created address space.
 */
as_t *as_create(void);

/**
 * @brief Free user address space.
 *
 * @param[in] mm pointer to mm.
 */
void as_free(as_t *mm);

/**
 * @brief Generic-purpose memory mapping function.
 *
 * @param[in] mm Pointer to memory manager.
 * @param[in] virt Virtual address to map.
 * @param[in] len size in bytes (page-rounded).
 * @param[in] vmm_flags VMM flags.
 * @param[in] prot_flags Protection flags.
 * @param[in/out] io_addr input/output address (flags dependent).
 * @return 0 if success; non-zero otherwise.
 */
kerror_t as_map(as_t *mm, uintptr_t virt, size_t len, vmm_flags_t vmm_flags,
                vmm_prot_t prot_flags, uintptr_t *io_addr);

/**
 * @brief Load page table.
 *
 * @param[in] mm Pointer to memory manager.
 */
static inline void as_load_ptable(as_t *mm) {
  return arch_load_ptable((uintptr_t)mm->ptable);
}

/**
 * @brief Memory copy onto mm's address.
 *
 * @param[in] mm Pointer to memory manager.
 * @param[in] dest Destination (mm).
 * @param[in] src Source (current).
 * @param[in] len Size in bytes.
 * @return 0 if success, non-zero otherwise.
 */
kerror_t as_memcpy(as_t *mm, uintptr_t dest, uintptr_t src, size_t len);

/**
 * @brief Zero memory in mm's address.
 *
 * @param[in] mm Pointer to memory manager.
 * @param[in] addr Address.
 * @param[in] len Size in bytes.
 * @return 0 if success, non-zero otherwise.
 */
kerror_t as_zero(as_t *mm, uintptr_t addr, size_t len);

#endif // ARACHNYAA_KERNEL_MM_H_
