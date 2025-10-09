#include "process/thread.h"
#include "arch/irq.h"
#include "arch/x86/tss.h"
#include "mm/kheap.h"
#include "mm/kstack.h"
#include "mm/vmm.h"
#include <lib/string.h>
#include <stdint.h>

static void thread_trampoline(void (*fn)(void*), void *arg) {
  local_irq_enable();
  fn(arg);

  while (1) cpu_idle();
}


thread_t* thread_create(void (*fn)(void *), void *arg) {
  thread_t *t = kmalloc(sizeof *t);
  memset(t, 0, sizeof *t);

  kstack_t ks = kstack_alloc(KSTACK_PAGES * PAGE_SIZE);
  t->kstack_base = ks.base;
  t->kstack_top = ks.top;


  uint32_t *sp = (uint32_t*)t->kstack_top;
  
  *--sp = (uint32_t)(uintptr_t)arg;
  *--sp = (uint32_t)(uintptr_t)fn;
  *--sp = 0;

  t->regs.eip = (uint32_t)thread_trampoline;
  t->regs.esp = (uint32_t)sp;
  t->state = 0;

  return t;
}

static thread_t *current, *runq_head, *runq_tail;

void rq_push(thread_t *t) {
  t->next = NULL;
  if (!runq_tail) runq_head = runq_tail = t;
  else runq_tail = runq_tail->next = t;
}

thread_t* rq_pop(void) {
  thread_t *t = runq_head;
  if (t) {
    runq_head = t->next;
    if (!runq_head) runq_tail = NULL;
  }
  return t;
}

void thread_yield(void) {
  thread_t *prev = current;
  rq_push(prev);
  current = rq_pop();

  tss_set_kernel_stack(current->kstack_top);
  switch_to(prev, current);
}

void thread_init() {
  current = rq_pop();
  thread_t dummy = {0};
  switch_to(&dummy, current);
  for(;;) cpu_idle();
}
