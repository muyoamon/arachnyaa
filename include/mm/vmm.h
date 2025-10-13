#ifndef ARACHNYAA_MM_VMM_H_
#define ARACHNYAA_MM_VMM_H_

#include "mm/tracker.h"
#include <lib/stddef.h>
#include <stdint.h>

#define PTE_PRESENT (1 << 0)
#define PTE_WRITABLE (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_WRITETHRU (1 << 3)
#define PTE_CACHE_DISABLE (1 << 4)
#define PTE_ACCESSED (1 << 5)
#define PTE_DIRTY (1 << 6)
#define PTE_HUGE_PAGE (1 << 7)
#define PTE_GLOBAL (1 << 8)
#define PTE_NX (1ULL << 63)

#define PAGE_SIZE 4096

typedef struct {
  uintptr_t base; // inclusive
  uintptr_t top;  // exclusive
  tracker_tree_t free_map;
} vmm_region_t;

enum vmm_flags {
  VMM_NONE = 0,

  // placement
  VMM_FIXED = 1ull << 0, // fixed
  VMM_HINT = 1ull << 1,  // with hint
  VMM_AUTO = 1ull << 2,  // anywhere

  // backing/commit
  VMM_MAP_ANON = 1ull << 8,         // no file
  VMM_MAP_FILE = 1ull << 9,         // file-backed
  VMM_MAP_PRIVATE = 1ull << 10,     // copy-on-write
  VMM_MAP_SHARED = 1ull << 11,      // shared mapping
  VMM_MAP_LAZY_COMMIT = 1ull << 12, // demand-zero
  VMM_MAP_POPULATE = 1ull << 13,    // eager commit

  // guard/stack
  VMM_GUARD_BELOW = 1ull << 16,
  VMM_GUARD_ABOVE = 1ull << 17,
  VMM_STACK_GROWSDOWN = 1ull << 18,

  // page size
  VMM_PAGE_4K = 1ull << 21,
  VMM_PAGE_2M = 1ull << 22,

  // physical
  VMM_PHYS_CONTIG = 1ull << 25,
  VMM_PHYS_LOW32 = 1ull << 26,

  // debug
  VMM_DEBUG_POISON = 1ull << 40,
  VMM_DEBUG_CANARY = 1ull << 41,
};

typedef uint64_t vmm_flags_t;

enum vmm_prot{
  VMM_PROT_NONE = 0,
  VMM_PROT_READ = 1u << 0,
  VMM_PROT_WRITE = 1u << 1,
  VMM_PROT_EXEC = 1u << 2,
  VMM_PROT_USER = 1u << 3,
  VMM_PROT_GLOBAL = 1u << 4,
};

typedef uint32_t vmm_prot_t;

enum vmm_error_code {
  VMM_ERR_NONE  = 0,    // success; no error
  VMM_ERR_INVAL = -1,   // invalid flags/parameter
  VMM_ERR_PERM  = -2,   // invalid permission
  VMM_ERR_NOMEM = -3,   // no memory
};
typedef uint16_t vmm_error_code_t;

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
// User-space functions
//


/**
 * @brief create user page table.
 *
 * @return physical address of user page table.
 */
uintptr_t vmm_create_user_ptable(void);

/**
 * @brief Map virtual address to physical address in user page table.
 *
 * @param[in] utable address of user page table.
 * @param[in] virt virtual address to map.
 * @param[in] phys physical address to map to.
 * @param[in] flags page table flags.
 */
void vmm_map_user(uintptr_t utable, uintptr_t virt, uintptr_t phys, uint64_t flags);

/**
 * @brief Unmap virtual address in user page table.
 *
 * @param[in] utable address of user page table.
 * @param[in] virt virtual address to unmap.
 */
void vmm_unmap_user(uintptr_t utable, uintptr_t virt);

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
// User
//


/**
 * @brief Map a user region with flags; size is page-rounded.
 *
 * @param[in] utable address of user page table.
 * @param[in] virt virtual address to map.
 * @param[in] size size in bytes (page-rounded).
 * @param[in] flags pte flags.
 * @return 0 if success, non-zero otherwise.
 */
vmm_error_code_t vmm_map_user_range(uintptr_t utable, uintptr_t virt, size_t size, uint64_t flags);

/**
 * @brief Unmap a user region; size is page-rounded.
 *
 * @param[in] utable address of user page table.
 * @param[in] virt Virtual address to unmap.
 * @param[in] size Size in bytes (page-rounded).
 * @return 0 if success, non-zero otherwise.
 */
vmm_error_code_t vmm_unmap_user_range(uintptr_t utable, uintptr_t virt, size_t size);

//
// Address Reservation
//

uintptr_t vmm_reserve(vmm_region_t *r, size_t size, size_t align);
bool vmm_release(vmm_region_t *r, uintptr_t base, size_t size);

//
// Regional Allocation
//

/**
 * @brief Allocate memory in region.
 *
 * @param[in] r Pointer to region.
 * @param[in] size Size in bytes.
 * @param[in] prot_flags Protection flags.
 * @param[in] vmm_flags VMM flags.
 * @param[in/out] io_addr pointer to input/output address.
 * @return 0 if success, non-zero otherwise.
 */
vmm_error_code_t vmm_alloc_region(vmm_region_t *r, size_t size,
                                  vmm_prot_t prot_flags, vmm_flags_t vmm_flags,
                                  uintptr_t *io_addr);
/**
 * @brief free memory in region.
 *
 * @param[in]  r pointer to region.
 * @param[in]  base base address.
 * @param[in] size Size in bytes (guard included).
 * @param[in] guard_below Number of guard pages after base.
 * @param[in] guard_above Number of guard pages above base+size.
 * @return 0 if success, non-zero otherwise.
 */
vmm_error_code_t vmm_free_region(vmm_region_t *r, uintptr_t base, size_t size,
                                 size_t guard_below, size_t guard_above);

#endif // ARACHNYAA_MM_VMM_H_
