#ifndef ARACHNYAA_PROCESS_TASK_H_
#define ARACHNYAA_PROCESS_TASK_H_

#include "arch/registers.h"
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

#endif // ARACHNYAA_PROCESS_TASK_H_
