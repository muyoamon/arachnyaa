#ifndef ARACHNYAA_MM_KHEAP_H_
#define ARACHNYAA_MM_KHEAP_H_

#include <stdbool.h>
#include <lib/stddef.h>
#include <stdint.h>

#define KHEAP_MAGIC 0xDEADBEEF
#define HEAP_ALIGN 16

typedef struct kheap_block {
  uint32_t magic;
  size_t size;
  bool free;
  struct kheap_block *next;
  struct kheap_block *prev;
} kheap_block_t;

typedef struct {
  uint32_t magic;
  kheap_block_t *header;
} kheap_footer_t;

typedef struct {
  kheap_block_t *head;
  uintptr_t heap_start;
  uintptr_t heap_end;
  uintptr_t heap_max;
  size_t total_blocks;
  size_t free_blocks;
  size_t used_blocks;
} kheap_t;




/**
 *  @brief Initialize the kernel heap
 *  should be called after pmm_init
 *  @param initial_heap_start address of the start of the initial heap
 * 
 */
void kheap_init(uintptr_t heap_base);

/**
 * @brief Allocates a chunk of memory from the kernel heap.
 * @param size The number of bytes to allocate.
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 * The returned pointer is to the usable memory area (after the header).
 */
void* kmalloc(size_t size);


/**
 * @brief Allocates a contiguous chunk of memory from the kernel heap as an array.
 *
 * @param[in] num number of elements.
 * @param[in] size size of each element in bytes.
 */
void* kcalloc(size_t num, size_t size);



/**
 * @brief Allocates a chunk of memory and set to zero.
 *
 * @param[in] size The number of bytes to allocate.
 */
void* kzalloc(size_t size);

/**
 * @brief Frees a previously allocated chunk of memory.
 * @param ptr Pointer to the memory chunk to free (must have been returned by kmalloc).
 */
void kfree(void* ptr);

#endif // ARACHNYAA_MM_KHEAP_H_
