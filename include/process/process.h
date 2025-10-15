#ifndef ARACHNYAA_PROCESS_TASK_H_
#define ARACHNYAA_PROCESS_TASK_H_

#include "arch/registers.h"
#include "kernel/mm.h"
#include "process/thread.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  PROCESS_STATE_RUNNING,
  PROCESS_STATE_READY,
  PROCESS_STATE_BLOCKED,
  PROCESS_STATE_ZOMBIE,
} process_state_t;

typedef struct process {
  int32_t pid, ppid;
  process_state_t state;
  mm_t *mm;
  int exit_code;
  thread_t  *main;
  struct process* next;
} process_t;  

/*
 * @brief create kernel process
 * @param entry_point entry point of the process
 * @return return allocated process_t
 */
process_t* process_create_kernel_process(void(*entry_point)(void));

void process_schedule(void);

void process_init(void);

bool process_context_init(process_t *task, void(*entry_point)(void));



#endif // ARACHNYAA_PROCESS_TASK_H_
