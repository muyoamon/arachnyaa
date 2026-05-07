#ifndef ARACHNYAA_SYS_PROC_H_
#define ARACHNYAA_SYS_PROC_H_

#include "kernel/cap.h"
#include <stdint.h>

typedef struct {
  cap_handle_t vspace;
  uintptr_t entry;
  uintptr_t user_sp;
  cap_handle_t handle_table;
  cap_handle_t endpoint;
  uint32_t flags;
  uint32_t priority;
  const char *argv0;
  /* optional, flag-defined */
  union {
    const char *module_name;
    void *payload;
  };
} sys_proc_arg_t;

typedef enum {
  /* Boot Module flag: only module_name, flags, and argv0 is matter in this mode */
  SYS_PROG_F_BOOTMODULE = 1 << 13,
} sys_proc_flag_t;

/**
 * @brief Syscall for spawning process.
 *
 * @param[in] args args.
 * @param[out] pid output pid.
 * @param[out] cap output cap.
 * @return 0 if success, non-zero otherwise.
 */
int sys_proc_spawn(sys_proc_arg_t *args, pid_t *pid,
                   cap_handle_t *cap);

#endif // ARACHNYAA_SYS_PROC_H_
