#include <mm/paging.h>
#include "paging.h"
#include "tty.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <mm/pmm.h>
#include <string.h>


extern uint64_t pdpt[];

static inline void invlpg(void* addr) {
  asm volatile ("invlpg (%0)" :: "r" (addr) : "memory");
}

static inline uintptr_t read_cr2(void) {
  uintptr_t fault_addr;
  asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
  return fault_addr;
}

static inline uintptr_t phys_to_virt(uintptr_t paddr) {
  return (((uintptr_t)(paddr) + KERNEL_VIRT_BASE - KERNEL_PHYS_OFFSET));
}

void clear_identity_map(void) {
  if (!(pdpt[0] & 1)) return;

  uint64_t* pd = (uint64_t*)((uintptr_t)(pdpt) & ~0xFFF);

  if (!(pd[0] & 1) || (pd[0] & PTE_HUGE_PAGE)) return;

  uint64_t* pt = (uint64_t*)((uintptr_t)(pd) & ~0xFFF);

  for (int i = 0; i < 512; i++) {
    if (pt[i] & 1) {
      pt[i] = 0;
      invlpg((void*)(i * 0x1000));
    }
  }

  pd[0] = 0;
}

void paging_map(uintptr_t virt, uintptr_t phys, size_t count, uint64_t flags) {
  for (size_t i = 0; i < count; i++) {
    uint32_t vaddr = virt + (i * PAGE_SIZE);
    uint32_t paddr = phys + (i * PAGE_SIZE);

    size_t pdpt_idx = (vaddr >> 30) & 0x3;
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
      uint64_t *pd = (uint64_t*)pmm_alloc_frame();
      memset(pdpt, 0, PAGE_SIZE);
      pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | flags;
    }

    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFF);
    pd = (uint64_t*) phys_to_virt((uintptr_t)pd);
    size_t pd_idx = (vaddr >> 21) & 0x1FF;
    if (!(pd[pd_idx] & PTE_PRESENT)) {
      uint64_t *pt = (uint64_t*)pmm_alloc_frame();
      memset(pt, 0, PAGE_SIZE);
      pd[pd_idx] = (uint64_t)(uintptr_t)pt | flags;
    }
    
    uint64_t *pt = (uint64_t*)(uintptr_t)(pd[pd_idx] & ~0xFFF);
    size_t pt_idx = (vaddr >> 12) & 0x1FF;
    pt[pt_idx] = paddr | flags;

    //invalidate TLB entry
    asm volatile ("invlpg (%0)" : : "r" (vaddr) : "memory");
  }
}

void paging_init() {
  // already initialize in boot
  
  // unmap identity map 
  clear_identity_map();
}

void page_fault_handler(uint32_t error_code) {
  uintptr_t fault_addr = read_cr2();
  bool present = error_code & 0x01;
  // bool write = error_code & 0x02;
  bool user = error_code & 0x04;
  // bool reserved = error_code & 0x08;
  // bool instruction = error_code & 0x10;

  // TODO:
  if (!present) {
    // allocate new page
    uintptr_t phys = (uintptr_t)pmm_alloc_frame();
    if (phys) {
      paging_map(fault_addr,phys,1, PTE_PRESENT | PTE_WRITABLE | (user ? PTE_USER : 0));
      return;
    }
  }

  tty_writestring("Page fault at 0x");
  tty_write_hex(fault_addr);
  tty_writestring(" (error=0x");
  tty_write_hex(error_code);
  tty_writestring(")\n");
}



