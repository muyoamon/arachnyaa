#ifndef ARACHNYAA_MM_KSTACK_H_
#define ARACHNYAA_MM_KSTACK_H_

#include "mm/vmm.h"
#include <lib/stddef.h>
#include <stdint.h>



#define KSTACK_PAGES 2

typedef struct {
  uintptr_t top;    // stack pointer start
  uintptr_t base;   // lowest address (after guard)
  size_t size;  // usable bytes (exclude guards)
} kstack_t;




/**
 * @brief initialize kernel stack manager.
 */
void kstack_init();

/**
 * @brief allocate kernel stack. 
 *
 * @param size Size in bytes.
 * @return return kstrack struct .
 */
kstack_t kstack_alloc(size_t size);

/**
 * @brief free kernel stack.
 *
 * @param ks Pointer to kernel stack.
 */
void kstack_free(kstack_t *ks);





#endif // ARACHNYAA_MM_KSTACK_H_
