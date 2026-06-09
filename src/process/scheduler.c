#include "arch/context.h"
#include "arch/cpu.h"
#include "arch/mm.h"
#include "arch/user.h"
#include "arch/x86/tss.h"
#include "kernel/mm.h"
#include "kernel/time.h"
#include "kernel/user.h"
#include "lib/stddef.h"
#include "mm/kstack.h"
#include "process/process.h"
#include "process/rq.h"
#include "process/thread.h"
#include <process/scheduler.h>

static scheduler_t scheduler;
static thread_t *current_thread;

/* Kstack deferred from the last T_TERM switch. We cannot call kstack_free
   (which unmaps pages) while running on the dying thread's own stack.
   Instead we stash it here and release it at the top of the next
   scheduler_reschedule call, when we're guaranteed to be on a different stack. */
static kstack_t dying_kstack;
static bool     have_dying_kstack;

static inline rq_t *sched_get_highest_runnable_rq() {
  for (int i = 0; i < SCHED_PRIO_LEVELS; i++) {
    if (rq_peek(&scheduler.rq[i]) != NULL) {
      return &scheduler.rq[i];
    }
  }
  return NULL; // no runnable queue
}

static void idle_thread(void *arg) {
  (void)arg;

  arch_local_irq_enable();

  for (;;) {
    arch_cpu_idle();
  }

  // no return
}

// critical section
static inline void crit_enter(void) { arch_local_irq_disable(); }
static inline void crit_exit(void) { arch_local_irq_enable(); }

void scheduler_init(int time_slice_ms) {
  scheduler.time_slice_ms = time_slice_ms;
  timer_set_slice(100 / system_clock_hz * time_slice_ms);
  current_thread = NULL;

  process_t *idle = process_create_kernel_process(idle_thread);
  // set idle process's main thread to be of lowest priority.
  idle->main->priority = SCHED_PRIO_LEVELS - 1;
  scheduler_add(idle->main);
}

void scheduler_yield(void) {
  crit_enter();

  thread_t *old = current_thread;
  if (!old) {
    crit_exit();
    return;
  }

  // Put current back to ready.
  old->state = T_READY;
  rq_push(&scheduler.rq[old->priority], old);

  // Pick and switch
  thread_t *next = scheduler_pick_next();

  scheduler_switch(next);
  crit_exit();
}

thread_t *scheduler_pick_next(void) {
  crit_enter();
  rq_t *rq = sched_get_highest_runnable_rq();
  thread_t *t = rq_pop(rq);
  crit_exit();
  return t;
}

void scheduler_add(thread_t *t) {
  if (!t)
    return;
  crit_enter();
  if (t->priority > SCHED_PRIO_LEVELS-1) t->priority = SCHED_PRIO_LEVELS-1;
  int prio = t->priority;
  rq_push(&scheduler.rq[prio], t);
  crit_exit();
}

void scheduler_remove(tid_t tid) {
  crit_enter();
  for (int i = 0; i < SCHED_PRIO_LEVELS; i++) {
    rq_remove(&scheduler.rq[i], tid);
  }
  crit_exit();
}

void scheduler_switch(thread_t *next) {
  crit_enter();

  if (next == current_thread)
    return;
  if (next == NULL)
    return;
  thread_t *old_t = current_thread;
  current_thread = next;
  next->state = T_RUNNING;
  tss_set_kernel_stack(current_thread->kstack.top);

  if (old_t) {
    if (old_t->proc == next->proc) {
      // skip table switch
      arch_context_switch(old_t->ctx, current_thread->ctx);
      crit_exit();
    }
  }

  // table switch 
  as_load_ptable(next->proc->mm);

  arch_context_switch(old_t->ctx, current_thread->ctx);
  crit_exit();
}

void scheduler_reschedule(void) {
  crit_enter();

  /* Release any kstack deferred from the previous T_TERM switch.
     We are now executing on a different kernel stack, so unmapping the
     old pages is safe here. */
  if (have_dying_kstack) {
    kstack_free(&dying_kstack);
    have_dying_kstack = false;
  }

  thread_t *old_t = current_thread;

  if (!old_t) {
    thread_t *next = scheduler_pick_next();
    if (next) {
      current_thread = next;
      next->state = T_RUNNING;
      tss_set_kernel_stack(current_thread->kstack.top);
      as_load_ptable(current_thread->proc->mm);
      arch_context_first_switch(current_thread->ctx);
    }
    crit_exit();
    return;
  }

  if (old_t->state == T_RUNNING) {
    old_t->state = T_READY;
    rq_push(&scheduler.rq[old_t->priority], old_t);
  } else if (old_t->state == T_TERM) {
    /* kstack_free (called inside thread_free) calls vmm_unmap on every stack
       page, removing them from the kernel page table.  We are still executing
       on this stack, so doing that now causes an immediate page fault.
       Stash the kstack for deferred release (see top of this function) and
       zero the field so thread_free's kstack_free call is a no-op. */
    dying_kstack    = old_t->kstack;
    have_dying_kstack = true;
    old_t->kstack   = (kstack_t){0};

    thread_free(old_t);
    thread_t *next = scheduler_pick_next();
    if (next) {
      current_thread = next;
      next->state = T_RUNNING;
      tss_set_kernel_stack(next->kstack.top);
      as_load_ptable(next->proc->mm);
      arch_context_first_switch(next->ctx);
    }
    crit_exit();
    return;
  }

  thread_t *next = scheduler_pick_next();

  scheduler_switch(next);
  crit_exit();
}

thread_t *scheduler_get_current() { return current_thread; }
