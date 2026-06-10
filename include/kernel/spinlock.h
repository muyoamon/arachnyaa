#ifndef ARACHNYAA_KERNEL_SPINLOCK_H_
#define ARACHNYAA_KERNEL_SPINLOCK_H_

#include <stdint.h>

#define SPINLOCK_INIT {.locked = 0;}

typedef struct {
  volatile int locked;
} spinlock_t;

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

/*
 * IRQ-safe spinlock variants: save+disable IRQs before acquiring, restore
 * after releasing. Required for locks shared between thread context (where
 * IRQs are enabled after a context switch) and interrupt handlers.
 */
static inline uint32_t _spin_flags_cli(void) {
  uint32_t f;
  __asm__ volatile("pushfl; popl %0; cli" : "=r"(f) :: "memory");
  return f;
}

static inline void _spin_flags_restore(uint32_t f) {
  __asm__ volatile("pushl %0; popfl" :: "r"(f) : "memory", "cc");
}

static inline void spin_lock_irqsave(spinlock_t *lock, uint32_t *flags) {
  *flags = _spin_flags_cli();
  spin_lock(lock);
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags) {
  spin_unlock(lock);
  _spin_flags_restore(flags);
}

#endif // ARACHNYAA_KERNEL_SPINLOCK_H_
