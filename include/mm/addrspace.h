#ifndef ARACHNYAA_MM_ADDRSPACE_H_
#define ARACHNYAA_MM_ADDRSPACE_H_

/**
 *  Architecture-agnostic address space header.  
 */

#include "mm/vmm.h"
#include <lib/stddef.h>
#include <stdint.h>

typedef struct addr_space addr_space_t;

/**
 * @brief Create new user address space.
 *
 * @return Pointer to created address space.
 */
addr_space_t* as_create(void);

/**
 * @brief Map virtual address in user address space.
 *
 * @param[in] mm Pointer to address space.
 * @param[in] virt Virtual address to map.
 * @param[in] size Size in bytes (page-rounded).
 * @param[in] flags PTE flags
 * @return 0 if success; non-zero otherwise.
 */
vmm_error_code_t as_map_user(addr_space_t *mm, uintptr_t virt, size_t size, uint64_t flags);

/**
 * @brief Unmap virtual address in user address space.
 *
 * @param[in] mm Pointer to address space.
 * @param[in] virt Virtual address to unmap.
 * @param[in] size Size in bytes (page-rounded).
 * @return 0 if success; non-zero otherwise.
 */
vmm_error_code_t as_unmap_user(addr_space_t *mm, uintptr_t virt, size_t size);

/**
 * @brief Map user stack.
 *
 * @param[in] mm Pointer to user addres space
 * @param[out] out_ustack_top Pointer to user stack top.
 * @return 0 if success, non-zero otherwise.
 */
vmm_error_code_t as_map_user_stack(addr_space_t *mm, uintptr_t *out_ustack_top);

/**
 * @brief Map virtual address to exact physical address in address space.
 *
 * @param[in] mm Pointer to user address space.
 * @param[in] virt Virtual address.
 * @param[in] phys Physical address.
 * @param[in] flags PTE flags.
 */
void as_map_user_exact(addr_space_t *mm, uintptr_t virt, uintptr_t phys, uint64_t flags);

/**
 * @brief Load user address space.
 *
 * @param[in] mm Pointer to user address space.
 */
void as_load_address_space(addr_space_t *mm);




#endif // ARACHNYAA_MM_ADDRSPACE_H_
