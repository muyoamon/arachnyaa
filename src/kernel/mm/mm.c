
#include "kernel/error.h"
#include "kernel/mm.h"
#include "mm/kheap.h"
#include "mm/tracker.h"
#include "mm/vmm.h"
#include <lib/string.h>

static inline void vma_insert(mm_t *mm, vma_t *vma) {
  for (vma_t *pvma = mm->vmal; pvma != NULL; pvma = pvma->next) {
    if (vma->base > pvma->base) {
      vma->next = pvma->next;
      pvma->next = vma;
      vma->prev = pvma;
    }
  }
}

mm_t *mm_create(void) {
  mm_t *mm = (mm_t *)kmalloc(sizeof *mm);
  memset(mm, 0, sizeof *mm);
  mm->ptable = (uintptr_t *)vmm_create_user_ptable();
  mm->refcnt = 1;
  tracker_init(&(mm->freetree));
  mm->vmal = NULL;
  return mm;
}

kerror_t mm_map(mm_t *mm, uintptr_t virt, size_t len, vmm_flags_t vmm_flags,
                vmm_prot_t prot_flags, uintptr_t *io_addr) {
  kerror_t err =
      vmm_alloc(virt, len, vmm_flags, prot_flags, io_addr, mm->ptable);
  if (err)
    return err;


  vma_t* vma = (vma_t*)kmalloc(sizeof(vma_t));
  vma->prot_flags = prot_flags;
  vma->vmm_flags = vmm_flags;
  vma->prev = NULL;
  vma->next = NULL;
  vma->base = virt;
  vma->len = len;
  vma_insert(mm, vma);

  return 0;
}
