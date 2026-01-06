#ifndef ARACHNYAA_KERNEL_SPINLOCK_H_
#define ARACHNYAA_KERNEL_SPINLOCK_H_

#define SPINLOCK_INIT {.locked = 0;}

typedef struct {
  volatile int locked;
} spinlock_t;

void spin_lock(spinlock_t *lock);

void spin_unlock(spinlock_t* lock);




#endif // ARACHNYAA_KERNEL_SPINLOCK_H_
