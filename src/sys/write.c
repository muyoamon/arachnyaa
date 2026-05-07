#include "sys/write.h"
#include "drivers/tty.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "lib/string.h"
#include "process/process.h"
#include "process/scheduler.h"
#include <stdint.h>

static kobj_t *_cap_resolve(process_t *p, cap_handle_t handle,
                            uint32_t rights) {
  if (!handle || !p) {
    return NULL;
  }
  const cap_entry_t *entry = cap_resolve(p, handle, rights);

  if (!entry) {
    return NULL;
  }

  return entry->obj;
}

int sys_write(cap_handle_t handle, const void *user_buf, size_t len) {
  process_t *p = scheduler_get_current()->proc;
  if (!user_buf && len != 0) {
    return -KERR_INVAL;
  }

  kobj_t *obj = _cap_resolve(p, handle, KOP_WRITE);

  if (!obj)
    return -KERR_INVAL;

  if (obj->type != KOBJ_REMOTE)
    return -KERR_UNSUPPORTED;

  kobj_remote_t *remote = (kobj_remote_t *)obj->payload;

  if (!remote || !remote->endpoint) {
    return -KERR_INVAL;
  }

  ipc_kmsg_t req = {0};
  ipc_kmsg_t reply = {0};

  req.opcode = IPC_OP_WRITE;
  /* v1: write payload is limited to IPC_INLINE_BYTES. */
  req.num_bytes = len > IPC_INLINE_BYTES ? IPC_INLINE_BYTES : (uint32_t)len;
  memcpy(req.data, user_buf, req.num_bytes);

  kerror_t err = ipc_call(remote->endpoint, scheduler_get_current(),
                          remote->server_object_id, &req, &reply);

  if (err) {
    return -err;
  }

  if (reply.num_handles != 0) {
    return -KERR_IO;
  }
  if (reply.num_bytes != sizeof(uint32_t)) {
    return -KERR_IO;
  }

  return (int)(*(uint32_t *)reply.data);
}
