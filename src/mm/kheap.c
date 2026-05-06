// lib/src/kheap.c

#include "arch/x86/defs.h"
#include "mm/tracker.h"
#include "mm/vmm.h"
#include <mm/kheap.h>
#include <stdint.h>
#include <lib/string.h>

static kheap_t kernel_heap;


static inline uintptr_t align_up(uintptr_t val, size_t align) {
  return (uintptr_t)(val + (uintptr_t)align - 1) & ~(uintptr_t)(align - 1);
}

void kheap_init(uintptr_t heap_base) {
  kernel_heap.heap_start = heap_base;
  kernel_heap.heap_end = heap_base;
  kernel_heap.heap_max = heap_base + KHEAP_SIZE;
  kernel_heap.head = NULL;
}

static void split_block(kheap_block_t *block, size_t size) {
  if (block->size <= size + sizeof(kheap_block_t) + HEAP_ALIGN)
    return;

  // calculate new block position
  uintptr_t new_addr = (uintptr_t)block + sizeof(kheap_block_t) + size;
  new_addr = align_up(new_addr, HEAP_ALIGN);

  kheap_block_t *new_block = (kheap_block_t *)new_addr;
  new_block->size = block->size - size - sizeof(kheap_block_t);
  new_block->magic = KHEAP_MAGIC;
  new_block->free = true;

  // Insert into free list
  new_block->next = block->next;
  new_block->prev = block->prev;
  if (block->next)
    block->next->prev = new_block;
  block->next = new_block;

  // Update original block
  block->size = size;
  kernel_heap.free_blocks++;
}

static void coalesce_blocks(kheap_block_t *block) {
  // Coalesce with next block if free
  if (block->next && block->next->free) {
    block->size += sizeof(kheap_block_t) + block->next->size;
    block->next = block->next->next;
    if (block->next)
      block->next->prev = block;
    kernel_heap.free_blocks--;
  }

  // Coalesce with preious block if free
  if (block->prev && block->prev->free) {
    block->prev->size += sizeof(kheap_block_t) + block->size;
    block->prev->next = block->next;
    if (block->next)
      block->next->prev = block->prev;
    kernel_heap.free_blocks--;
  }
}

void *kmalloc(size_t size) {
  // align size and add header overhead.
  size = align_up(size, HEAP_ALIGN);
  kheap_block_t *block = kernel_heap.head;

  // First-fit search
  while (block) {
    if (block->free && block->size >= size) {
      // Split if remaining space is sufficient
      split_block(block, size);
      block->free = false;
      kernel_heap.used_blocks++;
      kernel_heap.free_blocks--;
      return (void *)((uintptr_t)block + sizeof(kheap_block_t));
    }
    block = block->next;
  }

  // No free block found, expanding heap
  uintptr_t new_end = kernel_heap.heap_end + sizeof(kheap_block_t) + size;
  if (new_end > kernel_heap.heap_max) {
    return NULL; // out of memory
  }
  block = (kheap_block_t *)kernel_heap.heap_end;
  block->magic = KHEAP_MAGIC;
  block->size = size;
  block->free = false;
  block->next = NULL;
  block->prev = NULL;

  // update heap state
  kernel_heap.heap_end = new_end;
  kernel_heap.total_blocks++;
  kernel_heap.used_blocks++;

  return (void *)((uintptr_t)block + sizeof(kheap_block_t));
}

void *kcalloc(size_t num, size_t size) {
  // TODO: better approach to guarantee no integer overflow.
  void* p = kmalloc(num*size);
  memset(p, (uint8_t)0, num * size);
  return p;
}

void *kzalloc(size_t size) {
  void* p = kmalloc(size);
  memset(p, (uint8_t)0, size);
  return p;
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  kheap_block_t *block =
      (kheap_block_t *)((uintptr_t)ptr - sizeof(kheap_block_t));

  // Validate magic number
  if (block->magic != KHEAP_MAGIC) {
    return;
  }

  block->free = true;
  kernel_heap.used_blocks--;
  kernel_heap.free_blocks++;

  // Coalesce adjacent free block
  coalesce_blocks(block);
}
