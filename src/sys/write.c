#include "sys/write.h"
#include "drivers/tty.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "lib/string.h"

static kobj_t *_cap_resolve(process_t *p, cap_handle_t handle, uint32_t rights) {
  if (!handle | !p) {
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
  
  kobj_t *obj = _cap_resolve(p, handle, R_IO_WRITE);

  if (!obj) return -KERR_INVAL;

  switch (obj->type) {
    case KOBJ_LOGSINK:
      {
        char *str = kmalloc(len + 1);
        strcpy(str, user_buf);
        str[len] = '\0';
        tty_writestring(str);
        kfree(str);
        return len;
      }
    default:
      return -KERR_INVAL;
  }
  
}


