#ifndef ARACHNYAA_PROCESS_TASK_H_
#define ARACHNYAA_PROCESS_TASK_H_

#include "arch/registers.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  TASK_STATE_RUNNING,
  TASK_STATE_READY,
  TASK_STATE_BLOCKED,
  TASK_STATE_ZOMBIE,
} task_state_t;

typedef struct task {
  int32_t id;
  task_state_t state;
  registers_t *registers;

  uintptr_t kernel_stack_top;
  uintptr_t kernel_stack_bottom;

  uintptr_t page_directory_phys;

  struct task* next;
} task_t;

/*
 * @brief create kernel task
 * @param entry_point entry point of the task
 * @return return allocated task_t
 */
task_t* task_create_kernel_task(void(*entry_point)(void));


void task_schedule(void);

void task_init(void);

bool task_context_init(task_t *task, void(*entry_point)(void));


#endif // ARACHNYAA_PROCESS_TASK_H_
