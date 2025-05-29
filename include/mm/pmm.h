#ifndef ARACHNYAA_MM_PMM_H_
#define ARACHNYAA_MM_PMM_H_


#include <stdint.h>
#include <mm/multiboot.h> // For multiboot_info_t

#define PMM_PAGE_SIZE 4096 // 4KB page frames

// Type field for mmap_entry
typedef enum { 
  MEMORY_TYPE_FREE = 1,
  MEMORY_TYPE_RESERVED,         
  MEMORY_TYPE_ACPI_RECLAIMABLE,  
  MEMORY_TYPE_NVS, 
  MEMORY_TYPE_BADRAM            
} memory_type_t;

typedef struct {
  uint64_t base_addr;
  uint64_t length;
  memory_type_t type;
} memory_map_entry_t;






/**
 * @brief Initializes the physical memory manager.
 * @param mb_info Pointer to the Multiboot info structure.
 * @param kernel_code_start Start address of kernel code/data.
 * @param kernel_code_end End address of kernel code/data/bss (before PMM bitmap).
 */
void pmm_init(multiboot_info_t *mb_info, uintptr_t kernel_code_start, uintptr_t kernel_code_end);

/**
 * @brief Allocates a single physical page frame.
 * @return Physical address of the allocated frame, or NULL if no memory is available.
 */
void* pmm_alloc_frame(void);

/**
 * @brief Frees a previously allocated physical page frame.
 * @param frame_addr Physical address of the frame to free.
 */
void pmm_free_frame(void* frame_addr);

/**
 * @brief Gets the total detected physical memory in bytes.
 */
uint64_t pmm_get_total_memory_bytes(void);

/**
 * @brief Gets the total free physical memory in bytes.
 */
uint64_t pmm_get_free_memory_bytes(void);

#endif // ARACHNYAA_PMM_PMM_H_
