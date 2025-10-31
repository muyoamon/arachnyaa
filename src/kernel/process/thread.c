#include "process/thread.h"
#include "arch/context.h"
#include "arch/cpu.h"
#include "arch/x86/defs.h"
#include "drivers/tty.h"
#include "mm/kheap.h"
#include "mm/kstack.h"
#include "mm/vmm.h"
#include "process/scheduler.h"
#include <lib/string.h>
#include <stdint.h>

int next_tid = 0;



static void kthread_trampoline(void (*fn)(void*), void *args) {
  arch_local_irq_enable();

  fn(args);
  thread_exit(); // never return;
}


thread_t* thread_alloc(void) {
  thread_t *t = kmalloc(sizeof(thread_t));
  if (!t) return NULL;
  memset(t, 0, sizeof(thread_t));
  t->tid = next_tid++;
  return t;
}

void thread_free(thread_t *t) {
  arch_context_free(t->ctx);
  kstack_free(&t->kstack);
  kfree(t);
}


void thread_ksetup(thread_t *t, void (*entry)(void *), void *args) {
  kstack_t ks = kstack_alloc(KSTACK_DEFAULT_SIZE);

  t->kstack = ks;
  t->state = T_READY;

 void** sp = (void**)t->kstack.top;
  *--sp = args;
  *--sp = entry;
  *--sp = thread_exit;
  
  // initialize thread context
  t->ctx =
      arch_context_init((void *)kthread_trampoline, (void *)sp);
}

void thread_exit(void) {
  thread_t *current = scheduler_get_current();

  current->state = T_DEAD;

  // debug
  tty_writestring("thread exited\n");


  scheduler_reschedule();
}


