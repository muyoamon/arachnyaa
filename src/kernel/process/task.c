
#include "process/task.h"
#include "mm/kheap.h"
#include "drivers/tty.h"
#include <lib/stddef.h>
#include <stdbool.h>
#include <lib/string.h>

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
task_t *task_create_kernel_task(void (*entry_point)(void)) {
  task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
  if (!new_task)
    return NULL;
  memset(new_task, 0, sizeof(task_t));

  new_task->id = next_pid++;
  new_task->state = TASK_STATE_READY;


  bool succeed = task_context_init(new_task, entry_point);
  if (!succeed) {
    return NULL;
  }

  return new_task;
}
