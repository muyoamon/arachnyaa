#include "paging.h"
#include "drivers/tty.h"
#include <mm/paging.h>
#include <mm/pmm.h>
#include <stdbool.h>
#include <lib/stddef.h>
#include <stdint.h>
#include <lib/string.h>

extern uint64_t pdpt[];

static bool vmm_initialized = false;

static inline void invlpg(void *addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static inline uintptr_t read_cr2(void) {
  uintptr_t fault_addr;
  asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
  return fault_addr;
}

static inline uintptr_t phys_to_virt(uintptr_t paddr) {
  return (((uintptr_t)(paddr) + KERNEL_VIRT_BASE - KERNEL_PHYS_OFFSET));
}

void vmm_clear_identity_map(void) {
  if (!(pdpt[0] & 1))
    return;

  uint64_t *pd = (uint64_t *)phys_to_virt((uintptr_t)(pdpt[0]) & ~0xFFF);

  if (!(pd[0] & 1) || (pd[0] & PTE_HUGE_PAGE))
    return;

  // pd contain physical address convert
  uint64_t *pt = (uint64_t *)phys_to_virt((uintptr_t)(pd[0]) & ~0xFFF);

  for (int i = 0; i < 512; i++) {
    if (pt[i] & 1) {
      pt[i] = 0;
      invlpg((void *)(i * 0x1000));
    }
  }

  pd[0] = 0;
}

void vmm_unmap(uintptr_t virt) {
  uint32_t vaddr = virt;

  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pt_idx = (vaddr >> 12) & 0x1FF;
  uint64_t *v_pt =
      (uint64_t *)(uintptr_t)((((vaddr >> 9) + (pdpt_idx << 30)) | 0x3FE00000) &
                              ~0xFFF);
  
  uintptr_t phys = v_pt[pt_idx];
  if (!(--pmm_ref_count[phys / PMM_PAGE_SIZE])) {
    pmm_free_frame((void*)phys);
  }
  v_pt[pt_idx] = 0;
  invlpg((void *)(uintptr_t)vaddr);
}

void vmm_map(uintptr_t virt, uintptr_t phys, size_t count, uint64_t flags) {
  for (size_t i = 0; i < count; i++) {
    uint32_t vaddr = virt + (i * PAGE_SIZE);
    uint32_t paddr = phys + (i * PAGE_SIZE);

    size_t pdpt_idx = (vaddr >> 30) & 0x3;
    size_t pd_idx = (vaddr >> 21) & 0x1FF;
    size_t pt_idx = (vaddr >> 12) & 0x1FF;

    uint64_t *v_pd = (uint64_t *)(uintptr_t)((vaddr | 0x3FFFF000) & ~0xFFF);
    uint64_t *v_pt =
        (uint64_t *)(uintptr_t)((((vaddr >> 9) + (pdpt_idx << 30)) |
                                 0x3FE00000) &
                                ~0xFFF);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
      uint64_t *pd = (uint64_t *)pmm_alloc_frame();
      uintptr_t v_temp_pd = TEMP_MAPPING_BASE;
      vmm_map(v_temp_pd, (uintptr_t)pd, 1, PTE_WRITABLE | PTE_PRESENT);
      memset((uint64_t*)v_temp_pd, 0, PAGE_SIZE);
      uint64_t pd_64 = (uintptr_t)pd | flags;
      memcpy((uint64_t*)(v_temp_pd + 511 * sizeof(uint64_t)), &pd_64, sizeof(uint64_t));
      pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | flags;
      pmm_ref_count[(uintptr_t)pd / PMM_PAGE_SIZE]++;
      vmm_unmap(v_temp_pd);
    }

    if (!(v_pd[pd_idx] & PTE_PRESENT)) {
      uint64_t *pt = (uint64_t *)pmm_alloc_frame();
      uintptr_t v_temp_pt = TEMP_MAPPING_BASE;
      vmm_map(v_temp_pt, (uintptr_t)pt, 1, PTE_WRITABLE | PTE_PRESENT);
      memset((uint64_t*)v_temp_pt, 0, PAGE_SIZE);
      v_pd[pd_idx] = (uint64_t)(uintptr_t)pt | flags;
      pmm_ref_count[(uintptr_t)pt / PMM_PAGE_SIZE]++;
      vmm_unmap(v_temp_pt);
    }

    v_pt[pt_idx] = paddr | flags;

    pmm_ref_count[paddr / PMM_PAGE_SIZE]++;
    
    // invalidate TLB entry
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
  }
}

void vmm_init() {
  // already initialize in boot

  // map last entry of pd0 and pd3 to itself
  ((uint64_t *)(uintptr_t)phys_to_virt(pdpt[0] & ~0xFFF))[511] =
      (pdpt[0] & ~0xFFF) | PTE_PRESENT | PTE_WRITABLE;
  ((uint64_t *)(uintptr_t)phys_to_virt(pdpt[3] & ~0xFFF))[511] =
      (pdpt[3] & ~0xFFF) | PTE_WRITABLE | PTE_PRESENT;

  // allocate page for pdpt:3 pd:510 for kernel operation that need temporary
  // virtual address
  uint64_t temp_ptable = (uintptr_t)pmm_alloc_frame();

  ((uint64_t *)(uintptr_t)phys_to_virt(pdpt[3] & ~0xFFF))[510] =
      temp_ptable | PTE_WRITABLE | PTE_PRESENT;

  // reload entire cr3
  uintptr_t cr3;
  asm volatile ("mov %%cr3, %0" : "=r"(cr3));
  asm volatile ("mov %0, %%cr3" :: "r"(cr3));
  
  vmm_initialized = true;
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
      vmm_map(fault_addr, phys, 1,
              PTE_PRESENT | PTE_WRITABLE | (user ? PTE_USER : 0));
      return;
    }
  }

  tty_writestring("Page fault at 0x");
  tty_write_hex(fault_addr);
  tty_writestring(" (error=0x");
  tty_write_hex(error_code);
  tty_writestring(")\n");
}


uintptr_t vmm_get_phys_addr(uintptr_t virt) {
  if (!vmm_initialized) {
    return virt;
  }
  uint8_t pdpt_idx = (virt >> 30) & 0x3;
  uint64_t *v_pt =
      (uint64_t *)(uintptr_t)((((virt >> 9) + (pdpt_idx << 30)) | 0x3FE00000) &
                              ~0xFFF);
  uint16_t pt_idx = (virt >> 12) & 0x1FF;
  uint16_t offset = virt & 0xFFF;
  return v_pt[pt_idx] + offset;
}
