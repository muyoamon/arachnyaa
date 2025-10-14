
#include "mm/addrspace.h"
#include "arch/x86/defs.h"
#include "mm/kheap.h"
#include "mm/vmm.h"
#include <lib/stddef.h>
#include <stdint.h>
#include <lib/string.h>
struct addr_space {
  uintptr_t cr3_phys;
  int refcnt;
};

addr_space_t* as_create(void) {
  addr_space_t *mm = (addr_space_t*) kmalloc(sizeof *mm);
  memset(mm, 0, sizeof *mm);
  mm->cr3_phys = vmm_create_user_ptable();
  mm->refcnt = 1;
  return mm;
}

vmm_error_code_t as_map_user(addr_space_t *mm, uintptr_t virt, size_t size, uint64_t flags) {
  return vmm_map_user_range(mm->cr3_phys, virt, size, flags | PTE_USER);
}

vmm_error_code_t as_unmap_user(addr_space_t *mm, uintptr_t virt, size_t size) {
  return vmm_unmap_user_range(mm->cr3_phys, virt, size);
}

vmm_error_code_t as_map_user_stack(addr_space_t *mm, uintptr_t *out_ustack_top) {
  uintptr_t base = (USER_STACK_TOP - USER_STACK_SIZE);

  vmm_error_code_t err = as_map_user(mm, base, USER_STACK_SIZE, PTE_WRITABLE | PTE_PRESENT | PTE_USER);
  if (err) return err;
  *out_ustack_top = USER_STACK_TOP;
  return 0;
}

void as_map_user_exact(addr_space_t *mm, uintptr_t virt, uintptr_t phys, uint64_t flags)  {
  return vmm_map_user(mm->cr3_phys, virt, phys, flags | PTE_USER | PTE_PRESENT);
}

void as_load_address_space(addr_space_t *mm) {
  uint32_t cr3_val = (uint32_t)(mm->cr3_phys);

  asm volatile("mov %0, %%cr3" :: "r"(cr3_val) : "memory");
}


