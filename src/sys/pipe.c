#include "sys/pipe.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "sys/cap.h"
#include "kernel/pipe.h"
#include "kernel/protocol.h"
#include "process/process.h"
#include "process/scheduler.h"

int sys_pipe(sys_pipe_result_t *out) {
  if (!out) return -KERR_INVAL;

  process_t *p = scheduler_get_current()->proc;

  kobj_t *robj = NULL;
  kobj_t *wobj = NULL;
  int err = pipe_create(&robj, &wobj);
  if (err) return -err;

  cap_rights_t rr = { .bits = KOP_READ  | KOP_CLOSE };
  cap_rights_t wr = { .bits = KOP_WRITE | KOP_CLOSE };

  cap_handle_t rh = kcap_install_root(p, robj, rr);
  cap_handle_t wh = kcap_install_root(p, wobj, wr);

  kobj_put(robj);
  kobj_put(wobj);

  if (!rh || !wh) {
    if (rh) sys_cap_close(rh);
    if (wh) sys_cap_close(wh);
    return -KERR_NOMEM;
  }

  out->read_cap  = rh;
  out->write_cap = wh;
  return 0;
}
