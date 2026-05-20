#ifndef ARACHNYAA_KERNEL_CAP_H_
#define ARACHNYAA_KERNEL_CAP_H_

#include <kernel/spinlock.h>
#include <kernel/revnode.h>
#include <stdatomic.h>
#include <stdint.h>
#include <kernel/kobj.h>
#include <kernel/protocol.h>

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

#define PROCESS_CAP_TABLE_CAPACITY 64
#define CAP_RIGHT_BIND_PROTOCOL (1u << 31)

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

// PROC
#define R_PROC_SIGNAL (1u << 0)
#define R_PROC_DEBUG (1u << 1)
#define R_PROC_CTRL (1u << 2)       // control rights: kill, suspend, resume
#define R_PROC_INSP (1u << 3)       // inspect rights: read status, resource usage
#define R_PROC_WAIT (1u << 4)
#define R_PROC_TRANSFER (1u << 5)

// THREAD
#define R_THREAD_SUSP (1u << 0)
#define R_THREAD_RESUM (1u << 0)

// TUNNEL
#define R_TN_TX (1u << 0)
#define R_TN_RX (1u << 1)
#define R_TN_CONF (1u << 2)

// IOSTREAMS
#define R_IO_READ (1u << 0)
#define R_IO_WRITE (1u << 1)

const cap_entry_t *cap_resolve(struct process *p, cap_handle_t h, uint32_t rights); 

void cap_table_destroy(cap_table_t *ct);

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

int kcap_transfer(struct process *src, struct process *dst,
                  cap_handle_t handle, uint32_t rights_bits);



#endif // ARACHNYAA_CAP_CAP_C_
