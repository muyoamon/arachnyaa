#include "paging.h"
#include "arch/cpu.h"
#include "arch/mm.h"
#include "drivers/io.h"
#include "drivers/tty.h"
#include "kernel/error.h"
#include <mm/vmm.h>
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
      invlpg((void *)((uintptr_t)i * 0x1000));
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
    if (phys) 
      pmm_free_frame((void*)phys);
    // leave [0,PAGE_SIZE) as mapped.
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
      uint64_t pd_64 = (uintptr_t)pd | PTE_WRITABLE | PTE_PRESENT;
      memcpy((uint64_t*)(v_temp_pd + 511 * sizeof(uint64_t)), &pd_64, sizeof(uint64_t));
      pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE;
      pmm_ref_count[(uintptr_t)pd / PMM_PAGE_SIZE]++;
      vmm_unmap(v_temp_pd);
    }

    if (!(v_pd[pd_idx] & PTE_PRESENT)) {
      uint64_t *pt = (uint64_t *)pmm_alloc_frame();
      uintptr_t v_temp_pt = TEMP_MAPPING_BASE;
      vmm_map(v_temp_pt, (uintptr_t)pt, 1, PTE_WRITABLE | PTE_PRESENT);
      memset((uint64_t*)v_temp_pt, 0, PAGE_SIZE);
      v_pd[pd_idx] = (uint64_t)(uintptr_t)pt | PTE_PRESENT | PTE_WRITABLE;
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

/* Minimal serial (COM1) output for fault debugging without a display. */
static void serial_init_once(void) {
  static int done = 0;
  if (done) return;
  done = 1;
  outb(0x3F8 + 1, 0x00); /* disable interrupts */
  outb(0x3F8 + 3, 0x80); /* DLAB on */
  outb(0x3F8 + 0, 0x01); /* divisor lo: 115200 baud */
  outb(0x3F8 + 1, 0x00); /* divisor hi */
  outb(0x3F8 + 3, 0x03); /* 8N1, DLAB off */
  outb(0x3F8 + 2, 0xC7); /* FIFO */
}
static void serial_putc(char c) {
  serial_init_once();
  while (!(inb(0x3F8 + 5) & 0x20)) {}
  outb(0x3F8, (uint8_t)c);
}
static void serial_puts(const char *s) {
  while (*s) serial_putc(*s++);
}
static void serial_puthex(uint32_t v) {
  static const char hex[] = "0123456789abcdef";
  for (int i = 7; i >= 0; i--)
    serial_putc(hex[(v >> (i*4)) & 0xF]);
}
static void serial_puthex64(uint64_t v) {
  serial_puthex((uint32_t)(v >> 32));
  serial_puthex((uint32_t)v);
}

/* Read the PTE from a user page table for a given virtual address. */
static uint64_t user_get_pte(uintptr_t utable, uintptr_t vaddr) {
  uint64_t *tmp_pdpt = (uint64_t *)TEMP_MAPPING_BASE;
  vmm_map((uintptr_t)tmp_pdpt, utable, 1, PTE_PRESENT | PTE_WRITABLE);

  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pd_idx   = (vaddr >> 21) & 0x1FF;
  size_t pt_idx   = (vaddr >> 12) & 0x1FF;

  uint64_t *v_pd   = (uint64_t *)(TEMP_MAPPING_BASE + PAGE_SIZE);
  uint64_t *v_pt   = (uint64_t *)(TEMP_MAPPING_BASE + 2 * PAGE_SIZE);

  if (!(tmp_pdpt[pdpt_idx] & PTE_PRESENT)) { vmm_unmap((uintptr_t)tmp_pdpt); return 0; }
  vmm_map((uintptr_t)v_pd, tmp_pdpt[pdpt_idx] & ~0xFFFull, 1, PTE_PRESENT | PTE_WRITABLE);

  if (!(v_pd[pd_idx] & PTE_PRESENT)) {
    vmm_unmap((uintptr_t)v_pd);
    vmm_unmap((uintptr_t)tmp_pdpt);
    return 0;
  }
  vmm_map((uintptr_t)v_pt, v_pd[pd_idx] & ~0xFFFull, 1, PTE_PRESENT | PTE_WRITABLE);

  uint64_t pte = v_pt[pt_idx];
  vmm_unmap((uintptr_t)v_pt);
  vmm_unmap((uintptr_t)v_pd);
  vmm_unmap((uintptr_t)tmp_pdpt);
  return pte;
}

void page_fault_handler(uint32_t error_code) {
  uintptr_t fault_addr = read_cr2();
  bool present = error_code & 0x01;
  // bool write = error_code & 0x02;
  bool user = error_code & 0x04;
  // bool reserved = error_code & 0x08;
  // bool instruction = error_code & 0x10;

  /* Demand-paging: satisfy not-present faults by allocating a physical frame. */
  if (!present) {
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

  /* Dump via serial so it's visible without a VGA display. */
  serial_puts("\r\nPAGE FAULT err=");
  serial_puthex(error_code);
  serial_puts(" addr=");
  serial_puthex(fault_addr);
  uintptr_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  serial_puts(" cr3=");
  serial_puthex(cr3);
  if (user) {
    uint64_t pte = user_get_pte(cr3, fault_addr);
    serial_puts(" pte=");
    serial_puthex64(pte);
    /* Also dump PTE for the page-aligned base */
    uint64_t pte_page = user_get_pte(cr3, fault_addr & ~0xFFFu);
    serial_puts(" pte_page=");
    serial_puthex64(pte_page);
  }
  serial_puts("\r\n");

  for (;;) arch_cpu_idle();
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

uintptr_t vmm_create_user_ptable() {
  uintptr_t pdpt_phys = (uintptr_t)pmm_alloc_frame();
  pmm_ref_count[pdpt_phys/PAGE_SIZE]++;
  uint64_t *pdpt_virt = (uint64_t*)TEMP_MAPPING_TOP;
  vmm_map((uintptr_t)pdpt_virt, pdpt_phys, 1, PTE_PRESENT | PTE_WRITABLE);
  memset(pdpt_virt, 0, PAGE_SIZE);

  pdpt_virt[KERNEL_PDPT_INDEX] = pdpt[KERNEL_PDPT_INDEX] | PTE_PRESENT;
  pmm_ref_count[pdpt[KERNEL_PDPT_INDEX]/PAGE_SIZE]++;
  vmm_unmap((uintptr_t)pdpt_virt);
  return pdpt_phys;
}

void vmm_map_user(uintptr_t utable, uintptr_t virt, uintptr_t phys, uint64_t flags) {
  uint64_t* pdpt = (uint64_t*)TEMP_MAPPING_TOP;
  vmm_map((uintptr_t)pdpt, utable , 1, PTE_PRESENT | PTE_WRITABLE);
  uint32_t vaddr = virt;
  uint32_t paddr = phys;

  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pd_idx = (vaddr >> 21) & 0x1FF;
  size_t pt_idx = (vaddr >> 12) & 0x1FF;

  uint64_t *v_pd = (uint64_t*)(TEMP_MAPPING_BASE + PAGE_SIZE); 
  uint64_t *v_pt = (uint64_t*)(TEMP_MAPPING_BASE + 2 * PAGE_SIZE);
  
  if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
    uint64_t *pd = (uint64_t *)pmm_alloc_frame();
    uintptr_t v_temp_pd = TEMP_MAPPING_BASE;
    vmm_map(v_temp_pd, (uintptr_t)pd, 1, PTE_WRITABLE | PTE_PRESENT);
    memset((uint64_t*)v_temp_pd, 0, PAGE_SIZE);

    // map last entry in pd to itself
    uint64_t pd_64 = (uintptr_t)pd | PTE_WRITABLE | PTE_PRESENT | PTE_USER;
    memcpy((uint64_t*)(v_temp_pd + 511 * sizeof(uint64_t)), &pd_64, sizeof(uint64_t));
    
    pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pmm_ref_count[(uintptr_t)pd / PMM_PAGE_SIZE]++;
    vmm_unmap(v_temp_pd);
  }

  vmm_map((uintptr_t)v_pd, pdpt[pdpt_idx] & ~0xFFF, 1, PTE_WRITABLE | PTE_PRESENT);


  if (!(v_pd[pd_idx] & PTE_PRESENT)) {
    uint64_t *pt = (uint64_t *)pmm_alloc_frame();
    uintptr_t v_temp_pt = TEMP_MAPPING_BASE;
    vmm_map(v_temp_pt, (uintptr_t)pt, 1, PTE_WRITABLE | PTE_PRESENT | PTE_USER);
    memset((uint64_t*)v_temp_pt, 0, PAGE_SIZE);
    v_pd[pd_idx] = (uint64_t)(uintptr_t)pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pmm_ref_count[(uintptr_t)pt / PMM_PAGE_SIZE]++;
    vmm_unmap(v_temp_pt);
  }

  vmm_map((uintptr_t)v_pt, v_pd[pd_idx] & ~0xFFF, 1, PTE_WRITABLE | PTE_PRESENT);


  v_pt[pt_idx] = paddr | flags;

  pmm_ref_count[paddr / PMM_PAGE_SIZE]++;

  // vmm_unmap((uintptr_t)v_pd);
  // vmm_unmap((uintptr_t)v_pt);
  // vmm_unmap((uintptr_t)pdpt);
}

void vmm_unmap_user(uintptr_t utable, uintptr_t virt) {
  uint64_t *pdpt = (uint64_t *)TEMP_MAPPING_TOP;
  vmm_map((uintptr_t)pdpt, utable, 1, PTE_PRESENT | PTE_WRITABLE);

  uint32_t vaddr = virt;
  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pd_idx   = (vaddr >> 21) & 0x1FF;
  size_t pt_idx   = (vaddr >> 12) & 0x1FF;

  if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
    vmm_unmap((uintptr_t)pdpt);
    return;
  }

  uint64_t *v_pd = (uint64_t *)(TEMP_MAPPING_BASE + PAGE_SIZE);
  vmm_map((uintptr_t)v_pd, pdpt[pdpt_idx] & ~0xFFFull, 1,
          PTE_PRESENT | PTE_WRITABLE);

  if (!(v_pd[pd_idx] & PTE_PRESENT)) {
    vmm_unmap((uintptr_t)v_pd);
    vmm_unmap((uintptr_t)pdpt);
    return;
  }

  uint64_t *v_pt = (uint64_t *)(TEMP_MAPPING_BASE + 2 * PAGE_SIZE);
  vmm_map((uintptr_t)v_pt, v_pd[pd_idx] & ~0xFFFull, 1,
          PTE_PRESENT | PTE_WRITABLE);

  uintptr_t phys = (uintptr_t)(v_pt[pt_idx] & ~0xFFFull);
  if (phys && !(--pmm_ref_count[phys / PMM_PAGE_SIZE]))
    pmm_free_frame((void *)phys);
  v_pt[pt_idx] = 0;
  invlpg((void *)(uintptr_t)vaddr);

  vmm_unmap((uintptr_t)v_pt);
  vmm_unmap((uintptr_t)v_pd);
  vmm_unmap((uintptr_t)pdpt);
}



kerror_t vmm_cpy_user_range(uintptr_t utable, uintptr_t src, uintptr_t dest, size_t len) {
  if (len > PAGE_SIZE) return KERR_INVAL;

  uint64_t* pdpt = (uint64_t*)TEMP_MAPPING_TOP;
  vmm_map((uintptr_t)pdpt, utable , 1, PTE_PRESENT | PTE_WRITABLE);
  uint32_t vaddr = dest;

  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pd_idx = (vaddr >> 21) & 0x1FF;
  size_t pt_idx = (vaddr >> 12) & 0x1FF;
  size_t offset = (vaddr) & 0xFFF;

  uint64_t *v_pd = (uint64_t*)(TEMP_MAPPING_BASE + PAGE_SIZE); 
  uint64_t *v_pt = (uint64_t*)(TEMP_MAPPING_BASE + 2 * PAGE_SIZE);
  uint8_t  *v_dest = (uint8_t*)(TEMP_MAPPING_BASE + 3 * PAGE_SIZE);

  vmm_map((uintptr_t)v_pd, pdpt[pdpt_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);
  vmm_map((uintptr_t)v_pt, v_pd[pd_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);

  vmm_map((uintptr_t)v_dest, v_pt[pt_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);
  memcpy(v_dest + offset, (uint8_t*)src, len);
  vmm_unmap((uintptr_t)v_dest);

  vmm_unmap((uintptr_t)v_pt);
  vmm_unmap((uintptr_t)v_pd);
  vmm_unmap((uintptr_t)pdpt);
  
  return 0;
}

kerror_t vmm_zero_user_range(uintptr_t utable, uintptr_t addr, size_t len) {
  if (len > PAGE_SIZE) return KERR_INVAL;
  uint64_t* pdpt = (uint64_t*)TEMP_MAPPING_BASE;
  vmm_map((uintptr_t)pdpt, utable , 1, PTE_PRESENT | PTE_WRITABLE);
  uint32_t vaddr = addr;

  size_t pdpt_idx = (vaddr >> 30) & 0x3;
  size_t pd_idx = (vaddr >> 21) & 0x1FF;
  size_t pt_idx = (vaddr >> 12) & 0x1FF;
  size_t offset = (vaddr) & 0xFFF;

  uint64_t *v_pd = (uint64_t*)(TEMP_MAPPING_BASE + PAGE_SIZE); 
  uint64_t *v_pt = (uint64_t*)(TEMP_MAPPING_BASE + 2 * PAGE_SIZE);
  uint8_t  *v_dest = (uint8_t*)(TEMP_MAPPING_BASE + 3 * PAGE_SIZE);

  vmm_map((uintptr_t)v_pd, pdpt[pdpt_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);
  vmm_map((uintptr_t)v_pt, v_pd[pd_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);


  vmm_map((uintptr_t)v_dest, v_pt[pt_idx] & ~0xFFF, 1, PTE_PRESENT | PTE_WRITABLE);
  memset(v_dest + offset, 0u, PAGE_SIZE - offset);
  vmm_unmap((uintptr_t)v_dest);

  vmm_unmap((uintptr_t)v_pt);
  vmm_unmap((uintptr_t)v_pd);
  vmm_unmap((uintptr_t)pdpt);
  
  return 0;

}

void arch_load_ptable(uintptr_t ptable) {
  asm volatile("mov %0, %%cr3" :: "r"(ptable) : "memory");
}
