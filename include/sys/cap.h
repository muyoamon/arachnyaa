#ifndef ARACHNYAA_SYS_CAP_H_
#define ARACHNYAA_SYS_CAP_H_


/*
 * struct for cap syscall 
 */
#include "kernel/cap.h"

typedef struct {
  cap_handle_t handle;
  uint32_t rights_bits;
  uint64_t off;
  uint64_t len;
  uint32_t flags;
} cap_sys_arg_t;

cap_handle_t sys_cap_derive(cap_sys_arg_t* arg);

int sys_cap_revoke(cap_handle_t h);

int sys_cap_transfer(cap_handle_t dst, cap_sys_arg_t* arg);

cap_handle_t sys_cap_dup(cap_handle_t h, uint32_t rights_bits);

cap_handle_t sys_cap_restrict(cap_handle_t h, uint32_t new_bits);

/**
 * @brief Close (drop one refcnt);
 *
 * @param[in] h Cap handle.
 * @return zero if success. Non-zero otherwise.
 */
int sys_cap_close(cap_handle_t h);

/**
 * @brief Return a handle to the capability at the given slot in the calling
 *        process's cap table (installed by the kernel before first reschedule).
 *
 * @param slot  Cap table slot index (0 = server endpoint, 1 = boot manifest, …)
 * @return cap handle, or 0 if slot is empty or out of range.
 */
cap_handle_t sys_bootstrap_cap(uint32_t slot);


#endif // ARACHNYAA_SYS_CAP_H_
