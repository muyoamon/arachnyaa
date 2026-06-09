#ifndef ARACHNYAA_PROCESS_TASK_H_
#define ARACHNYAA_PROCESS_TASK_H_

#include "kernel/cap.h"
#include "kernel/elf_loader.h"
#include "kernel/mm.h"
#include "kernel/namespace.h"
#include "process/thread.h"
#include <stdbool.h>
#include <stdint.h>

typedef int32_t pid_t;

typedef struct process {
  pid_t pid;
  as_t *mm;
  int exit_code;
  thread_t *main;
  cap_table_t caps;
  process_namespace_t ns;

  struct thread *waiting_thread;

  struct ipc_call *saved_calls[4];

  struct process *next;
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

/**
 * @brief Spawn a process in an existing address space.
 *
 * @param[in] as  Address space to run in (refcount is bumped).
 * @param[in] entry   Entry-point virtual address.
 * @param[in] user_sp Initial user stack pointer.
 * @param[in] argv0   Process name (informational).
 * @return Allocated process, or NULL on failure.
 */
process_t *process_spawn_from_vspace(as_t *as, uintptr_t entry,
                                     uintptr_t user_sp, const char *argv0);


/**
 * @brief Get list of processes.
 *
 * @return list of processes.
 */
process_t* process_get_all(void);

#ifndef FOR_EACH_PROC
#define FOR_EACH_PROC(p) for (process_t *p=process_get_all(); p!=NULL; p=p->next)
#endif // !FOR_EACH_PROC(p)



#endif // ARACHNYAA_PROCESS_PROCESS_H_
