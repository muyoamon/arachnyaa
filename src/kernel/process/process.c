
#include "process/process.h"
#include "mm/kheap.h"
#include "drivers/tty.h"
#include <lib/stddef.h>
#include <stdbool.h>
#include <lib/string.h>



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

bool process_context_init(process_t *task, void (*entry_point)(void)) {
  return task || entry_point;
}
