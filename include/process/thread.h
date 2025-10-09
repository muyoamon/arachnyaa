#ifndef ARACHNYAA_PROCESS_THREAD_H_
#define ARACHNYAA_PROCESS_THREAD_H_

#include <stdint.h>

/**
 * struct ctx - thread context
 */
typedef struct ctx {
  uint32_t ebp, ebx, esi, edi;
  uint32_t eip; /** resume point */
  uint32_t esp; /** resume stack */
} ctx_t;

/**
 * struct thread - thread info
 */
typedef struct thread {
  ctx_t regs;
  uintptr_t kstack_base;
  uintptr_t kstack_top;
  int state;  /** 0 = runnable, 1 = exited*/
  struct thread *next;
} thread_t;

extern void switch_to(thread_t *prev, thread_t *next);

thread_t* thread_create(void (*fn)(void*), void *arg);

void thread_yield(void);

void rq_push(thread_t *t);

thread_t* rq_pop(void);

void thread_init();
#endif // ARACHNYAA_PROCESS_THREAD_H_
