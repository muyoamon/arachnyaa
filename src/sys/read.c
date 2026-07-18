#include "sys/read.h"
#include "drivers/tty.h"
#include "kernel/error.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/pipe.h"
#include "kernel/protocol.h"
#include "process/process.h"
#include "process/scheduler.h"
#include <lib/string.h>

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

int sys_read(cap_handle_t handle, void *buf, size_t nbytes) {
  process_t *p = scheduler_get_current()->proc;
  if (buf == NULL || nbytes == 0) {
    return -KERR_INVAL;
  }

  kobj_t *obj = _cap_resolve(p, handle, KOP_READ);

  if (!obj) {
    tty_writestring("unable to resolve kobj");
    return -KERR_INVAL;
  }

  if (obj->type == KOBJ_PIPE)
    return pipe_read((pipe_buf_t *)obj->payload, buf, (uint32_t)nbytes);

  if (obj->type != KOBJ_REMOTE) {
    return -KERR_UNSUPPORTED;
  }

  kobj_remote_t *remote = (kobj_remote_t *)obj->payload;

  if (!remote || !remote->endpoint) {
    tty_writestring("unable to resolve remote");
    return -KERR_INVAL;
  }

  ipc_kmsg_t req = {0};
  ipc_kmsg_t reply = {0};

  req.opcode    = IPC_OP_READ;
  req.num_bytes = (uint32_t)sizeof(nbytes);

  memcpy(req.data, &nbytes, sizeof(nbytes));

  kerror_t err = ipc_call(remote->endpoint, scheduler_get_current(),
      remote->server_object_id, &req, &reply);

  if (err) {
    tty_writestring("Read Error code is: ");
    tty_write_dec(err);
    tty_putc('\n');
    return -err;
  }
  
  if (reply.num_handles != 0) {
    return -KERR_IO;
  }

  if (reply.num_bytes > nbytes) {
    // malform logic, error instead of coercing
    return -KERR_IO;
  }

  memcpy(buf, reply.data, reply.num_bytes);
  
  return (int)(reply.num_bytes);
}
