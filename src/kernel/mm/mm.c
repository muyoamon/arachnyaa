
#include "kernel/error.h"
#include "kernel/mm.h"
#include "mm/kheap.h"
#include "mm/tracker.h"
#include "mm/vmm.h"
#include <lib/string.h>
#include <stdint.h>


#define MIN(a, b) ((a < b) ? a : b)

static inline uintptr_t round_page(uintptr_t addr, size_t pagesize) {
  return addr & ~(pagesize - 1);
}




static inline void vma_insert(as_t *mm, vma_t *vma) {
  if (mm->vmal == NULL) {
    mm->vmal = vma;
  }
  for (vma_t *pvma = mm->vmal; pvma != NULL; pvma = pvma->next) {
    if (vma->base > pvma->base) {
      vma->next = pvma->next;
      pvma->next = vma;
      vma->prev = pvma;
    }
  }
}

as_t *as_create(void) {
  as_t *mm = (as_t *)kmalloc(sizeof *mm);
  memset(mm, 0, sizeof *mm);
  mm->ptable = (uintptr_t *)vmm_create_user_ptable();
  mm->refcnt = 1;
  tracker_init(&(mm->freetree));
  mm->vmal = NULL;
  return mm;
}

void as_free(as_t *mm) {
  // TODO:
  kfree(mm);
}

kerror_t as_map(as_t *mm, uintptr_t virt, size_t len, vmm_flags_t vmm_flags,
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

kerror_t as_memcpy(as_t *mm, uintptr_t dest, uintptr_t src, size_t len) {
  size_t copied = 0;
  while (copied < len) {
    uintptr_t u_page = round_page(dest, PAGE_SIZE);
    size_t page_offset = dest - u_page;
    size_t chunk = MIN(len - copied, PAGE_SIZE - page_offset);

    kerror_t err = vmm_cpy_user_range((uintptr_t)mm->ptable, src, dest, chunk);
    if (err) return err;

    dest += chunk;
    src += chunk;
    copied += chunk;
  }
  return 0;
}

kerror_t as_zero(as_t *mm, uintptr_t addr, size_t len) {
  size_t copied = 0;
  while (copied < len) {
    uintptr_t u_page = round_page(addr, PAGE_SIZE);
    size_t page_offset = addr - u_page;
    size_t chunk = MIN(len - copied, PAGE_SIZE - page_offset);

    kerror_t err = vmm_zero_user_range((uintptr_t)mm->ptable, addr, chunk);
    if (err) return err;

    addr += chunk;
    copied += chunk;
  }
  return 0; 
}
