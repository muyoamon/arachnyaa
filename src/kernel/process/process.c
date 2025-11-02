
#include "process/process.h"
#include "arch/context.h"
#include "arch/x86/defs.h"
#include "drivers/tty.h"
#include "kernel/elf_loader.h"
#include "kernel/error.h"
#include "kernel/mm.h"
#include "kernel/user.h"
#include "mm/kheap.h"
#include "mm/kstack.h"
#include "process/thread.h"
#include <lib/stddef.h>
#include <lib/string.h>
#include <stdbool.h>
#include <stdint.h>

static int32_t next_pid = 1;

/*
 * @brief Initializes the multitasking system
 */

process_t *process_alloc(void) {
  process_t *p = (process_t *)kmalloc(sizeof(process_t));
  if (!p)
    return NULL;
  memset(p, 0, sizeof(process_t));

  return p;
}

// TODO:
void process_free(process_t *proc) {
  if (proc->mm) {
    as_free(proc->mm);
  }
  kfree(proc);
}

/*
 * @brief Creates a new kernel task.
 * @param entry_point Pointer to the function the task should start executing.
 */
process_t *process_create_kernel_process(void (*entry_point)(void *)) {
  process_t *new_task = (process_t *)kmalloc(sizeof(process_t));
  if (!new_task)
    return NULL;
  memset(new_task, 0, sizeof(process_t));

  new_task->pid = next_pid++;
  new_task->mm = as_create();

  thread_t *main_thread = thread_alloc();
  thread_ksetup(main_thread, entry_point, NULL);
  new_task->main = main_thread;
  new_task->main->proc = new_task;
  
  return new_task;
}

process_t *process_spawn_from_elf(const elf_image_t *img, const char *argv0) {
  elf_load_result_t load;
  process_t *p = process_alloc();
  if (!p)
    return NULL;

  p->pid = next_pid++;
  p->mm = as_create();
  
  if (elf32_load_image(img, p->mm, &load)) {
    process_free(p);
    return NULL;
  }

  uintptr_t user_sp = 0u;
  if (elf_setup_user_stack(p->mm, USER_STACK_TOP, argv0, &user_sp)) {
    process_free(p);
    return NULL;
  }

  // setup main thread.
  p->main = thread_alloc();
  thread_usetup(p->main, (void*)load.entry_va, (void*)user_sp);
  p->main->proc = p;

  return p;
}

