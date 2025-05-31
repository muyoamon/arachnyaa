#ifndef ARACHNYAA_MM_PAGING_H_
#define ARACHNYAA_MM_PAGING_H_



#include <stddef.h>
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

// initalize paging
void vmm_init(void);

void vmm_map(uintptr_t virt, uintptr_t phys, size_t count, uint64_t flags);

void vmm_unmap(uintptr_t virt);

#endif // ARACHNYAA_MM_PAGING_H_
