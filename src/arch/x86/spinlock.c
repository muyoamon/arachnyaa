
#include "kernel/spinlock.h"
void spin_lock(spinlock_t *lock) {
  for (;;) {
    int old;
    asm volatile (
      "xchg %0, %1"
      : "=r"(old), "+m"(lock->locked)
      : "0"(1)
      : "memory"
    );

    if (old == 0) {
      // got the lock
      return;
    }

    while (lock->locked) {
      asm volatile ("pause");
    }
  }
}

void spin_unlock(spinlock_t *lock) {
  asm volatile ("" ::: "memory");
  lock->locked = 0;
}
