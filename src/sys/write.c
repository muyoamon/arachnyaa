#include "sys/write.h"
#include "drivers/tty.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "lib/string.h"

static kobj_t *_cap_resolve(process_t *p, cap_handle_t handle, uint32_t rights) {
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
  
  kobj_t *obj = _cap_resolve(p, handle, KOP_WRITE);

  if (!obj) return -KERR_INVAL;
  if (!obj->ops || !obj->ops->write || !(obj->supported_ops & KOP_WRITE)) {
    return -KERR_UNSUPPORTED;
  }

  size_t written = 0;
  int err = obj->ops->write(obj, user_buf, len, &written);
  if (err) {
    return -err;
  }
  return (int)written;
}

