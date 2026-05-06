#ifndef ARACHNYAA_KERNEL_CAP_H_
#define ARACHNYAA_KERNEL_CAP_H_

#include <kernel/spinlock.h>
#include <kernel/revnode.h>
#include <stdatomic.h>
#include <stdint.h>
#include <kernel/kobj.h>

typedef int32_t pid_t;

/*
 * Capability handle.
 * structure:
 * bits [0-31]  : cap table index
 * bits [32-55] : generation counter
 * bits [56-63] : type tag
*/
typedef uint64_t cap_handle_t;

struct process;



enum { CAPF_HAS_RANGE = 1u << 0 };

typedef struct {
  uint32_t bits;
  uint32_t flags;
  uint64_t off;
  uint64_t len;
} cap_rights_t;

typedef struct {
  kobj_t *obj;
  cap_rights_t rights;
  revnode_t *rnode;
  uint32_t gen;
  uint8_t type;
  uint8_t _pad[3];
} cap_entry_t;

typedef struct {
  spinlock_t lock;
  cap_entry_t *slots;
  uint32_t cap_count;
  uint32_t free_head;
} cap_table_t;

/*
 * Type Specific Rights
 */

// VM_OBJ
#define R_VM_READ (1u << 0)
#define R_VM_WRITE (1u << 1)
#define R_VM_EXEC (1u << 2)
#define R_VM_MAP (1u << 3)
#define R_VM_DERIVE (1u << 4)
#define R_VM_SHARE (1u << 5)

// ENDPOINT
#define R_EP_BIND (1u << 0)
#define R_EP_CONNECT (1u << 1)
#define R_EP_CALL (1u << 2)
#define R_EP_REPLY (1u << 3)
#define R_EP_TRANSFER (1u << 4)

// ADDRESS_SPACE
#define R_AS_MAP (1u << 0)
#define R_AS_UNMAP (1u << 1)

// TASK/THREAD
#define R_TASK_SIGNAL (1u << 0)
#define R_TASK_DEBUG (1u << 1)
#define R_THREAD_SUSP (1u << 0)
#define R_THREAD_RESUM (1u << 0)

// TUNNEL
#define R_TN_TX (1u << 0)
#define R_TN_RX (1u << 1)
#define R_TN_CONF (1u << 2)

// IOSTREAMS
#define R_IO_READ (1u << 0)
#define R_IO_WRITE (1u << 1)


/*
 * struct for cap syscall 
 */
typedef struct {
  cap_handle_t handle;
  uint32_t rights_bits;
  uint64_t off;
  uint64_t len;
  uint32_t flags;
} cap_sys_arg_t;



cap_handle_t sys_cap_derive(cap_sys_arg_t* arg);

int sys_cap_revoke(cap_handle_t h);

int sys_cap_transfer(pid_t dst_pid, cap_sys_arg_t* arg);

cap_handle_t sys_cap_dup(cap_handle_t h, uint32_t rights_bits);

const cap_entry_t *cap_resolve(struct process *p, cap_handle_t h, uint32_t rights); 

/**
 * @brief Close (drop one refcnt);
 *
 * @param[in] h Cap handle.
 * @return zero if success. Non-zero otherwise.
 */
int sys_cap_close(cap_handle_t h);

/**
 * @brief Initialize cap table.
 *
 * @param[in] ct Pointer to cap table.
 * @param[in] capacity capacity.
 */
void cap_table_init(cap_table_t *ct, uint32_t capacity);

/**
 * @brief Install root cap to process.
 *
 * @param[in] p Pointer to process struct.
 * @param[in] obj Pointer to kernel obj.
 * @param[in] rights caps rights.
 * @return 64-bits cap handle
 */
cap_handle_t kcap_install_root(struct process *p, kobj_t *obj, cap_rights_t rights);



#endif // ARACHNYAA_CAP_CAP_C_
