#ifndef PROCD_PROC_ARGS_H_
#define PROCD_PROC_ARGS_H_

#include <stdint.h>

/*
 * Process Args Block (PAB) protocol — enforced by procd / elfloader.
 *
 * At process entry, the initial stack holds:
 *
 *   HIGH (USER_STACK_TOP)
 *     [ string data: argv[0]\0 argv[1]\0 ... envp[0]\0 ... ]
 *     [ proc_auxv_t { AT_CAPS_BASE, <slot> } ]
 *     [ proc_auxv_t { AT_CAPS_COUNT, <n>   } ]
 *     [ proc_auxv_t { AT_NULL, 0           } ]
 *     [ char *envp[M], ..., NULL ]
 *     [ char *argv[argc], ..., NULL ]
 *     [ int argc ]    ← esp on entry
 *   LOW (USER_STACK_BASE)
 *
 * stdio caps (stdin=fd0, stdout=fd1, stderr=fd2) are installed into the
 * child's cap table at consecutive slots starting at AT_CAPS_BASE by
 * sys_spawn's cap transfer.  crt0 reads AT_CAPS_BASE from the auxv and
 * calls sys_cap_get(base + i) to populate the fd table.
 *
 * The exec message data field (shell → vfs → elfloader) uses proc_exec_args_t
 * to carry argv and envp strings across IPC hops.
 */

/* ---- auxv ---- */

typedef struct {
  uint32_t type;
  uint32_t value;
} proc_auxv_t;

#define AT_NULL       0u  /* end-of-auxv sentinel */
#define AT_CAPS_BASE  1u  /* cap table slot of first stdio cap (stdin) */
#define AT_CAPS_COUNT 2u  /* number of stdio caps (normally 3) */

/* ---- exec message wire format ---- */

/*
 * Packed into sys_ipc_msg_t.data[] at each exec hop.
 * Total size must be ≤ 256 bytes: sizeof header (8) + argv_bytes + envp_bytes.
 *
 * argv blob: argv[0]\0 argv[1]\0 ... argv[argc-1]\0  (argv_bytes total)
 * envp blob: KEY=val\0 ...                            (envp_bytes total, may be 0)
 */
typedef struct {
  uint32_t argv_bytes;
  uint32_t envp_bytes;
  uint8_t  blobs[248]; /* 256 - 8 byte header */
} proc_exec_args_t;

#define PROC_EXEC_ARGS_HDR_SIZE  8u   /* sizeof argv_bytes + envp_bytes */
#define PROC_EXEC_ARGS_BLOB_MAX  248u /* maximum combined argv+envp bytes */

#endif /* PROCD_PROC_ARGS_H_ */
