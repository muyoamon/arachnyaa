#include "sys/namespace.h"

#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/namespace.h"
#include "kernel/protocol.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "lib/string.h"

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
  const cap_entry_t *entry = cap_resolve(proc, handler_handle,
                                         CAP_RIGHT_BIND_PROTOCOL);
  if (!entry) {
    return KERR_ACCESS;
  }
  if (entry->type != KOBJ_ENDPOINT) {
    return KERR_PERM;
  }

  uint32_t allowed_ops = entry->rights.bits & entry->obj->supported_ops;
  if ((declared_ops & allowed_ops) != declared_ops || declared_ops == 0) {
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
  if (!binding->handler || !binding->handler->ops || !binding->handler->ops->open) {
    return 0;
  }

  kobj_open_result_t result = {0};
  err = binding->handler->ops->open(binding->handler, proc, path, flags, &result);
  if (err || !result.obj) {
    return 0;
  }

  cap_rights_t rights = {
      .bits = result.rights,
      .flags = 0,
      .off = 0,
      .len = 0,
  };
  cap_handle_t handle = kcap_install_root(proc, result.obj, rights);
  kobj_put(result.obj);
  return handle;
}
