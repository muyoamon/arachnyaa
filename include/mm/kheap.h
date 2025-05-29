#ifndef ARACHNYAA_MM_KHEAP_H_
#define ARACHNYAA_MM_KHEAP_H_

#include <stddef.h>
/**
 *  @brief Initialize the kernel heap
 *  should be called after pmm_init
 *  @param initial_heap_start Physical address of the start of the initial heap
 *  @param initial_heap_size Size of the initial heap area in bytes
 * 
 */
void kheap_init(void* initial_heap_start, size_t initial_heap_size);

/**
 * @brief Allocates a chunk of memory from the kernel heap.
 * @param size The number of bytes to allocate.
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 * The returned pointer is to the usable memory area (after the header).
 */
void* kmalloc(size_t size);

/**
 * @brief Frees a previously allocated chunk of memory.
 * @param ptr Pointer to the memory chunk to free (must have been returned by kmalloc).
 */
void kfree(void* ptr);

#endif // ARACHNYAA_MM_KHEAP_H_
