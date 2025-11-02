#ifndef ARACHNYAA_PROCESS_TASK_H_
#define ARACHNYAA_PROCESS_TASK_H_

#include "kernel/elf_loader.h"
#include "kernel/mm.h"
#include "process/thread.h"
#include <stdbool.h>
#include <stdint.h>

typedef int32_t pid_t;


typedef struct process {
  pid_t pid;
  as_t *mm;
  int exit_code;
  thread_t  *main;
  struct process* next;
} process_t;  


/**
 * @brief Allocate an empty process.
 *
 * @return allocated process.
 */
process_t* process_alloc(void);

/**
 * @brief Free process.
 *
 * @param[in] proc pointer to process.
 */
void process_free(process_t* proc);



/*
 * @brief create kernel process
 * @param entry_point entry point of the process
 * @return return allocated process_t
 */
process_t* process_create_kernel_process(void(*entry_point)(void*));

process_t* process_spawn_from_elf(const elf_image_t *img, const char *argv0);






#endif // ARACHNYAA_PROCESS_TASK_H_
