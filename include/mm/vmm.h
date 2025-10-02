#ifndef ARACHNYAA_MM_VMM_H_
#define ARACHNYAA_MM_VMM_H_



#include "mm/tracker.h"
#include <lib/stddef.h>
#include <stdint.h>

#define PTE_PRESENT   (1 << 0)
#define PTE_WRITABLE  (1 << 1)
#define PTE_USER      (1 << 2)
#define PTE_WRITETHRU (1 << 3)
#define PTE_CACHE_DISABLE (1 << 4)
#define PTE_ACCESSED  (1 << 5)
#define PTE_DIRTY     (1 << 6)
#define PTE_HUGE_PAGE (1 << 7)
#define PTE_GLOBAL    (1 << 8)
#define PTE_NX        (1ULL << 63) 

#define PAGE_SIZE 4096



typedef struct {
  uintptr_t base; // inclusive
  uintptr_t top;  // exclusive
  tracker_tree_t free_map;
} vmm_region_t;






//
// --- Low-level VMM functions ---
//

// initalize paging
void vmm_init(void);

// map virtual address to physical address
void vmm_map(uintptr_t virt, uintptr_t phys, size_t count, uint64_t flags);

// unmap virtual address
void vmm_unmap(uintptr_t virt);

// clear identity map
void vmm_clear_identity_map(void);

// get physical address
uintptr_t vmm_get_phys_addr(uintptr_t virt);



//
// --- High Level VMM functions ---
//

//
// Direct Allocation
//

// allocate virtual address to arbitrary physical address
int vmm_alloc(uintptr_t virt, size_t pages, uint64_t flags);


// free virtual address
void vmm_free(uintptr_t virt, size_t pages);


//
// Address Reservation
//

bool vmm_reserve(vmm_region_t *r, uintptr_t base, size_t size);
bool vmm_release(vmm_region_t *r, uintptr_t base, size_t size);

//
// Regional Allocation
//

bool vmm_alloc_region(vmm_region_t *r, size_t size, 
                      size_t align, uint64_t flags, 
                      uint32_t vmm_flags, uintptr_t* out_addr);

#endif // ARACHNYAA_MM_VMM_H_
