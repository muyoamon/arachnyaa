#include "sys/ep.h"
#include "kernel/cap.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "process/scheduler.h"

static void ep_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  ipc_endpoint_destroy((kobj_endpoint_t *)obj->payload);
  kfree(obj->payload);
  obj->payload = NULL;
}

static const kobj_ops_t ep_ops = { .release = ep_release };

cap_handle_t sys_ep_create(void) {
  process_t *proc = scheduler_get_current()->proc;

  kobj_t *obj = kobj_create();
  if (!obj) return 0;

  kobj_endpoint_t *payload = kzalloc(sizeof(*payload));
  if (!payload) {
    kobj_put(obj);
    return 0;
  }

  if (ipc_endpoint_init(payload, proc) != KERR_OK) {
    kfree(payload);
    kobj_put(obj);
    return 0;
  }

  obj->type = KOBJ_ENDPOINT;
  obj->supported_ops = KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE | KOP_EXEC;
  obj->ops = &ep_ops;
  obj->payload = payload;

  cap_rights_t rights = {
    .bits = CAP_RIGHT_BIND_PROTOCOL | R_EP_BIND | R_EP_CALL | R_EP_REPLY | R_EP_TRANSFER,
  };
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  kobj_put(obj);
  return h;
}
