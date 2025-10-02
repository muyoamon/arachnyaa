// arachnyaa/src/kernel/pmm.c
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <stdbool.h>
#include <lib/stddef.h> // For NULL
#include <stdint.h>
#include <lib/string.h> // For memset 
#include <drivers/tty.h>    // For debug prints


extern uintptr_t vmm_get_phys_addr(uintptr_t virt);

// --- Bitmap PMM ---

uint64_t pmm_total_pages;

uint8_t *pmm_bitmap;
static uint8_t early_pmm_bitmap[PMM_MAX_PAGES / 8] = {
    0}; // Statically allocated bitmap

uint16_t *pmm_ref_count;
static uint16_t early_pmm_ref_count[PMM_MAX_PAGES] = {0};

#define PHYS_TO_VIRT(p) ((uintptr_t)(p) + (0xC0000000 - 0x100000))

static uint32_t total_memory_pages = 0;
static uint32_t used_memory_pages = 0;
static uintptr_t highest_detected_addr = 0;

// Helper to set a bit in the bitmap (marks page as used)
static void pmm_bitmap_set(size_t page_idx) {
  if (page_idx >= total_memory_pages)
    return;
  pmm_bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

// Helper to clear a bit in the bitmap (marks page as free)
static void pmm_bitmap_clear(size_t page_idx) {
  if (page_idx >= total_memory_pages)
    return;
  pmm_bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

// Helper to test if a bit is set (page is used)
static bool pmm_bitmap_test(size_t page_idx) {
  if (page_idx >= total_memory_pages)
    return true; // Treat out of bounds as used
  return (pmm_bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

// Marks a region of memory as used (rounds to page boundaries)
static void pmm_mark_region_used(uintptr_t base, size_t size) {
  uintptr_t start_page_addr = base / PMM_PAGE_SIZE;
  uintptr_t end_addr = base + size - 1; // -1 to get the last byte of the region
  uintptr_t end_page_addr = end_addr / PMM_PAGE_SIZE;

  tty_writestring("Marking used: ");
  tty_write_hex(base);
  tty_writestring(" - ");
  tty_write_hex(base + size);
  tty_writestring(" (Pages ");
  tty_write_dec(start_page_addr);
  tty_writestring(" to ");
  tty_write_dec(end_page_addr);
  tty_writestring(")\n");

  for (size_t i = start_page_addr; i <= end_page_addr; ++i) {
    if (!pmm_bitmap_test(i)) { // Only count if it wasn't already marked
      pmm_bitmap_set(i);
      used_memory_pages++;
    }
  }
}

void pmm_init(multiboot_info_t *mb_info, uintptr_t kernel_code_start,
              uintptr_t kernel_code_end) {
  pmm_bitmap = early_pmm_bitmap;
  pmm_ref_count = early_pmm_ref_count;
  kernel_code_end = kernel_code_end - 0xC0000000 + 0x100000;

  tty_writestring("PMM: Initializing Physical Memory Manager...\n");

  pmm_parse_mmap(mb_info, kernel_code_start, kernel_code_end, PMM_MAX_PAGES);

  tty_writestring("PMM: Initialization complete. Free pages: ");
  tty_write_dec(total_memory_pages - used_memory_pages);
  tty_putc('\n');
}

void *pmm_alloc_frame(void) {
  for (size_t i = 0; i < total_memory_pages; ++i) {
    if (!pmm_bitmap_test(i)) {
      pmm_bitmap_set(i);
      used_memory_pages++;
      return (void *)(i * PMM_PAGE_SIZE);
    }
  }
  tty_writestring("PMM: Out of memory!\n");
  return NULL; // Out of memory
}

void pmm_free_frame(void *frame_addr) {
  if (frame_addr == NULL)
    return;
  uintptr_t addr = (uintptr_t)frame_addr;
  size_t page_idx = addr / PMM_PAGE_SIZE;

  if (page_idx < total_memory_pages) {
    if (pmm_bitmap_test(page_idx)) { // Only free if it was used
      pmm_bitmap_clear(page_idx);
      used_memory_pages--;
    } else {
      // tty_writestring("PMM: Warning! Freeing already free page: 0x");
      // tty_write_hex(addr); tty_putc('\n');
    }
  } else {
    // tty_writestring("PMM: Warning! Freeing out of bounds page: 0x");
    // tty_write_hex(addr); tty_putc('\n');
  }
}

uint64_t pmm_get_total_memory_bytes(void) {
  return (uint64_t)total_memory_pages * PMM_PAGE_SIZE;
}

uint64_t pmm_get_free_memory_bytes(void) {
  return (uint64_t)(total_memory_pages - used_memory_pages) * PMM_PAGE_SIZE;
}

void pmm_parse_mmap(multiboot_info_t *mb_info, uintptr_t kernel_code_start,
                    uintptr_t kernel_code_end, size_t max_pages) {
  tty_writestring("PMM: Multiboot info struct at physical address: 0x");
  tty_write_hex((uintptr_t)mb_info); // Assuming mb_info is already the
                                     // correct virtual ptr if paging were on
  tty_writestring("\n");

  if (!mb_info) {
    tty_writestring("PMM: FATAL - Multiboot info pointer is NULL!\n");
    for (;;)
      asm("hlt");
  }

  tty_writestring("PMM: Multiboot flags: 0x");
  tty_write_hex(mb_info->flags);
  tty_writestring("\n");

  uint64_t end_of_highest_region = 0;

  // 1. Find the highest memory address from Multiboot map
  if (!(mb_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
    tty_writestring("PMM: No Multiboot memory map (mmap) available! Using "
                    "mem_lower/upper.\n");
    if (mb_info->flags & MULTIBOOT_INFO_MEMORY) {
      end_of_highest_region =
          (mb_info->mem_upper * 1024) + (1024 * 1024) - 1; // Approx
      tty_writestring("PMM: Using mem_upper, highest_address approx: 0x");
      tty_write_hex(end_of_highest_region);
      tty_writestring("\n");
    } else {
      tty_writestring("PMM: FATAL - No memory info at all! Halting.\n");
      for (;;)
        asm("hlt");
    }
  } else {
    tty_writestring("PMM: Memory map present at 0x");
    tty_write_hex(mb_info->mmap_addr);
    tty_writestring(" with length ");
    tty_write_dec(mb_info->mmap_length);
    tty_writestring("\n");

    end_of_highest_region = 0;
    multiboot_mmap_entry_t *mmap =
        (multiboot_mmap_entry_t *)(uintptr_t)mb_info->mmap_addr;
    for (uint32_t i = 0; i < mb_info->mmap_length;) {
      multiboot_mmap_entry_t *entry =
          (multiboot_mmap_entry_t *)((uintptr_t)mmap + i);
      tty_writestring("  MMap Entry: addr=0x");
      tty_write_hex((uint32_t)entry->addr);
      tty_writestring(" len=0x");
      tty_write_hex((uint32_t)entry->len);
      tty_writestring(" type=");
      tty_write_dec(entry->type);
      tty_writestring(" size_field=");
      tty_write_dec(entry->size);
      tty_writestring("\n");

      if (entry->addr + entry->len > end_of_highest_region) {
        end_of_highest_region = entry->addr + entry->len - 1;
      }
      if (entry->size == 0) { // Avoid infinite loop if size is 0
        tty_writestring("PMM: Warning! Mmap entry size is 0. Advancing by "
                        "default minimum.\n");
        i += sizeof(multiboot_mmap_entry_t) - sizeof(uint32_t); // Heuristic
      } else {
        i +=
            entry->size + sizeof(uint32_t); // entry->size is size of struct
                                            // *excluding* the size field itself
      }
    }
    tty_writestring("PMM: Calculated end_of_highest_region from mmap: 0x");
    tty_write_hex((uint32_t)end_of_highest_region);
    tty_writestring("\n");
  }

  if (end_of_highest_region == 0) {
    tty_writestring("PMM: Error! No valid memory regions foud. Using default "
                    "16MB for bitmap sizing");
    total_memory_pages = (16 * 1024 * 1024) / PMM_PAGE_SIZE;
    highest_detected_addr = (16 * 1024 * 1024) - 1;
  } else {
    total_memory_pages = (uint32_t)(end_of_highest_region / PMM_PAGE_SIZE);
    if (end_of_highest_region % PMM_PAGE_SIZE != 0) {
      total_memory_pages++;
    }
    highest_detected_addr = end_of_highest_region - 1;
  }
  tty_writestring("PMM: Inital total pages calculated: ");
  tty_write_dec(total_memory_pages);
  tty_putc('\n');
  pmm_total_pages = total_memory_pages;

  if (total_memory_pages > max_pages) {
    tty_writestring("PMM: Warning! Detected memory exceeds bitmap "
                    "capacity. Clamping to ");
    tty_write_dec(max_pages);
    tty_writestring(" pages.\n");
    total_memory_pages = max_pages;
    highest_detected_addr = (uintptr_t)total_memory_pages * PMM_PAGE_SIZE - 1;
  }
  tty_writestring("PMM: Total pages for bitmap: ");
  tty_write_dec(total_memory_pages);
  tty_writestring("\n");
  
  // Initialize bitmap: Mark ALL pages as RESERVED/USED initially.
  // We will then iterate through the memory map and mark AVAILABLE regions as
  // FREE. Using 0xFF means all bits are 1 (used).
  for (size_t i = 0; i < (total_memory_pages + 7) / 8; ++i) {
    pmm_bitmap[i] = 0xFF;
  }
  used_memory_pages = total_memory_pages; // Assume all used initially

  // 2. Parse Multiboot memory map and mark available RAM as free
  if ((mb_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
    tty_writestring("PMM: Parsing Multiboot memory map...\n");
    multiboot_mmap_entry_t *mmap_entry =
        (multiboot_mmap_entry_t *)mb_info->mmap_addr;
    while ((uintptr_t)mmap_entry < mb_info->mmap_addr + mb_info->mmap_length) {
      tty_writestring("  Region: addr=");
      tty_write_hex((uint32_t)mmap_entry->addr);
      tty_writestring(" len=");
      tty_write_hex((uint32_t)mmap_entry->len);
      tty_writestring(" type=");
      tty_write_dec(mmap_entry->type);
      tty_putc('\n');

      if (mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
        uintptr_t region_start_page = mmap_entry->addr / PMM_PAGE_SIZE;
        // Ensure addr is page aligned for start, or round up
        if (mmap_entry->addr % PMM_PAGE_SIZE != 0) {
          region_start_page++;
        }
        uintptr_t region_end_page =
            (mmap_entry->addr + mmap_entry->len) / PMM_PAGE_SIZE;

        for (size_t i = region_start_page;
             i < region_end_page && i < total_memory_pages; ++i) {
          if (pmm_bitmap_test(i)) { // If it was marked used (globally)
            pmm_bitmap_clear(i);    // Mark as free
            used_memory_pages--;
          }
        }
      }
      mmap_entry =
          (multiboot_mmap_entry_t *)((uintptr_t)mmap_entry + mmap_entry->size +
                                     sizeof(mmap_entry->size));
    }
  } else {
    tty_writestring("PMM: WARNING - No Multiboot mmap. Assuming RAM 0-1MB and "
                    "kernel region only.\n");
    // Fallback: Mark 0-640KB as "available" then reserve kernel. Less ideal.
    // For now, since everything is marked used, this means only kernel + bitmap
    // will be usable.
  }

  // 3. Mark kernel region as used
  // kernel_code_end should ideally point to the end of your PMM bitmap if it's
  // static
  tty_writestring("PMM: Kernel region: ");
  tty_write_hex(kernel_code_start);
  tty_writestring(" - ");
  tty_write_hex(kernel_code_end);
  tty_putc('\n');
  pmm_mark_region_used(kernel_code_start, kernel_code_end - kernel_code_start);

  // 4. Mark the PMM bitmap itself as used.
  // This assumes pmm_bitmap is linked right after kernel_code_end.
  // Or if pmm_bitmap is static, kernel_code_end from linker should be after it.
  // Let's pass the bitmap location and size to pmm_mark_region_used.
  uintptr_t bitmap_addr = vmm_get_phys_addr((uintptr_t)pmm_bitmap);
  size_t bitmap_actual_size_bytes =
      (total_memory_pages + 7) / 8; // Actual size used
  tty_writestring("PMM: Bitmap region: ");
  tty_write_hex(bitmap_addr);
  tty_writestring(" - ");
  tty_write_hex(bitmap_addr + bitmap_actual_size_bytes);
  tty_putc('\n');
  pmm_mark_region_used(bitmap_addr, bitmap_actual_size_bytes);

  // Mark ref count pointer as used;
  uintptr_t refcount_addr = vmm_get_phys_addr((uintptr_t)pmm_ref_count);
  size_t refcount_actual_size_bytes =
      total_memory_pages * 2; // Actual size used
  tty_writestring("PMM: Bitmap region: ");
  tty_write_hex(refcount_addr);
  tty_writestring(" - ");
  tty_write_hex(refcount_addr + refcount_actual_size_bytes);
  tty_putc('\n');
  pmm_mark_region_used(refcount_addr, refcount_actual_size_bytes);


  // Mark mmap as used
  tty_writestring("PMM: MMAP region: ");
  tty_write_hex((uintptr_t)mb_info->mmap_addr);
  tty_writestring(" - ");
  tty_write_hex((uintptr_t)mb_info->mmap_addr + mb_info->mmap_length);
  tty_putc('\n');
  pmm_mark_region_used(mb_info->mmap_addr, mb_info->mmap_length);
  


  // Mark first page (0x0) as used (contains IVT, BDA, etc.)
  pmm_mark_region_used(0x0, PMM_PAGE_SIZE);

  tty_writestring("PMM: Initialization complete. Free pages: ");
  tty_write_dec(total_memory_pages - used_memory_pages);
  tty_putc('\n');
}


void pmm_set_used_memory_bytes(size_t new_size) {
  used_memory_pages = new_size / PMM_PAGE_SIZE;
}
