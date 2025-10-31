#ifndef ARACHNYAA_PROCESS_THREAD_H_
#define ARACHNYAA_PROCESS_THREAD_H_

#include "arch/context.h"
#include "mm/kstack.h"
#include <stdint.h>

struct process;

/**
 * struct ctx - thread context
 */
typedef uint32_t tid_t;

typedef enum tstate {
  T_READY = 0,
  T_RUNNING,
  T_BLOCKED,
  T_SLEEP,
  T_DEAD,       // dead
  T_TERM,       // terminated
} tstate_t;

/**
 * struct thread - thread info
 */
typedef struct thread {
  tid_t tid;
  arch_context_t *ctx;
  struct process* proc;
  kstack_t kstack;
  tstate_t state;
  int priority;

  struct thread *next;
} thread_t;


/**
 * @brief Allocate an empty thread.
 *
 * @return pointer to allocated thread.
 */
thread_t* thread_alloc(void);

/**
 * @brief Free the thread object.
 *
 * @param[in] t Pointer to thread object.
 */
void thread_free(thread_t *t);


/**
 * @brief Setup kernel thread trampoline.
 *
 * @param[in/out] t pointer to thread.
 * @param[in] entry Kernel thread entry point.
 * @param[in] args Arguments.
 */
void thread_ksetup(thread_t *t, void (*entry)(void*), void* args);

/**
 * @brief Function to be called to finish thread.
 *
 */
void thread_exit(void);



#endif // ARACHNYAA_PROCESS_THREAD_H_
