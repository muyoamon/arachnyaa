#include "sys/namespace.h"

#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/namespace.h"
#include "kernel/protocol.h"
#include "lib/string.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "uapi/syscalls.h"
#include <stdint.h>

static int parse_resource_name(const char *name, char *protocol,
                               const char **path_out) {
  size_t idx = 0;
  if (!name || !protocol || !path_out) {
    return KERR_INVAL;
  }

  while (name[idx] != '\0' && name[idx] != ':') {
    if (idx + 1 >= PROCESS_PROTOCOL_NAME_MAX) {
      return KERR_NOSPACE;
    }
    protocol[idx] = name[idx];
    idx++;
  }

  if (idx == 0 || name[idx] != ':') {
    return KERR_INVAL;
  }

  protocol[idx] = '\0';
  *path_out = &name[idx + 1];
  return 0;
}

int sys_ns_bind(const char *protocol, cap_handle_t handler_handle,
                uint32_t declared_ops) {
  process_t *proc = scheduler_get_current()->proc;
  const cap_entry_t *entry =
      cap_resolve(proc, handler_handle, CAP_RIGHT_BIND_PROTOCOL | R_EP_BIND);
  if (!entry) {
    return KERR_ACCESS;
  }
  if (entry->type != KOBJ_ENDPOINT) {
    return KERR_PERM;
  }

  uint32_t allowed_ops = entry->obj->supported_ops;
  if ((declared_ops & allowed_ops) != declared_ops ||
      !(declared_ops & KOP_OPEN)) {
    return KERR_ACCESS;
  }

  return process_namespace_bind(&proc->ns, protocol, entry->obj, declared_ops);
}

cap_handle_t sys_open(const char *name, uint32_t flags) {
  process_t *proc = scheduler_get_current()->proc;
  char protocol[PROCESS_PROTOCOL_NAME_MAX];
  const char *path = NULL;
  int err = parse_resource_name(name, protocol, &path);
  if (err) {
    return 0;
  }

  const ns_entry_t *binding = process_namespace_lookup(&proc->ns, protocol);
  if (!binding) {
    return 0;
  }
  if (!(binding->declared_ops & KOP_OPEN)) {
    return 0;
  }
  if (!binding->handler || binding->handler->type != KOBJ_ENDPOINT) {
    return 0;
  }

  ipc_kmsg_t req = {0};
  ipc_kmsg_t reply = {0};

  req.opcode = IPC_OP_OPEN;
  req.flags = flags;

  size_t path_len = strlen(path);
  if (path_len > IPC_INLINE_BYTES) {
    path_len = IPC_INLINE_BYTES;
  }
  req.num_bytes = (uint32_t)path_len;
  memcpy(req.data, path, path_len);

  err = ipc_call(binding->handler, scheduler_get_current(), 0, &req, &reply);

  if (err) {
    return 0;
  }
  if (reply.num_handles != 0) {
    return 0;
  }
  if (reply.num_bytes != sizeof(sys_open_reply_t)) {
    return 0;
  }

  const sys_open_reply_t *open_reply = (const sys_open_reply_t *)reply.data;

  uint32_t server_ops = open_reply->allowed_ops;
  uint32_t final_ops = server_ops & binding->declared_ops;

  if (reply.object_id == 0) {
    return 0;
  }
  if (final_ops == 0) {
    return 0;
  }

  return ipc_install_remote_handle(proc, binding->handler, reply.object_id,
                                   final_ops);
}

static kobj_t *_cap_resolve(process_t *p, cap_handle_t handle,
                            uint32_t rights) {
  const cap_entry_t *e = cap_resolve(p, handle, rights);

  if (!e) {
    return NULL;
  }

  return e->obj;
}

int sys_close(cap_handle_t handle) {
  process_t *proc = scheduler_get_current()->proc;

  kobj_t *obj = _cap_resolve(proc, handle, KOP_CLOSE);

  if (!obj) {
    return -KERR_INVAL;
  }

  if (obj->type != KOBJ_REMOTE)
    return -KERR_UNSUPPORTED;

  kobj_remote_t *ep = obj->payload;

  if (!ep || !ep->endpoint) {
    return -KERR_INVAL;
  }

  ipc_kmsg_t req = {0};
  ipc_kmsg_t reply = {0};

  req.opcode = IPC_OP_CLOSE;

  // implementation defined close action
  kerror_t err = ipc_call(ep->endpoint, scheduler_get_current(),
                          ep->server_object_id, &req, &reply);

  if (err) {
    return -err;
  }

  if (reply.num_bytes != 0 || reply.num_handles != 0) {
    return -KERR_IO;
  }

  // close the cap handle
  err = sys_cap_close(handle);

  if (err)
    return -err;

  return KERR_OK;
}
