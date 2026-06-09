#ifndef ARACHNYAA_SYS_PROC_H_
#define ARACHNYAA_SYS_PROC_H_

#include "kernel/cap.h"
#include <lib/stddef.h>
#include <stdint.h>

typedef struct {
  cap_handle_t vspace;
  uintptr_t entry;
  uintptr_t user_sp;
  cap_handle_t handle_table;
  cap_handle_t endpoint;
  cap_handle_t stdio[3];  /* [0]=stdin [1]=stdout [2]=stderr */
  uint32_t flags;
  uint32_t priority;
  const char *argv0;
  /* optional, flag-defined */
  union {
    const char *module_name;
    struct {
      void *payload;
      size_t payload_size;
    };
  };
} sys_proc_arg_t;

typedef enum {
  /* Boot Module flag: only module_name, flags, and argv0 matter */
  SYS_PROG_F_BOOTMODULE = 1 << 13,
  /* User-memory ELF flag: payload points to ELF bytes in caller's address space */
  SYS_PROG_F_USERMEM = 1 << 14,
} sys_proc_flag_t;

/**
 * @brief Syscall for spawning process.
 *
 * @param[in] args args.
 * @param[out] pid output pid.
 * @param[out] cap output cap.
 * @return 0 if success, non-zero otherwise.
 */
int sys_proc_spawn(sys_proc_arg_t *args, pid_t *pid, cap_handle_t *cap);

/**
 * @brief Block caller until process exits, return exit code.
 *
 * @param[in] proc_cap Cap handle with R_PROC_WAIT.
 * @return exit code if success, negative kerror_t otherwise.
 */
int sys_proc_wait(cap_handle_t proc_cap);

#endif // ARACHNYAA_SYS_PROC_H_
