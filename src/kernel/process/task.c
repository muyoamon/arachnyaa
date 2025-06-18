
#include "process/task.h"
#include "mm/kheap.h"
#include "mm/layout.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "tty.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
volatile task_t *current_task = NULL;
volatile task_t *ready_queue_head = NULL;
static int32_t next_pid = 1;

/*
 * @brief Initializes the multitasking system
 */
void tasking_init(void) {
  current_task = NULL;
  ready_queue_head = NULL;
  tty_writestring("multitasking initialized.\n");
}

/*
 * @brief Creates a new kernel task.
 * @param entry_point Pointer to the function the task should start executing.
 */
task_t *create_kernel_task(void (*entry_point)(void)) {
  task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
  if (!new_task)
    return NULL;
  memset(new_task, 0, sizeof(task_t));

  new_task->id = next_pid++;
  new_task->state = TASK_STATE_READY;

  // Allocate a kernel stack for this task
  // Let's use 2 pages (8KB) for the kernel stack for now
  new_task->kernel_stack_bottom = (uintptr_t)pmm_alloc_frame();
  if (!new_task->kernel_stack_bottom) {
    kfree(new_task);
    return NULL;
  }
  void *second_page = pmm_alloc_frame();
  if (!second_page) {
    pmm_free_frame((void *)new_task->kernel_stack_bottom);
    kfree(new_task);
    return NULL;
  }
  vmm_map((uintptr_t)(kernel_stack_region_base + ((new_task->id * 2) * PAGE_SIZE)),
          new_task->kernel_stack_bottom, 1, PTE_PRESENT | PTE_WRITABLE);
  vmm_map((uintptr_t)(kernel_stack_region_base + ((new_task->id * 2 + 1) * PAGE_SIZE)),
          (uintptr_t)second_page, 1, PTE_PRESENT | PTE_WRITABLE);
  new_task->kernel_stack_top = (uintptr_t)(kernel_stack_region_base + ((new_task->id * 2 + 1) * PAGE_SIZE));
  new_task->kernel_stack_bottom = (uintptr_t)(kernel_stack_region_base + ((new_task->id * 2 + 1) * PAGE_SIZE));


}
