/*
 * procd — process manager service
 *
 * Protocol: proc:
 * Endpoint: BOOTSTRAP_SERVER_EP (cap slot 0, installed by kernel before first reschedule)
 *
 * IPC protocol:
 *   IPC_OP_OPEN "proc:spawn"
 *     → object_id=SPAWN_OID, allowed_ops=KOP_CALL
 *
 *   IPC_OP_CALL on SPAWN_OID
 *     req.handles[0]  = vspace_cap  (kernel transfers from caller)
 *     req.handles[1]  = stdin cap   (optional)
 *     req.handles[2]  = stdout cap  (optional)
 *     req.handles[3]  = stderr cap  (optional)
 *     req.data[0..3]  = entry_point (uint32_t)
 *     req.data[4..7]  = user_sp     (uint32_t)
 *     req.data[8+]    = argv0       (null-terminated, optional)
 *     → reply.handles[0] = watch_cap (KOBJ_PROC with R_PROC_WAIT)
 *       caller may call sys_proc_wait(watch_cap) directly
 */

#include <stddef.h>
#include <stdint.h>
#include "syscall.h"
#include "string.h"

/* sys_bootstrap_cap(0) gives us the endpoint with full bind/call rights. */
#define BOOTSTRAP_SERVER_EP \
  (((uint64_t)KOBJ_ENDPOINT << 56) | ((uint64_t)1 << 32) | 0u)

#define SPAWN_OID  1u

static void reply_empty(void) {
  sys_ipc_msg_t r;
  memset(&r, 0, sizeof(r));
  sys_reply(&r);
}

static void handle_open(const sys_ipc_msg_t *req) {
  /* Only "proc:spawn" is supported. */
  const char *data = (const char *)req->data;
  size_t len = req->num_bytes;

  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  /* path in data is "proc:<name>" — check for "proc:spawn" */
  static const char spawn_path[] = "proc:spawn";
  if (len == sizeof(spawn_path) - 1 &&
      memcmp(data, spawn_path, len) == 0) {
    sys_open_reply_t oreply = { .allowed_ops = KOP_CALL };
    reply.object_id = SPAWN_OID;
    reply.num_bytes = sizeof(oreply);
    memcpy(reply.data, &oreply, sizeof(oreply));
  }
  /* else: unknown path → object_id=0, empty reply */

  sys_reply(&reply);
}

static void handle_spawn_call(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  if (req->num_bytes < 8) {
    sys_reply(&reply);
    return;
  }

  uint32_t entry_point, user_sp;
  memcpy(&entry_point, req->data + 0, sizeof(uint32_t));
  memcpy(&user_sp,     req->data + 4, sizeof(uint32_t));
  const char *argv0 = (req->num_bytes > 8) ? (const char *)(req->data + 8) : "proc";

  cap_handle_t vspace_cap = req->handles[0];
  if (vspace_cap == 0) {
    sys_reply(&reply);
    return;
  }

  sys_proc_arg_t arg;
  memset(&arg, 0, sizeof(arg));
  arg.flags    = SYS_PROG_F_VSPACE;
  arg.vspace   = vspace_cap;
  arg.entry    = entry_point;
  arg.user_sp  = user_sp;
  arg.argv0    = argv0;
  arg.stdio[0] = req->handles[1];
  arg.stdio[1] = req->handles[2];
  arg.stdio[2] = req->handles[3];

  cap_handle_t proc_cap = 0;
  int err = sys_spawn(&arg, NULL, &proc_cap);
  if (err || proc_cap == 0) {
    sys_reply(&reply);
    return;
  }

  /* Restrict to wait-only before handing to caller. */
  cap_handle_t watch_cap = sys_cap_restrict(proc_cap, R_PROC_WAIT);
  sys_cap_close(proc_cap);

  reply.num_handles = 1;
  reply.handles[0]  = watch_cap;
  sys_reply(&reply);

  /* watch_cap was transferred to caller by sys_reply; close local copy. */
  sys_cap_close(watch_cap);
}

void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:
      handle_open(&req);
      break;
    case IPC_OP_EXEC:
      if (req.object_id == SPAWN_OID)
        handle_spawn_call(&req);
      else
        reply_empty();
      break;
    default:
      reply_empty();
      break;
    }
  }
}
