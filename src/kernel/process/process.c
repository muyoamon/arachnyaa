
#include "process/process.h"
#include "arch/x86/defs.h"
#include "kernel/elf_loader.h"
#include "kernel/error.h"
#include "kernel/mm.h"
#include "mm/kheap.h"
#include "drivers/tty.h"
#include "mm/kstack.h"
#include "process/thread.h"
#include <lib/stddef.h>
#include <stdbool.h>
#include <lib/string.h>
#include <stdint.h>



volatile process_t *current_task = NULL;
volatile process_t *ready_queue_head = NULL;
static int32_t next_pid = 1;

/*
 * @brief Initializes the multitasking system
 */
void process_init(void) {
  current_task = NULL;
  ready_queue_head = NULL;
  tty_writestring("multitasking initialized.\n");
}

process_t* process_alloc(void) {
process_t *p = (process_t*)kmalloc(sizeof(process_t));
  if (!p) return NULL;
  memset(p, 0, sizeof(process_t));

  return p;
}

// TODO:
void process_free(process_t *proc) {
  if (proc->mm) {
    mm_free(proc->mm);
  }
  kfree(proc);
}



/*
 * @brief Creates a new kernel task.
 * @param entry_point Pointer to the function the task should start executing.
 */
process_t *process_create_kernel_process(void (*entry_point)(void)) {
  process_t *new_task = (process_t *)kmalloc(sizeof(process_t));
  if (!new_task)
    return NULL;
  memset(new_task, 0, sizeof(process_t));

  new_task->pid = next_pid++;
  new_task->state = PROCESS_STATE_READY;


  bool succeed = process_context_init(new_task, entry_point);
  if (!succeed) {
    return NULL;
  }

  return new_task;
}



process_t* process_spawn_from_elf(const elf_image_t* img, const char* argv0) {
  elf_load_result_t load;
  process_t* p = process_alloc();
  if (!p) return NULL;

  if (elf32_load_image(img, p->mm, &load)) {
    process_free(p);
    return NULL;
  }

  p->mm = mm_create();
  
  uintptr_t user_sp = 0u;
  if(elf_setup_user_stack(p->mm, USER_STACK_TOP, argv0, &user_sp)) {
    process_free(p);
    return NULL;
  }

  // setup main thread.
  p->main = thread_alloc();
  p->main->proc = p;
  kstack_t ks = kstack_alloc(KSTACK_DEFAULT_SIZE);
  p->main->kstack_top = ks.top;
  p->main->kstack_base = ks.base;
  

  return p;
}


bool process_context_init(process_t *task, void (*entry_point)(void)) {
  return task || entry_point;
}
